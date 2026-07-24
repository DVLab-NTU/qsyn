/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ `qcir synthesize` -- BQSKit-style instantiation that takes
                 the focused QCir, computes its tensor, and re-synthesises
                 it back into a U3+CNOT circuit using the KAK pipeline.

                 NOTE -- This command is a *logical* (all-to-all)
                 synthesis driver, matching BQSKit's KAK / QSD passes.
                 The connectivity-aware path (coupling map, SABRE-style
                 routing, qubit placement) is intentionally NOT plumbed
                 through here; users that need physical compilation
                 should chain `qcir synthesize` with `device read ...`
                 plus `qcir optimize --physical` or `qcir translate
                 <gate_set>`. A future PR will expose BQSKit's
                 `setmodel.py` workflow once the qsyn `device` integration
                 is ready. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "convert/qcir_to_tensor.hpp"
#include "qcir/qcir.hpp"
#include "synthesis/search/qsearch.hpp"
#include "tensor/decomposer.hpp"
#include "tensor/kak.hpp"
#include "tensor/qsd.hpp"
#include "tensor/qtensor.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::qcir {

Command qcir_synthesize_cmd(QCirMgr& qcir_mgr) {
    return {
        "synthesize",
        [&](ArgumentParser& parser) {
            parser.description(
                "Re-synthesise the focused QCir into a U3+CNOT circuit. "
                "Currently supports 1- and 2-qubit circuits (KAK for 2-qubit). "
                "For circuits with more qubits, use `convert qcir tensor; "
                "convert tensor qcir` until QSD (PR-7) is wired in.");

            parser.add_argument<bool>("-r", "--replace")
                .action(store_true)
                .help("Replace the focused QCir in-place with the synthesised one");
            parser.add_argument<bool>("--three-cnot-kak")
                .action(store_true)
                .help("(Experimental) try the 3-CNOT QFactor-instantiated ansatz "
                      "for 2-qubit blocks; slow with the coordinate-descent "
                      "instantiator (replaced by LBFGS in PR-B).");
            parser.add_argument<int>("--three-cnot-restarts")
                .default_value(1)
                .help("Number of random restarts for the 3-CNOT QFactor ansatz.");
            parser.add_argument<bool>("--three-cnot-lbfgs")
                .action(store_true)
                .help("Use LBFGS (PR-B) instead of coordinate descent for the "
                      "3-CNOT QFactor ansatz. Order of magnitude fewer "
                      "to_tensor evaluations on the 24-parameter ansatz.");
            // ---- PR-C: native QSearch / LEAP ----
            parser.add_argument<bool>("--qsearch-native")
                .action(store_true)
                .help("(PR-C) Use the native QSearch best-first search to "
                      "synthesise the target unitary instead of QSD/KAK. "
                      "Outputs a U3+CX circuit and respects --qsearch-* "
                      "tuning flags below.");
            parser.add_argument<bool>("--leap-native")
                .action(store_true)
                .help("(PR-C) Use the native LEAP (prefix-freeze QSearch) "
                      "synthesiser. Recommended for >=3-qubit targets where "
                      "plain QSearch's frontier blows up.");
            parser.add_argument<int>("--qsearch-max-depth")
                .default_value(6)
                .help("Max number of CX-layers the native QSearch / LEAP "
                      "frontier may add (default 6).");
            parser.add_argument<int>("--qsearch-max-iters")
                .default_value(256)
                .help("Max number of frontier pops in native QSearch / LEAP "
                      "(default 256).");
            parser.add_argument<int>("--qsearch-inst-iters")
                .default_value(60)
                .help("QFactor budget per child instantiation in native "
                      "QSearch / LEAP (default 60).");
        },
        [&](ArgumentParser const& parser) -> CmdExecResult {
            if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;

            auto const& src = *qcir_mgr.get();
            auto const  n   = src.get_num_qubits();

            auto tensor_opt = to_tensor(src);
            if (!tensor_opt.has_value()) {
                spdlog::error("synthesize: failed to compute tensor of QCir {}", qcir_mgr.focused_id());
                return CmdExecResult::error;
            }
            *tensor_opt = tensor_opt->to_matrix();

            // The unified dispatcher; QSD itself routes n=1 -> ZYZ,
            // n=2 -> KAK (+ gray-code fallback) and n>=3 -> recursive QSD
            // (currently scaffolded as gray-code; see qsd.cpp for the
            // pending cosine-sine decomposition follow-up).
            std::optional<QCir> synth;
            bool const use_qsearch = parser.parsed("--qsearch-native");
            bool const use_leap    = parser.parsed("--leap-native");
            if (use_qsearch || use_leap) {
                synthesis::search::QSearchOptions qopt;
                qopt.max_depth         = static_cast<std::size_t>(parser.get<int>("--qsearch-max-depth"));
                qopt.max_iterations    = static_cast<std::size_t>(parser.get<int>("--qsearch-max-iters"));
                qopt.instantiate_iters = static_cast<std::size_t>(parser.get<int>("--qsearch-inst-iters"));
                qopt.minimizer         = parser.parsed("--three-cnot-lbfgs")
                                             ? tensor::opt::MinimizerKind::LBFGS
                                             : tensor::opt::MinimizerKind::LBFGS;  // default LBFGS
                if (use_leap) {
                    synthesis::search::LeapOptions lopt;
                    static_cast<synthesis::search::QSearchOptions&>(lopt) = qopt;
                    synth = synthesis::search::leap_synthesize(*tensor_opt, lopt);
                } else {
                    synth = synthesis::search::qsearch_synthesize(*tensor_opt, qopt);
                }
            } else {
                tensor::qsd::QSDOptions qsd_opt;
                qsd_opt.try_three_cnot_qfactor = parser.parsed("--three-cnot-kak");
                qsd_opt.three_cnot_restarts    = parser.get<int>("--three-cnot-restarts");
                qsd_opt.three_cnot_use_lbfgs   = parser.parsed("--three-cnot-lbfgs");
                synth = tensor::qsd::synthesize(*tensor_opt, qsd_opt);
            }
            spdlog::info("synthesize: produced a {}-qubit circuit ({} qubits target).", n, n);

            if (!synth.has_value()) {
                spdlog::error("synthesize: all decomposition strategies failed.");
                return CmdExecResult::error;
            }

            synth->set_filename(src.get_filename());
            synth->add_procedures(src.get_procedures());
            synth->add_procedure(use_leap ? "Synthesize-LeapNative"
                                          : use_qsearch ? "Synthesize-QSearchNative"
                                                        : "Synthesize");

            if (parser.parsed("--replace")) {
                *qcir_mgr.get() = std::move(*synth);
            } else {
                qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<QCir>(std::move(*synth)));
            }
            return CmdExecResult::done;
        }};
}

}  // namespace qsyn::qcir
