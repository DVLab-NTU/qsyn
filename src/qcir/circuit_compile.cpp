/****************************************************************************
  PackageName  [ qcir / circuit_compile ]
  Synopsis     [ Partitioned U3+CX compile and gate retargeting. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./circuit_compile.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "convert/qcir_to_tensor.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir_equiv.hpp"
#include "qcir/qcir_io.hpp"
#include "qcir/coupling_constraints.hpp"
#include "qcir/quick_partitioner.hpp"
#include "qcir/scanning_gate_removal.hpp"
#include "device/device.hpp"
#include "synthesis/pas/permutation_aware.hpp"
#include "synthesis/search/qsearch.hpp"
#include "tensor/kak.hpp"
#include "tensor/opt/minimizer.hpp"
#include "tensor/qfactor.hpp"
#include "tensor/qsd.hpp"
#include "tensor/tensor.hpp"

namespace qsyn::qcir {

QCir retarget_to_u3_cx(QCir const& src);

bool use_external_u3syn(U3CxCompileOptions const& opt) {
    return opt.use_u3syn || opt.use_bqskit;
}

bool use_external_block_synth(U3CxCompileOptions const& opt) {
    if (use_external_u3syn(opt)) return false;
    auto const e = effective_block_engine(opt);
    // The *Native* engines run inside the qsyn process; only the
    // BQSKit subprocess engines are "external" w.r.t. block synthesis.
    if (e == BlockSynthEngine::Native ||
        e == BlockSynthEngine::QSearchNative ||
        e == BlockSynthEngine::LeapNative) {
        return false;
    }
    return true;
}

BlockSynthEngine effective_block_engine(U3CxCompileOptions const& opt) {
    if (opt.block_engine != BlockSynthEngine::Native) return opt.block_engine;
    if (opt.use_qsearch) return BlockSynthEngine::QSearch;
    if (opt.use_qfast) return BlockSynthEngine::QFast;
    if (opt.use_qpredict) return BlockSynthEngine::QPredict;
    return BlockSynthEngine::Native;
}

namespace {

[[nodiscard]] int effective_opt_level(U3CxCompileOptions const& opt) {
    return opt.optimization_level > 0 ? opt.optimization_level : opt.bqskit_opt_level;
}

[[nodiscard]] char const* block_engine_name(BlockSynthEngine e) {
    switch (e) {
        case BlockSynthEngine::QSearch:       return "qsearch";
        case BlockSynthEngine::Leap:          return "leap";
        case BlockSynthEngine::QFast:         return "qfast";
        case BlockSynthEngine::QPredict:      return "qpredict";
        case BlockSynthEngine::QSearchNative: return "qsearch-native";
        case BlockSynthEngine::LeapNative:    return "leap-native";
        default:                              return "native";
    }
}

[[nodiscard]] bool validate_synthesis_flags(U3CxCompileOptions const& opt) {
    if (use_external_u3syn(opt) &&
        (opt.use_qsearch || opt.use_qfast || opt.use_qpredict)) {
        spdlog::warn("circuit_compile: --qsearch/--qfast/--qpredict ignored with --u3syn "
                     "(full BQSKit compile already includes block synthesis).");
    }
    (void)opt;
    return true;
}

[[nodiscard]] bool should_use_monolithic(QCir const& src, U3CxCompileOptions const& opt) {
    if (opt.force_monolithic) {
        return src.get_num_qubits() <= opt.monolithic_max_qubits;
    }
    if (src.get_num_gates() != 1) {
        return false;
    }
    return src.get_num_qubits() <= opt.monolithic_max_qubits;
}

[[nodiscard]] QCir scanning_gate_removal_lite(QCir const& compiled,
                                              tensor::QTensor<double> const& target_unitary,
                                              U3CxCompileOptions const& opt);

[[nodiscard]] std::optional<QCir> compile_partitioned(QCir const& src, U3CxCompileOptions const& opt);

[[nodiscard]] QCir native_resynthesis(QCir const& compiled, tensor::QTensor<double> const& target,
                                      U3CxCompileOptions const& opt);

[[nodiscard]] QCir apply_native_optimization(QCir const& compiled,
                                             tensor::QTensor<double> const& target,
                                             U3CxCompileOptions const& opt) {
    QCir cur = compiled;
    auto const level = std::clamp(opt.optimization_level, 1, 3);

    if (level >= 1) {
        tensor::qfactor::QFactorOptions qopt;
        qopt.tolerance = opt.synthesis_epsilon;
        qopt.verbosity = 0;
        qopt.strategy  = opt.qfactor_use_lbfgs
                             ? tensor::qfactor::QFactorStrategy::LBFGS
                             : tensor::qfactor::QFactorStrategy::CoordinateDescent;
        if (opt.qfactor_use_lbfgs) {
            // LBFGS converges in fewer outer iterations; cap to a sensible
            // budget so a stuck line search cannot dominate the wallclock.
            qopt.max_iterations = 80;
        }
        auto res = tensor::qfactor::instantiate(cur, target, qopt);
        if (res.n_parameters > 0) {
            spdlog::debug("circuit_compile: qfactor polish residual {:.3e} ({} passes).",
                          res.final_residual, res.n_passes);
        }
    }
    if (level >= 2) {
        cur = scanning_gate_removal_lite(cur, target, opt);
    }
    if (level >= 3) {
        cur = native_resynthesis(cur, target, opt);
    }
    return cur;
}

// BQSKit build_resynthesis_optimization_workflow (lite): re-partition and re-synthesize
// while preserving the compiled unitary; stop when gate count stops decreasing.
[[nodiscard]] QCir native_resynthesis(QCir const& compiled, tensor::QTensor<double> const& target,
                                      U3CxCompileOptions const& opt) {
    QCir cur = compiled;
    constexpr std::size_t k_max_rounds = 3;

    U3CxCompileOptions inner = opt;
    inner.optimization_level = std::min(2, effective_opt_level(opt));

    for (std::size_t round = 0; round < k_max_rounds; ++round) {
        std::size_t const before = cur.get_num_gates();
        auto              next   = compile_partitioned(cur, inner);
        if (!next.has_value()) {
            spdlog::debug("circuit_compile: resynthesis round {} failed.", round);
            break;
        }
        if (!is_equivalent(cur, *next)) {
            spdlog::warn("circuit_compile: resynthesis round {} broke equivalence; keeping prior.",
                         round);
            break;
        }
        if (next->get_num_gates() >= before) {
            spdlog::debug("circuit_compile: resynthesis round {}: gates {} -> {} (stop).", round,
                          before, next->get_num_gates());
            break;
        }
        spdlog::info("circuit_compile: resynthesis round {}: gates {} -> {}.", round, before,
                     next->get_num_gates());
        cur = retarget_to_u3_cx(*next);
        if (inner.optimization_level >= 1) {
            tensor::qfactor::QFactorOptions qopt;
            qopt.tolerance = opt.synthesis_epsilon;
            qopt.verbosity = 0;
            qopt.strategy  = opt.qfactor_use_lbfgs
                                 ? tensor::qfactor::QFactorStrategy::LBFGS
                                 : tensor::qfactor::QFactorStrategy::CoordinateDescent;
            if (opt.qfactor_use_lbfgs) qopt.max_iterations = 80;
            (void)tensor::qfactor::instantiate(cur, target, qopt);
        }
        if (inner.optimization_level >= 2) {
            cur = scanning_gate_removal_lite(cur, target, opt);
        }
    }
    cur.add_procedure("Resynthesis-Lite");
    return cur;
}

[[nodiscard]] std::optional<std::filesystem::path> find_script(char const* name) {
    for (auto const& base : {std::filesystem::current_path(),
                             std::filesystem::current_path().parent_path()}) {
        auto p = base / "scripts" / name;
        if (std::filesystem::exists(p)) return p;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::filesystem::path> find_bqskit_script() {
    return find_script("bqskit_u3cx_compile.py");
}

[[nodiscard]] std::optional<std::filesystem::path> find_bqskit_block_script() {
    return find_script("bqskit_block_synth.py");
}

[[nodiscard]] std::optional<QCir> compile_via_bqskit_block(QCir const& src, BlockSynthEngine engine,
                                                         int opt_level, size_t block_size) {
    auto script = find_bqskit_block_script();
    if (!script.has_value()) return std::nullopt;

    auto const tmp_dir = std::filesystem::temp_directory_path() / "qsyn_bqskit_block";
    std::error_code ec;
    std::filesystem::create_directories(tmp_dir, ec);

    auto const in_path  = tmp_dir / "in.qasm";
    auto const out_path = tmp_dir / "out.qasm";
    if (!src.write_qasm(in_path)) return std::nullopt;

    std::ostringstream cmd;
    cmd << "python3 \"" << script->string() << "\""
        << " --engine " << block_engine_name(engine) << " --opt " << opt_level << " --block-size "
        << block_size << " \"" << in_path.string() << "\" \"" << out_path.string() << "\" 2>&1";
    if (std::system(cmd.str().c_str()) != 0) {
        spdlog::warn("circuit_compile: bqskit_block_synth ({}) failed.", block_engine_name(engine));
        return std::nullopt;
    }

    auto parsed = from_file(out_path);
    if (!parsed.has_value()) return std::nullopt;
    parsed->add_procedure("U3Syn-Block");
    return parsed;
}

[[nodiscard]] std::optional<QCir> compile_via_bqskit(QCir const& src, int opt_level) {
    auto script = find_bqskit_script();
    if (!script.has_value()) {
        spdlog::error("circuit_compile: scripts/bqskit_u3cx_compile.py not found (run qsyn from repo root?).");
        return std::nullopt;
    }

    auto const tmp_dir = std::filesystem::temp_directory_path() / "qsyn_bqskit_u3cx";
    std::error_code ec;
    std::filesystem::create_directories(tmp_dir, ec);

    auto const in_path  = tmp_dir / "in.qasm";
    auto const out_path = tmp_dir / "out.qasm";
    if (!src.write_qasm(in_path)) {
        spdlog::error("circuit_compile: failed to write temp QASM for BQSKit.");
        return std::nullopt;
    }

    std::ostringstream cmd;
    cmd << "python3 \"" << script->string() << "\""
        << " --opt " << opt_level << " \"" << in_path.string() << "\" \"" << out_path.string()
        << "\" 2>&1";
    if (std::system(cmd.str().c_str()) != 0) {
        spdlog::error("circuit_compile: BQSKit subprocess failed.");
        return std::nullopt;
    }

    auto parsed = from_file(out_path);
    if (!parsed.has_value()) {
        spdlog::error("circuit_compile: failed to read BQSKit output QASM.");
        return std::nullopt;
    }
    parsed->add_procedure("U3Syn-External");
    return parsed;
}

[[nodiscard]] bool should_use_block_engine(U3CxCompileOptions const& opt, size_t n_qubits) {
    auto const engine = effective_block_engine(opt);
    if (engine == BlockSynthEngine::Native) return false;
    // Native QSearch / LEAP variants do not go through the external
    // bqskit subprocess; they are handled by `synthesize_block` itself.
    if (engine == BlockSynthEngine::QSearchNative ||
        engine == BlockSynthEngine::LeapNative) return false;
    if (engine == BlockSynthEngine::QFast && n_qubits < opt.qfast_min_qubits) return false;
    return n_qubits >= 2;
}

[[nodiscard]] std::optional<QCir>
synthesize_block(QCir const& block, std::map<QubitIdType, QubitIdType> const& to_local,
                 U3CxCompileOptions const& opt) {
    if (block.get_num_gates() == 0) return QCir{static_cast<size_t>(to_local.size())};

    auto const n_local = to_local.size();
    auto try_block_engine = [&](BlockSynthEngine engine) -> std::optional<QCir> {
        return compile_via_bqskit_block(block, engine, effective_opt_level(opt), opt.max_block_qubits);
    };

    if (opt.prefer_qsearch_blocks && n_local >= 2) {
        if (auto ext = try_block_engine(BlockSynthEngine::QSearch); ext.has_value()) return ext;
    }

    if (should_use_block_engine(opt, n_local)) {
        if (auto ext = try_block_engine(effective_block_engine(opt)); ext.has_value()) return ext;
        if (effective_block_engine(opt) == BlockSynthEngine::QFast) {
            if (auto ext = try_block_engine(BlockSynthEngine::QSearch); ext.has_value()) return ext;
        }
        spdlog::warn("circuit_compile: block engine '{}' unavailable; native QSD/KAK.",
                     block_engine_name(effective_block_engine(opt)));
    }

    if (to_local.size() == 1) {
        QCir out{1};
        for (auto const* g : block.get_gates()) {
            auto tens = to_tensor(g->get_operation());
            if (!tens.has_value()) {
                out.append(g->get_operation(), {0});
                continue;
            }
            auto u3 = tensor::kak::single_qubit_synthesize(*tens);
            if (!u3.has_value()) return std::nullopt;
            for (auto const* sg : u3->get_gates()) {
                out.append(sg->get_operation(), {0});
            }
        }
        return out;
    }

    auto tensor_opt = to_tensor(block);
    if (!tensor_opt.has_value()) return std::nullopt;
    *tensor_opt = tensor_opt->to_matrix();

    // ---- Permutation-Aware Synthesis (PAS) ----
    if (opt.use_pas && n_local >= 2 && n_local <= opt.pas_max_qubits) {
        bool const lbfgs_for_kak = opt.qfactor_use_lbfgs || opt.three_cnot_use_lbfgs;
        tensor::qsd::QSDOptions qsd_opt;
        qsd_opt.inter_round_gate_removal = opt.qsd_inter_round_scan;
        qsd_opt.synthesis_epsilon        = opt.synthesis_epsilon;
        qsd_opt.try_three_cnot_qfactor   = opt.try_three_cnot_kak;
        qsd_opt.three_cnot_restarts      = opt.three_cnot_restarts;
        qsd_opt.three_cnot_use_lbfgs     = lbfgs_for_kak;

        synthesis::pas::BaseSynth base = [&](tensor::QTensor<double> const& U) -> std::optional<QCir> {
            if (n_local == 2) {
                tensor::kak::TwoQubitSynthesizeOptions kopt;
                kopt.try_three_cnot_qfactor = opt.try_three_cnot_kak;
                kopt.three_cnot_restarts    = opt.three_cnot_restarts;
                kopt.three_cnot_use_lbfgs   = lbfgs_for_kak;
                return tensor::kak::two_qubit_synthesize(U, kopt);
            }
            return tensor::qsd::synthesize(U, qsd_opt);
        };
        synthesis::pas::PasOptions pas_opt;
        pas_opt.max_qubits = opt.pas_max_qubits;
        if (auto pas_out = synthesis::pas::pas_synthesize(*tensor_opt, base, pas_opt);
            pas_out.has_value()) {
            auto retargeted = retarget_to_u3_cx(*pas_out);
            retargeted.add_procedure("PAS");
            return retargeted;
        }
        spdlog::debug("circuit_compile: PAS found no improvement; falling back.");
    }

    // ---- Native QSearch / LEAP ----
    {
        auto const engine = effective_block_engine(opt);
        if (engine == BlockSynthEngine::QSearchNative ||
            engine == BlockSynthEngine::LeapNative) {
            synthesis::search::QSearchOptions sopt;
            sopt.success_threshold = opt.synthesis_epsilon;
            sopt.minimizer         = tensor::opt::MinimizerKind::LBFGS;
            // Heuristic budget: scale max_depth with qubit count so that 3-
            // and 4-qubit blocks have room to grow without runaway frontiers.
            sopt.max_depth         = std::max<std::size_t>(4, n_local * 3);
            sopt.max_iterations    = std::max<std::size_t>(64, n_local * 64);
            sopt.instantiate_iters = 30;
            sopt.verbosity         = 0;
            std::optional<QCir> native;
            if (engine == BlockSynthEngine::LeapNative) {
                synthesis::search::LeapOptions lopt;
                static_cast<synthesis::search::QSearchOptions&>(lopt) = sopt;
                lopt.prefix_freeze_period = std::max<std::size_t>(2, n_local);
                native = synthesis::search::leap_synthesize(*tensor_opt, lopt);
            } else {
                native = synthesis::search::qsearch_synthesize(*tensor_opt, sopt);
            }
            if (native.has_value()) return retarget_to_u3_cx(*native);
            spdlog::warn("circuit_compile: native '{}' failed; falling back to KAK/QSD.",
                         block_engine_name(engine));
        }
    }

    bool const lbfgs_for_kak         = opt.qfactor_use_lbfgs || opt.three_cnot_use_lbfgs;
    tensor::qsd::QSDOptions qsd_opt;
    qsd_opt.inter_round_gate_removal = opt.qsd_inter_round_scan;
    qsd_opt.synthesis_epsilon        = opt.synthesis_epsilon;
    qsd_opt.try_three_cnot_qfactor   = opt.try_three_cnot_kak;
    qsd_opt.three_cnot_restarts      = opt.three_cnot_restarts;
    qsd_opt.three_cnot_use_lbfgs     = lbfgs_for_kak;

    if (n_local == 2) {
        tensor::kak::TwoQubitSynthesizeOptions kopt;
        kopt.try_three_cnot_qfactor = opt.try_three_cnot_kak;
        kopt.three_cnot_restarts    = opt.three_cnot_restarts;
        kopt.three_cnot_use_lbfgs   = lbfgs_for_kak;
        kopt.three_cnot_epsilon     = opt.synthesis_epsilon * 100;
        if (auto kak = tensor::kak::two_qubit_synthesize(*tensor_opt, kopt); kak.has_value()) {
            return retarget_to_u3_cx(*kak);
        }
    }

    auto synth = tensor::qsd::synthesize(*tensor_opt, qsd_opt);
    if (!synth.has_value()) return std::nullopt;
    return synth;
}

[[nodiscard]] QCir embed_local(QCir const& local,
                               std::map<QubitIdType, QubitIdType> const& to_local) {
    size_t n_global = 0;
    for (auto const& [g, l] : to_local) {
        (void)l;
        n_global = std::max(n_global, static_cast<size_t>(g) + 1);
    }
    std::vector<QubitIdType> loc_to_glob(to_local.size());
    for (auto const& [g, l] : to_local) {
        loc_to_glob[l] = g;
    }
    QCir result{n_global};
    for (auto const* gate : local.get_gates()) {
        QubitIdList mapped;
        for (auto lq : gate->get_qubits()) {
            mapped.push_back(loc_to_glob[lq]);
        }
        result.append(gate->get_operation(), mapped);
    }
    return result;
}

[[nodiscard]] std::optional<QCir> compile_monolithic(QCir const& src, U3CxCompileOptions const& opt) {
    auto tensor_opt = to_tensor(src);
    if (!tensor_opt.has_value()) return std::nullopt;
    *tensor_opt = tensor_opt->to_matrix();
    tensor::qsd::QSDOptions qsd_opt;
    qsd_opt.inter_round_gate_removal = opt.qsd_inter_round_scan;
    qsd_opt.synthesis_epsilon        = opt.synthesis_epsilon;
    qsd_opt.try_three_cnot_qfactor   = opt.try_three_cnot_kak;
    qsd_opt.three_cnot_restarts      = opt.three_cnot_restarts;
    qsd_opt.three_cnot_use_lbfgs     = opt.qfactor_use_lbfgs || opt.three_cnot_use_lbfgs;
    auto synth = tensor::qsd::synthesize(*tensor_opt, qsd_opt);
    if (!synth.has_value()) return std::nullopt;
    return retarget_to_u3_cx(*synth);
}

[[nodiscard]] bool synthesize_region(UnitaryRegion const& region, QCir& result,
                                     U3CxCompileOptions const& opt) {
    if (region.gates.empty()) return true;

    std::map<QubitIdType, QubitIdType> to_local;
    size_t                             next = 0;
    for (auto q : region.qubits) to_local[q] = static_cast<QubitIdType>(next++);

    if (region.gates.size() == 1) {
        auto const* g = region.gates[0];
        result.append(g->get_operation(), g->get_qubits());
        return true;
    }

    if (region.qubits.size() > opt.max_block_qubits) {
        for (auto const* g : region.gates) {
            QCir single{result.get_num_qubits()};
            single.append(g->get_operation(), g->get_qubits());
            auto one = compile_monolithic(single, opt);
            if (!one.has_value()) {
                result.append(g->get_operation(), g->get_qubits());
            } else {
                result.compose(*one);
            }
        }
        return true;
    }

    QCir block{to_local.size()};
    for (auto const* g : region.gates) {
        QubitIdList local_qs;
        for (auto qb : g->get_qubits()) local_qs.push_back(to_local.at(qb));
        block.append(g->get_operation(), local_qs);
    }

    auto local_synth = synthesize_block(block, to_local, opt);
    if (!local_synth.has_value()) return false;

    auto embedded = embed_local(retarget_to_u3_cx(*local_synth), to_local);
    result.compose(embedded);
    return true;
}

[[nodiscard]] std::optional<QCir> compile_partitioned(QCir const& src, U3CxCompileOptions const& opt) {
    QCir result{src.get_num_qubits()};

    auto regions = partition_circuit(src, opt.max_block_qubits, opt.partition_strategy);

    if (opt.partition_at_entangling && effective_block_engine(opt) == BlockSynthEngine::Native) {
        std::vector<UnitaryRegion> refined;
        refined.reserve(regions.size() * 2);
        for (auto const& region : regions) {
            auto parts = split_entangling_boundaries(region);
            refined.insert(refined.end(), parts.begin(), parts.end());
        }
        regions = std::move(refined);
    }

    for (auto const& region : regions) {
        if (!synthesize_region(region, result, opt)) return std::nullopt;
    }

    return retarget_to_u3_cx(result);
}

[[nodiscard]] CouplingConstraints resolve_coupling(QCir const& circ, U3CxCompileOptions const& opt) {
    if (!opt.coupling.all_to_all) return opt.coupling;
    return all_to_all_coupling(circ.get_num_qubits());
}

[[nodiscard]] QCir scanning_gate_removal_lite(QCir const& compiled,
                                              tensor::QTensor<double> const& target_unitary,
                                              U3CxCompileOptions const& opt) {
    ScanningGateRemovalOptions sopt;
    sopt.synthesis_epsilon = opt.synthesis_epsilon;
    sopt.coupling          = resolve_coupling(compiled, opt);
    if (opt.optimization_level >= 2) {
        return gate_deletion_optimization_workflow(compiled, target_unitary, sopt);
    }
    return scanning_gate_removal_workflow(compiled, target_unitary, sopt);
}

}  // namespace

namespace {

// True iff `op` is a CXGate (ControlGate with a single Pauli-X target).
[[nodiscard]] bool is_cx_gate(Operation const& op) {
    auto const ctrl = op.get_underlying_if<ControlGate>();
    if (!ctrl.has_value()) return false;
    if (ctrl->get_num_ctrls() != 1) return false;
    auto const& targ = ctrl->get_target_operation();
    auto const  px   = targ.get_underlying_if<PXGate>();
    return px.has_value() && px->get_phase() == dvlab::Phase(1);  // X = PX(π).
}

// True iff `op` is a UGate.
[[nodiscard]] bool is_u_gate(Operation const& op) {
    return op.get_underlying_if<UGate>().has_value();
}

// Expand a 1-qubit gate (any kind) to a single UGate via KAK ZYZ.
[[nodiscard]] std::optional<UGate> rebase_single_qubit(Operation const& op) {
    auto tens = to_tensor(op);
    if (!tens.has_value()) return std::nullopt;
    auto u3 = tensor::kak::single_qubit_synthesize(*tens);
    if (!u3.has_value() || u3->get_num_gates() != 1) return std::nullopt;
    auto const& g = u3->get_gates().front();
    auto const  u = g->get_operation().get_underlying_if<UGate>();
    if (!u.has_value()) return std::nullopt;
    return *u;
}

}  // namespace

QCir retarget_to_u3_cx(QCir const& src) {
    QCir out{src.get_num_qubits()};
    for (auto const* g : src.get_gates()) {
        auto const& op = g->get_operation();

        // Already in target gate set.
        if (is_u_gate(op) || is_cx_gate(op)) {
            out.append(op, g->get_qubits());
            continue;
        }

        // -------------------- 1-qubit rebase --------------------
        if (g->get_num_qubits() == 1) {
            if (auto u = rebase_single_qubit(op); u.has_value()) {
                out.append(Operation{*u}, g->get_qubits());
                continue;
            }
            spdlog::warn("retarget_to_u3_cx: failed to rebase single-qubit gate '{}'; passing through.",
                         op.get_type());
            out.append(op, g->get_qubits());
            continue;
        }

        // -------------------- 2-qubit rebase --------------------
        if (g->get_num_qubits() == 2) {
            if (auto tens = to_tensor(op); tens.has_value()) {
                auto m = tens->to_matrix();
                if (m.shape().size() == 2 && m.shape()[0] == 4 && m.shape()[1] == 4) {
                    if (auto circ = tensor::kak::two_qubit_synthesize(m); circ.has_value()) {
                        for (auto const* sg : circ->get_gates()) {
                            QubitIdList q;
                            q.reserve(sg->get_num_qubits());
                            for (auto local : sg->get_qubits()) {
                                q.push_back(g->get_qubits().at(local));
                            }
                            out.append(sg->get_operation(), q);
                        }
                        continue;
                    }
                }
            }
            spdlog::warn("retarget_to_u3_cx: failed to rebase 2-qubit gate '{}'; passing through.",
                         op.get_type());
            out.append(op, g->get_qubits());
            continue;
        }

        // -------------------- 3+-qubit (e.g. CCX/CCZ): leave to upstream
        //                      partition + QSD path; pass through unchanged.
        spdlog::debug("retarget_to_u3_cx: leaving {}-qubit gate '{}' intact for upstream synthesis.",
                      g->get_num_qubits(), op.get_type());
        out.append(op, g->get_qubits());
    }
    return out;
}

// Returns true iff every gate in `circ` is either a UGate or a CXGate
// (no other 2-qubit gates, no remaining single-qubit RZ/RY/RX/H/etc.).
// Useful as a hard contract on the output of `compile_to_u3_cnot_impl`.
[[nodiscard]] bool circuit_is_u3_cx_only(QCir const& circ) {
    for (auto const* g : circ.get_gates()) {
        auto const& op = g->get_operation();
        if (!is_u_gate(op) && !is_cx_gate(op)) return false;
    }
    return true;
}

std::optional<QCir> compile_to_u3_cnot_impl(QCir const& src, U3CxCompileOptions const& opt) {
    if (src.is_empty()) {
        spdlog::warn("circuit_compile: empty QCir.");
        return QCir{0};
    }

    if (!validate_synthesis_flags(opt)) {
        return std::nullopt;
    }

    U3CxCompileOptions run_opt = opt;
    if (run_opt.coupling.all_to_all && !src.is_empty()) {
        run_opt.coupling = all_to_all_coupling(src.get_num_qubits());
    }

    if (use_external_u3syn(opt)) {
        auto const bq_level = effective_opt_level(opt);
        auto       out      = compile_via_bqskit(src, bq_level);
        if (out.has_value()) {
            out->add_procedure("U3Syn-BQSKit");
        }
        return out;
    }

    std::optional<QCir> out;
    if (should_use_monolithic(src, run_opt)) {
        spdlog::debug("circuit_compile: monolithic QSD (n={}, gates=1).", src.get_num_qubits());
        out = compile_monolithic(src, run_opt);
        if (out.has_value()) out->add_procedure("To-U3CX");
    } else {
        spdlog::info("circuit_compile: partitioned compile (n={}, gates={}, max_block={}).",
                     src.get_num_qubits(), src.get_num_gates(), run_opt.max_block_qubits);
        out = compile_partitioned(src, run_opt);
        if (out.has_value()) out->add_procedure("To-U3CX-Partitioned");
    }

    if (!out.has_value()) return std::nullopt;

    auto target_opt = to_tensor(*out);
    if (!target_opt.has_value()) {
        spdlog::warn("circuit_compile: skipping opt-level passes (tensor failed).");
        return out;
    }
    *target_opt = target_opt->to_matrix();

    if (run_opt.optimization_level >= 1) {
        *out = apply_native_optimization(*out, *target_opt, run_opt);
    }

    // Hard contract: every gate in the final output must be UGate or CXGate.
    // Run one more retarget pass to guarantee it.
    *out = retarget_to_u3_cx(*out);
    if (!circuit_is_u3_cx_only(*out)) {
        spdlog::error("compile_to_u3_cnot_impl: output contains non-U3+CX gate(s); "
                      "this indicates an unsupported input gate type or a bug in retarget_to_u3_cx.");
    }
    return out;
}

}  // namespace qsyn::qcir
