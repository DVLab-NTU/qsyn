/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ Pauli-sum stream import/export and global label pruning. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./pauli_dag.hpp"

#include <algorithm>

#include "./timeline.hpp"
#include "tableau/cpf/angle_utils.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

namespace {

[[nodiscard]] PauliLabelKey label_key(PauliProduct const& p) {
    return p.to_bit_string();
}

}  // namespace

PauliDag from_tableau(Tableau const& tableau) {
    auto const tl = build_timeline(tableau);
    PauliDag   dag;
    dag.n_qubits = tl.n_qubits;
    dag.entries  = tl.entries;
    return dag;
}

Tableau to_tableau(PauliDag const& dag) {
    Timeline tl;
    tl.n_qubits = dag.n_qubits;
    tl.entries  = dag.entries;
    return rebuild_tableau(tl);
}

std::unordered_map<PauliLabelKey, dvlab::Phase>
accumulate_global_terms(PauliDag const& dag) {
    std::unordered_map<PauliLabelKey, dvlab::Phase> acc;
    for (auto const& e : dag.entries) {
        auto const* r = std::get_if<PauliRotation>(&e);
        if (r == nullptr) continue;
        auto key = label_key(r->pauli_product());
        auto it  = acc.find(key);
        if (it == acc.end()) {
            acc.emplace(key, r->phase());
        } else {
            it->second += r->phase();
        }
    }
    return acc;
}

FoldTermsResult fold_terms(std::unordered_map<PauliLabelKey, dvlab::Phase> const& raw) {
    FoldTermsResult out;
    out.n_merged = raw.size() > 0 ? 0 : 0;

    std::unordered_map<PauliLabelKey, dvlab::Phase> acc = raw;
    if (raw.size() > acc.size()) {
        out.n_merged = raw.size() - acc.size();
    }

    std::size_t const n_in = acc.size();
    for (auto const& [key, phase] : acc) {
        if (is_zero_phase(phase)) {
            ++out.n_removed_zero;
            continue;
        }
        if (is_clifford_phase(phase)) {
            // Count for stats; keep rotations in the stream so equivalence
            // is preserved. Clifford absorption is `full_optimize`'s job.
            ++out.n_removed_clifford;
        }
        out.pruned.emplace(key, phase);
    }
    out.n_merged = n_in > out.pruned.size() ? n_in - out.pruned.size() : 0;
    return out;
}

std::unordered_set<PauliLabelKey>
globally_removed_labels(std::unordered_map<PauliLabelKey, dvlab::Phase> const& raw,
                        FoldTermsResult const&                         folded) {
    std::unordered_set<PauliLabelKey> removed;
    for (auto const& [key, _] : raw) {
        if (folded.pruned.find(key) == folded.pruned.end()) {
            removed.insert(key);
        }
    }
    return removed;
}

GlobalPruneStats global_prune_pass(Tableau& tableau) {
    GlobalPruneStats stats;
    auto const dag = from_tableau(tableau);
    auto const raw = accumulate_global_terms(dag);
    stats.n_terms_before = raw.size();

    auto const folded  = fold_terms(raw);
    stats.n_merged_global        = folded.n_merged;
    stats.n_removed_zero         = folded.n_removed_zero;
    stats.n_removed_clifford     = folded.n_removed_clifford;
    stats.n_terms_after          = folded.pruned.size();

    auto const removed = globally_removed_labels(raw, folded);

    Tableau new_tab{tableau.n_qubits()};
    std::vector<PauliRotation> cur_rots;

    auto flush_rots = [&]() {
        if (!cur_rots.empty()) {
            new_tab.emplace_back(std::move(cur_rots));
            cur_rots.clear();
        }
    };

    for (auto const& sub : tableau) {
        if (auto const* st = std::get_if<StabilizerTableau>(&sub)) {
            flush_rots();
            new_tab.emplace_back(*st);
            continue;
        }
        auto const* rots = std::get_if<std::vector<PauliRotation>>(&sub);
        if (rots == nullptr) continue;
        for (auto const& r : *rots) {
            auto key = label_key(r.pauli_product());
            if (removed.contains(key)) {
                ++stats.n_rotations_pruned;
                continue;
            }
            if (is_zero_phase(r.phase())) continue;
            cur_rots.push_back(r);
        }
    }
    flush_rots();
    tableau = std::move(new_tab);
    return stats;
}

}  // namespace qsyn::experimental::cpf::pauli_dag
