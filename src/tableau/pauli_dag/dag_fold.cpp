/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ Full Pauli-DAG CPF fold driver. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./dag_fold.hpp"

#include "./pauli_dag_graph.hpp"
#include "./staq_fold.hpp"
#include "./timeline.hpp"
#include "tableau/cpf/angle_utils.hpp"
#include "tableau/cpf/merge_log.hpp"
#include "tableau/tableau_optimization.hpp"

namespace qsyn::experimental::cpf {

namespace {

void log_zero_phase_rotations(Tableau const& tableau) {
    if (!merge_log_enabled()) return;

    // Mirror `collapse`: walk reverse, accumulate Clifford suffix, conjugate.
    using Flat = std::variant<StabilizerTableau, PauliRotation>;
    std::vector<Flat> flat;
    for (auto const& sub : tableau) {
        if (auto const* st = std::get_if<StabilizerTableau>(&sub)) {
            flat.emplace_back(*st);
            continue;
        }
        if (auto const* rots = std::get_if<std::vector<PauliRotation>>(&sub)) {
            for (auto const& r : *rots) flat.emplace_back(r);
        }
    }

    CliffordOperatorString suffix;
    int zero_idx = 0;
    for (std::size_t i = flat.size(); i-- > 0;) {
        if (auto const* st = std::get_if<StabilizerTableau>(&flat[i])) {
            StabilizerTableau tmp = *st;
            tmp.apply(suffix);
            suffix = extract_clifford_operators(tmp);
            continue;
        }
        auto const* rot = std::get_if<PauliRotation>(&flat[i]);
        PauliRotation rc = *rot;
        auto const phase = rc.phase();
        rc.apply(suffix);
        if (is_zero_phase(phase)) {
            merge_log_line(fmt::format(
                "{{\"event\":\"drop_zero\",\"kind\":\"pre_fold_zero\","
                "\"idx\":{},\"pauli_at_site\":\"{}\",\"collapsed_pauli\":\"{}\","
                "\"phase\":\"{}\"}}",
                zero_idx++,
                rot->pauli_product().to_string('+'),
                rc.pauli_product().to_string('+'),
                phase.get_print_string()));
        }
    }
}

}  // namespace

DagFoldStats dag_fold(Tableau& tableau, DagFoldOptions const& opt) {
    DagFoldStats stats;
    log_zero_phase_rotations(tableau);

    // The explicit Pauli-DAG graph merge (`build_pauli_dag_graph` +
    // `graph_component_merge`) is an O(n^2) all-pairs scan. For very large
    // rotation counts it dominates runtime, so above this threshold we rely on
    // the near-linear hash-based `global_prune_pass` (plus staq + global_fold)
    // instead. Smaller circuits keep the more aggressive graph merge.
    constexpr std::size_t graph_merge_max_rotations = 1500;

    while (true) {
        ++stats.n_passes;
        auto const rots_before = tableau.n_pauli_rotations();

        auto timeline = pauli_dag::build_timeline(tableau);
        pauli_dag::merge_track_begin(timeline, static_cast<int>(stats.n_passes));

        pauli_dag::StaqFoldStats st{};
        if (opt.run_staq) {
            st = pauli_dag::staq_fold_timeline(timeline);
            stats.staq.n_passes += st.n_passes;
            stats.staq.n_merges += st.n_merges;
            stats.staq.n_rotations_before = st.n_rotations_before;
            stats.staq.n_rotations_after  = st.n_rotations_after;
        }

        bool const do_graph_merge = opt.run_graph_merge && rots_before <= graph_merge_max_rotations;
        std::size_t graph_merges = 0;
        if (do_graph_merge) {
            auto graph = pauli_dag::build_pauli_dag_graph(timeline);
            stats.graph.n_nodes       = graph.nodes.size();
            stats.graph.n_merge_edges = graph.merge_edges.size();
            graph_merges = opt.use_matching_merge
                               ? pauli_dag::max_matching_merge(timeline, graph)
                               : pauli_dag::graph_component_merge(timeline, graph);
            stats.graph.n_component_merges += graph_merges;
        }

        pauli_dag::merge_track_end();
        tableau = pauli_dag::rebuild_tableau(timeline);

        pauli_dag::GlobalPruneStats gp{};
        if (opt.run_global_prune) {
            gp = pauli_dag::global_prune_pass(tableau);
            stats.global.n_terms_before += gp.n_terms_before;
            stats.global.n_terms_after = gp.n_terms_after;
            stats.global.n_merged_global += gp.n_merged_global;
            stats.global.n_removed_zero += gp.n_removed_zero;
            stats.global.n_removed_clifford += gp.n_removed_clifford;
            stats.global.n_rotations_pruned += gp.n_rotations_pruned;
        }

        GlobalFoldStats gf{};
        if (opt.run_global_fold) {
            gf = global_fold(tableau);
            stats.local.n_passes += gf.n_passes;
            stats.local.n_local_merges += gf.n_local_merges;
            stats.local.n_propagation_merges += gf.n_propagation_merges;
            stats.local.n_rotations_removed += gf.n_rotations_removed;
            stats.local.n_segments_collapsed += gf.n_segments_collapsed;
            stats.local.n_clifford_angle_left = gf.n_clifford_angle_left;
        }

        auto const rots_after = tableau.n_pauli_rotations();
        bool const changed =
            st.n_merges > 0 || graph_merges > 0 || gp.n_rotations_pruned > 0 ||
            gf.n_local_merges + gf.n_propagation_merges > 0 ||
            gf.n_segments_collapsed > 0 || rots_before != rots_after;

        if (!changed) break;
        if (stats.n_passes >= opt.max_passes) break;
    }

    return stats;
}

}  // namespace qsyn::experimental::cpf
