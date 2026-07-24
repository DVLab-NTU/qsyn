/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ `qcir instantiate` -- QFactor-lite numerical polish of the
                 focused QCir's U3 parameters against a target unitary.

                 Two driving modes:

                   1. `--target <id>` : compute the tensor of QCir <id>
                      and tune the focused QCir's U3s to match it.  Use
                      this after a lossy / approximate compile step (KAK
                      fallback to gray-code, CPF reconstruction, ...).

                   2. `--self`        : use the focused QCir's *own*
                      tensor as the target.  This is useful as a
                      regression test: a well-formed circuit should
                      already satisfy the target up to tolerance.

                 Defaults to `--self` when neither option is given.

                 NOTE -- Like BQSKit's QFactor pass, `qcir instantiate`
                 ignores connectivity. The ansatz topology is whatever
                 the caller built; we only refine the U3 parameters.
                 Use `qcir optimize --physical` afterwards for routing. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "convert/qcir_to_tensor.hpp"
#include "qcir/qcir.hpp"
#include "tensor/qfactor.hpp"
#include "tensor/qtensor.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::qcir {

Command qcir_instantiate_cmd(QCirMgr& qcir_mgr) {
    return {
        "instantiate",
        [&](ArgumentParser& parser) {
            parser.description(
                "QFactor-lite numerical instantiation: tune every UGate parameter "
                "in the focused QCir so its tensor matches a target unitary. "
                "Use after `qcir synthesize` to polish a fallback decomposition.");

            auto src = parser.add_mutually_exclusive_group().required(false);
            src.add_argument<size_t>("-t", "--target")
                .help(
                    "ID of the QCir whose tensor is used as the target. "
                    "Defaults to comparing the focused circuit against itself.");
            src.add_argument<bool>("--self")
                .action(store_true)
                .help("Use the focused QCir's own tensor as the target (default).");

            parser.add_argument<size_t>("--max-iter")
                .default_value(static_cast<size_t>(200))
                .help("Maximum coordinate-descent passes.");
            parser.add_argument<double>("--tol")
                .default_value(1e-8)
                .help("Residual tolerance; iteration stops once `1 - cosine_similarity` is below this.");
            parser.add_argument<double>("--init-step")
                .default_value(0.4)
                .help("Initial probe step in radians (halved on stagnation).");
            parser.add_argument<bool>("-v", "--verbose")
                .action(store_true)
                .help("Print one log line per pass.");
        },
        [&](ArgumentParser const& parser) -> CmdExecResult {
            if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;

            auto& ansatz = *qcir_mgr.get();

            QCir const* target_qcir = &ansatz;
            if (parser.parsed("--target")) {
                auto const tid    = parser.get<size_t>("--target");
                auto const* maybe = qcir_mgr.find_by_id(tid);
                if (maybe == nullptr) {
                    spdlog::error("instantiate: QCir id {} not found.", tid);
                    return CmdExecResult::error;
                }
                target_qcir = maybe;
            }

            if (target_qcir->get_num_qubits() != ansatz.get_num_qubits()) {
                spdlog::error("instantiate: target ({} qubits) and ansatz ({} qubits) disagree on width.",
                              target_qcir->get_num_qubits(), ansatz.get_num_qubits());
                return CmdExecResult::error;
            }

            auto target_tens = to_tensor(*target_qcir);
            if (!target_tens.has_value()) {
                spdlog::error("instantiate: failed to compute target tensor.");
                return CmdExecResult::error;
            }
            *target_tens = target_tens->to_matrix();

            tensor::qfactor::QFactorOptions opt{
                .max_iterations = parser.get<size_t>("--max-iter"),
                .tolerance      = parser.get<double>("--tol"),
                .initial_step   = parser.get<double>("--init-step"),
                .step_shrink    = 0.5,
                .min_step       = 1e-9,
                .verbosity      = parser.parsed("--verbose") ? size_t{2} : size_t{1},
            };

            auto const result = tensor::qfactor::instantiate(ansatz, *target_tens, opt);

            spdlog::info("instantiate: residual {:.3e} -> {:.3e} in {} passes ({} params, {})",
                         result.initial_residual, result.final_residual,
                         result.n_passes, result.n_parameters,
                         result.converged ? "converged" : "stagnated");

            ansatz.add_procedure("QFactor-Lite");
            return CmdExecResult::done;
        }};
}

}  // namespace qsyn::qcir
