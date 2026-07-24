/****************************************************************************
  PackageName  [ qcir / cpf_pipeline ]
  Synopsis     [ Target CPF product pipeline implementation. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./cpf_pipeline.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <ranges>
#include <spdlog/spdlog.h>

#include <fmt/format.h>
#include <variant>

#include "convert/qcir_to_tensor.hpp"
#include "convert/tableau_to_qcir.hpp"
#include "tableau/cpf/trace_replay.hpp"
#include "tableau/cpf/pauli_compress.hpp"
#include "tableau/pauli_dag/dag_fold.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau_optimization.hpp"
#include "qcir/circuit_compile.hpp"
#include "qcir/basic_gate_type.hpp"

namespace qsyn::qcir {

CpfFoldStrategy parse_cpf_fold_strategy(std::string const& name) {
    std::string s = name;
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (s == "dag" || s == "default") return CpfFoldStrategy::dag;
    if (s == "global" || s == "global_only" || s == "global-fold") {
        return CpfFoldStrategy::global_only;
    }
    if (s == "staq" || s == "staq_only") return CpfFoldStrategy::staq_only;
    if (s == "no_prune" || s == "no-global-prune" || s == "no_global_prune") {
        return CpfFoldStrategy::no_global_prune;
    }
    if (s == "matching" || s == "cole-matching") return CpfFoldStrategy::matching;
    return CpfFoldStrategy::dag;
}

namespace {

[[nodiscard]] experimental::cpf::DagFoldOptions dag_options_for(CpfFoldStrategy strat) {
    experimental::cpf::DagFoldOptions opt;
    switch (strat) {
        case CpfFoldStrategy::staq_only:
            opt.run_graph_merge  = false;
            opt.run_global_prune = false;
            opt.run_global_fold  = false;
            opt.max_passes       = 1;
            break;
        case CpfFoldStrategy::no_global_prune:
            opt.run_global_prune = false;
            break;
        case CpfFoldStrategy::matching:
            opt.use_matching_merge = true;
            break;
        case CpfFoldStrategy::dag:
        default:
            break;
    }
    return opt;
}

}  // namespace

namespace {

[[nodiscard]] std::size_t count_rotation_gates(QCir const& qcir) {
    std::size_t n = 0;
    for (auto const& gate : qcir.get_gates()) {
        auto const& op = gate->get_operation();
        if (op.is<RXGate>() || op.is<RYGate>() || op.is<RZGate>()) {
            ++n;
        }
    }
    return n;
}

[[nodiscard]] bool all_rotations_diagonal(std::vector<experimental::PauliRotation> const& rots) {
    return std::ranges::all_of(rots, &experimental::PauliRotation::is_diagonal);
}

void log_qcir_gate_type_counts(QCir const& qcir, std::string_view step) {
    if (qcir.is_empty()) return;
    auto const stat = get_gate_statistics(qcir);
    static constexpr std::string_view k_agg[] = {
        "clifford", "1-qubit", "2-qubit", "h-internal", "t-family"};
    spdlog::info("cpf-pipeline step {} QCir: {} gates", step, qcir.get_num_gates());
    std::vector<std::pair<std::string, std::size_t>> types;
    for (auto const& [type, count] : stat) {
        if (std::ranges::any_of(k_agg, [&](std::string_view k) { return k == type; })) {
            continue;
        }
        types.emplace_back(type, count);
    }
    std::ranges::sort(types, {}, &decltype(types)::value_type::first);
    for (auto const& [type, count] : types) {
        spdlog::info("  gate type {:<8}: {}", type, count);
    }
    if (stat.contains("2-qubit")) {
        spdlog::info("  2-qubit total    : {}", stat.at("2-qubit"));
    }
}

void log_qcir_arbitrary_rotation_gates(QCir const& qcir, std::string_view step) {
    std::vector<std::string> lines;
    for (auto const& gate : qcir.get_gates()) {
        auto const& op = gate->get_operation();
        if (!op.is<RXGate>() && !op.is<RYGate>() && !op.is<RZGate>() && !op.is<UGate>()) {
            continue;
        }
        auto qubits = gate->get_qubits();
        lines.push_back(fmt::format(
            "  {} {}",
            op.get_repr(),
            fmt::join(qubits | std::views::transform([](QubitIdType q) {
                          return fmt::format("q[{}]", q);
                      }),
                      ", ")));
    }
    if (lines.empty()) return;
    spdlog::info("cpf-pipeline step {} arbitrary rotation gates ({}):", step, lines.size());
    for (auto const& line : lines) {
        spdlog::info("{}", line);
    }
}

void log_tableau_pauli_rotations(experimental::Tableau const& tab, std::string_view step) {
    spdlog::info("cpf-pipeline step {}: {} Pauli rotations ({} Clifford segments)",
                 step, tab.n_pauli_rotations(), tab.n_cliffords());
    std::size_t idx = 0;
    for (auto const& sub : tab) {
        auto const* rots = std::get_if<std::vector<experimental::PauliRotation>>(&sub);
        if (rots == nullptr) continue;
        for (auto const& r : *rots) {
            spdlog::info("  Pauli rotation [{}]: {}", idx++, fmt::format("{}", r));
        }
    }
}

}  // namespace

std::optional<QCir> compile_to_u3_cnot(QCir const& src, U3CxCompileOptions const& opt) {
    auto out = compile_to_u3_cnot_impl(src, opt);
    if (!out.has_value()) {
        spdlog::error("cpf-pipeline: U3+CX synthesis failed.");
    }
    return out;
}

std::optional<QCir> compile_to_zyz_cnot(QCir const& src) {
    auto expanded = to_basic_gates(src);
    if (!expanded.has_value()) {
        spdlog::error("cpf-pipeline: ZYZ+CX expansion failed.");
        return std::nullopt;
    }
    expanded->add_procedure("To-ZYZCX");
    return expanded;
}

std::optional<experimental::Tableau> build_cpf_tableau(QCir const& src) {
    auto tab = experimental::cpf::trace_replay(src);
    if (!tab.has_value()) {
        spdlog::error("cpf-pipeline: trace_replay failed (unsupported gate?).");
    }
    return tab;
}

void apply_cpf_fold(experimental::Tableau& tableau, CpfPipelineStats& stats,
                    CpfPipelineOptions const& opt) {
    stats.ran_cpf_fold          = opt.run_cpf_fold;
    stats.rotations_before_fold = tableau.n_pauli_rotations();
    stats.cliffords             = tableau.n_cliffords();

    if (opt.run_cpf_fold) {
        auto const strat = opt.use_dag_fold ? opt.fold_strategy : CpfFoldStrategy::global_only;
        if (strat == CpfFoldStrategy::global_only) {
            stats.fold_stats = experimental::cpf::global_fold(tableau);
        } else {
            stats.used_dag_fold  = true;
            stats.dag_fold_stats = experimental::cpf::dag_fold(tableau, dag_options_for(strat));
            stats.fold_stats     = stats.dag_fold_stats.local;
        }
    }
    if (opt.run_full_optimize) {
        experimental::full_optimize(tableau);
    }

    stats.rotations_after_fold = tableau.n_pauli_rotations();
}

void canonicalize_cpf_tableau(experimental::Tableau& tableau, CpfPipelineStats& stats) {
    experimental::collapse(tableau);
    stats.rotations_after_collapse = tableau.n_pauli_rotations();
    stats.cliffords_after_collapse = tableau.n_cliffords();
}

void apply_lossless_pauli_compress(experimental::Tableau& tableau, CpfPipelineStats& stats,
                                   CpfPipelineOptions const& opt) {
    stats.ran_lossless_pauli_compress = opt.run_lossless_pauli_compress;
    stats.rotations_after_pauli_compress = stats.rotations_after_collapse;

    if (!opt.run_lossless_pauli_compress) {
        return;
    }

    experimental::cpf::PauliCompressOptions pc_opt;
    pc_opt.l2_budget     = 0.0;
    pc_opt.absorb_clifford = true;
    stats.pauli_compress_stats = experimental::cpf::pauli_compress(tableau, pc_opt);
    stats.rotations_after_pauli_compress = tableau.n_pauli_rotations();
    stats.cliffords_after_collapse       = tableau.n_cliffords();
}

void log_cpf_pipeline_step_counts(CpfPipelineStats const& stats, bool ran_fold) {
    if (ran_fold) {
        spdlog::info("cpf-pipeline step dag_fold summary: {} -> {} Pauli rotations",
                     stats.rotations_before_fold, stats.rotations_after_fold);
    } else {
        spdlog::info("cpf-pipeline step dag_fold summary: skipped ({} Pauli rotations)",
                     stats.rotations_after_trace_replay);
    }
    if (stats.ran_lossless_pauli_compress) {
        auto const& pc = stats.pauli_compress_stats;
        spdlog::info(
            "cpf-pipeline step pauli-compress summary: {} -> {} Pauli rotations "
            "({} D-merged, {} F-cancelled)",
            pc.n_before, pc.n_after, pc.n_merged, pc.n_cancelled);
    }
}

std::optional<QCir> synthesize_tableau_to_qcir(experimental::Tableau const& tableau,
                                               CpfPipelineStats& stats,
                                               CpfPipelineOptions const& opt) {
    experimental::HOptSynthesisStrategy                 st_strategy;
    experimental::GraySynthPauliRotationsSynthesisStrategy gray_strategy;
    experimental::NaivePauliRotationsSynthesisStrategy naive_strategy;

    qcir::QCir qcir{tableau.n_qubits()};

    for (auto const& sub : tableau) {
        if (auto const* st = std::get_if<experimental::StabilizerTableau>(&sub)) {
            auto frag = experimental::to_qcir(*st, st_strategy);
            if (!frag.has_value()) return std::nullopt;
            qcir.compose(*frag);
            continue;
        }

        auto const* rots = std::get_if<std::vector<experimental::PauliRotation>>(&sub);
        if (rots == nullptr || rots->empty()) continue;

        std::optional<QCir> frag;
        if (opt.prefer_graysynth && all_rotations_diagonal(*rots)) {
            frag = gray_strategy.synthesize(*rots);
            if (frag.has_value()) stats.used_graysynth_per_block = true;
        }
        if (!frag.has_value()) {
            frag = naive_strategy.synthesize(*rots);
            if (frag.has_value() && opt.prefer_graysynth) {
                stats.used_naive_fallback = true;
            }
        }
        if (!frag.has_value()) {
            spdlog::error("cpf-pipeline: failed to synthesise a Pauli-rotation block.");
            return std::nullopt;
        }
        qcir.compose(*frag);
    }

    qcir.add_procedure("From-Tableau-GraySynth");
    return qcir;
}

std::optional<QCir> run_cpf_pipeline_round(QCir const& src, CpfPipelineStats& stats,
                                           CpfPipelineOptions const& opt) {
    QCir const* cur = &src;

    std::optional<QCir> u3_owned;
    if (!opt.skip_u3cx) {
        u3_owned = compile_to_u3_cnot(src, opt.u3cx);
        if (!u3_owned.has_value()) return std::nullopt;
        u3_owned->set_filename(src.get_filename());
        u3_owned->add_procedures(src.get_procedures());
        cur = &*u3_owned;
    }

    auto zyz = compile_to_zyz_cnot(*cur);
    if (!zyz.has_value()) return std::nullopt;
    zyz->set_filename(src.get_filename());
    zyz->add_procedures(cur->get_procedures());

    log_qcir_gate_type_counts(*cur, "input (before ZYZ)");
    log_qcir_arbitrary_rotation_gates(*cur, "input (before ZYZ)");
    log_qcir_gate_type_counts(*zyz, "ZYZ+CX");
    log_qcir_arbitrary_rotation_gates(*zyz, "ZYZ+CX");

    auto tableau = build_cpf_tableau(*zyz);
    if (!tableau.has_value()) return std::nullopt;

    stats.rotations_after_trace_replay   = tableau->n_pauli_rotations();
    stats.cliffords_after_trace_replay   = tableau->n_cliffords();
    stats.cliffords                      = stats.cliffords_after_trace_replay;
    stats.rotations_before_fold          = stats.rotations_after_trace_replay;
    log_tableau_pauli_rotations(*tableau, "trace_replay");

    apply_cpf_fold(*tableau, stats, opt);
    log_tableau_pauli_rotations(*tableau, "dag_fold");

    canonicalize_cpf_tableau(*tableau, stats);
    log_tableau_pauli_rotations(*tableau, "collapse");

    apply_lossless_pauli_compress(*tableau, stats, opt);
    log_tableau_pauli_rotations(*tableau, "pauli-compress");
    log_cpf_pipeline_step_counts(stats, opt.run_cpf_fold);

    auto out = synthesize_tableau_to_qcir(*tableau, stats, opt);
    if (!out.has_value()) return std::nullopt;

    log_qcir_gate_type_counts(*out, "from-tableau");
    log_qcir_arbitrary_rotation_gates(*out, "from-tableau");
    spdlog::info(
        "cpf-pipeline step from-tableau summary: {} Pauli rotations -> QCir with {} gates ({} rx/ry/rz)",
        stats.rotations_after_pauli_compress, out->get_num_gates(), count_rotation_gates(*out));

    out->set_filename(src.get_filename());
    out->add_procedures(zyz->get_procedures());
    return out;
}

std::optional<QCir> run_cpf_pipeline(QCir const& src, CpfPipelineStats& stats,
                                     CpfPipelineOptions const& opt) {
    QCir cur = src;
    cur.set_filename(src.get_filename());
    cur.add_procedures(src.get_procedures());

    std::size_t const max_rounds = std::max<std::size_t>(1, opt.max_rounds);
    std::size_t       prev_rots  = std::numeric_limits<std::size_t>::max();

    for (std::size_t rnd = 0; rnd < max_rounds; ++rnd) {
        CpfPipelineStats round_stats;
        auto             out = run_cpf_pipeline_round(cur, round_stats, opt);
        if (!out.has_value()) return std::nullopt;

        stats.rotations_after_trace_replay = round_stats.rotations_after_trace_replay;
        stats.rotations_before_fold        = round_stats.rotations_before_fold;
        stats.rotations_after_fold         = round_stats.rotations_after_fold;
        stats.rotations_after_collapse     = round_stats.rotations_after_collapse;
        stats.rotations_after_pauli_compress = round_stats.rotations_after_pauli_compress;
        stats.cliffords_after_trace_replay = round_stats.cliffords_after_trace_replay;
        stats.cliffords_after_collapse     = round_stats.cliffords_after_collapse;
        stats.cliffords                    = round_stats.cliffords;
        stats.fold_stats                   = round_stats.fold_stats;
        stats.dag_fold_stats               = round_stats.dag_fold_stats;
        stats.pauli_compress_stats         = round_stats.pauli_compress_stats;
        stats.used_dag_fold                = round_stats.used_dag_fold;
        stats.ran_cpf_fold                 = round_stats.ran_cpf_fold;
        stats.ran_lossless_pauli_compress  = round_stats.ran_lossless_pauli_compress;
        stats.used_graysynth_per_block     = round_stats.used_graysynth_per_block;
        stats.used_naive_fallback          = round_stats.used_naive_fallback;
        stats.n_rounds                     = rnd + 1;

        out->add_procedure(rnd == 0 ? "CPF-Pipeline" : fmt::format("CPF-Round-{}", rnd + 1));

        if (rnd > 0 && round_stats.rotations_after_pauli_compress >= prev_rots) {
            return out;
        }
        prev_rots = round_stats.rotations_after_pauli_compress;

        if (rnd + 1 < max_rounds) {
            cur = std::move(*out);
        } else {
            return out;
        }
    }

    return std::nullopt;
}

}  // namespace qsyn::qcir
