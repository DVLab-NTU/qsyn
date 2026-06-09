/**
 * @file t_merge.cpp
 * @brief BBMerge / FastTMerge core
 */

#include "./t_merge.hpp"

#include <cassert>
#include <optional>
#include <unordered_map>
#include <vector>

#include "tableau/stabilizer_tableau.hpp"

namespace qsyn::experimental {

namespace {

struct PauliAxisKeyHash {
    size_t operator()(PauliAxisKey const& key) const {
        // FNV-1a over set bits
        size_t hash = 14695981039346656037ULL;
        for (size_t i = 0; i < key.bits.size(); ++i) {
            if (key.bits[i]) {
                hash ^= i;
                hash *= 1099511628211ULL;
            }
        }
        return hash;
    }
};

void prepend_clifford(StabilizerTableau& tab, TMergeGate const& gate, bool s_as_sdg) {
    switch (gate.kind) {
        case TMergeGate::Kind::h:
            tab.prepend_h(gate.q0);
            break;
        case TMergeGate::Kind::x:
            tab.prepend_x(gate.q0);
            break;
        case TMergeGate::Kind::z:
            tab.prepend_z(gate.q0);
            break;
        case TMergeGate::Kind::s:
            if (s_as_sdg) {
                tab.prepend_sdg(gate.q0);
            } else {
                tab.prepend_s(gate.q0);
                tab.prepend_z(gate.q0);
            }
            break;
        case TMergeGate::Kind::sdg:
            tab.prepend_sdg(gate.q0);
            break;
        case TMergeGate::Kind::cx:
            tab.prepend_cx(gate.q0, gate.q1);
            break;
        case TMergeGate::Kind::t:
            break;
    }
}

bool diagonalize_pauli_rotation(StabilizerTableau& tab, size_t col) {
    auto const n_qubits = tab.n_qubits();
    std::optional<size_t> pivot;
    for (size_t j = 0; j < n_qubits; ++j) {
        if (tab.destabilizer(j).is_x_set(col)) {
            pivot = j;
            break;
        }
    }
    if (!pivot.has_value()) {
        return false;
    }

    for (size_t j = 0; j < n_qubits; ++j) {
        if (j != *pivot && tab.destabilizer(j).is_x_set(col)) {
            tab.cx(*pivot, j);
        }
    }
    if (tab.stabilizer(*pivot).is_z_set(col)) {
        tab.s(*pivot);
    }
    tab.h(*pivot);
    return true;
}

StabilizerTableau reverse_diagonalization(std::vector<TMergeGate> const& gates) {
    auto const n_qubits = gates.empty() ? 0ul : [&] {
        size_t max_q = 0;
        for (auto const& g : gates) {
            max_q = std::max(max_q, std::max(g.q0, g.q1));
        }
        return max_q + 1;
    }();

    StabilizerTableau tab{n_qubits};
    for (auto const& g : gates) {
        if (g.kind == TMergeGate::Kind::t) {
            continue;
        }
        prepend_clifford(tab, g, false);
    }

    for (auto it = gates.rbegin(); it != gates.rend(); ++it) {
        if (it->kind == TMergeGate::Kind::t) {
            diagonalize_pauli_rotation(tab, it->q0);
            continue;
        }
        prepend_clifford(tab, *it, it->kind == TMergeGate::Kind::sdg);
    }
    return tab;
}

TMergePlan run_merge(
    std::vector<TMergeGate> const& gates,
    size_t n_qubits,
    std::vector<bool> const& rank,
    bool fast_t_merge) {
    auto w = rank;
    std::vector<int> r(rank.size(), 1);
    StabilizerTableau tab{n_qubits};
    std::vector<PauliProduct> pauli_products;
    std::unordered_map<PauliAxisKey, std::vector<std::pair<size_t, bool>>, PauliAxisKeyHash> axis_map;

    size_t t_index = 0;
    for (auto const& gate : gates) {
        if (gate.kind != TMergeGate::Kind::t) {
            prepend_clifford(tab, gate, false);
            continue;
        }

        auto const q    = gate.q0;
        auto const pauli = tab.stabilizer(q);
        auto const key   = pauli_axis_key(pauli);
        bool merge       = axis_map.contains(key);
        std::vector<std::pair<size_t, bool>> stack;

        if (merge) {
            stack = std::move(axis_map[key]);
            axis_map.erase(key);
            auto const [prev_index, prev_sign] = stack.back();
            stack.pop_back();

            for (size_t i = prev_index + 1; i < t_index; ++i) {
                if (!rank[i] || pauli.is_commutative(pauli_products[i])) {
                    continue;
                }
                if (fast_t_merge) {
                    if (r[i] == 1) {
                        merge = false;
                        break;
                    }
                    for (size_t j = i + 1; j < t_index; ++j) {
                        if (w[j] && r[j] == 1 && !pauli.is_commutative(pauli_products[j])) {
                            merge = false;
                            break;
                        }
                    }
                } else {
                    merge = false;
                }
                break;
            }

            if (merge) {
                if (fast_t_merge && rank[prev_index]) {
                    for (size_t i = prev_index + 1; i < t_index; ++i) {
                        w[i] = true;
                    }
                }
                if (fast_t_merge) {
                    w[prev_index] = false;
                }
                r[prev_index] = 0;
                r[t_index]    = 0;
                if (prev_sign == pauli.is_neg()) {
                    r[t_index] = 2;
                    tab.prepend_s(q);
                }
            }

            if (!merge) {
                stack.emplace_back(prev_index, prev_sign);
            }
        }

        if (!merge) {
            stack.emplace_back(t_index, pauli.is_neg());
            axis_map.emplace(key, std::move(stack));
        }

        pauli_products.push_back(pauli);
        ++t_index;
    }

    TMergePlan plan;
    plan.t_outcomes.reserve(rank.size());
    for (auto it = r.rbegin(); it != r.rend(); ++it) {
        switch (*it) {
            case 0:
                plan.t_outcomes.push_back(TMergeOutcome::remove);
                break;
            case 1:
                plan.t_outcomes.push_back(TMergeOutcome::keep);
                break;
            case 2:
                plan.t_outcomes.push_back(TMergeOutcome::replace_with_s);
                break;
            default:
                plan.t_outcomes.push_back(TMergeOutcome::keep);
                break;
        }
    }
    return plan;
}

}  // namespace

PauliAxisKey pauli_axis_key(PauliProduct const& pauli) {
    PauliAxisKey key;
    auto const n = pauli.n_qubits();
    key.bits.resize(2 * n);
    for (size_t i = 0; i < n; ++i) {
        key.bits[i]       = pauli.is_z_set(i);
        key.bits[i + n]   = pauli.is_x_set(i);
    }
    return key;
}

std::vector<bool> rank_vector(std::vector<TMergeGate> const& gates, size_t n_qubits) {
    auto tab = reverse_diagonalization(gates);
    std::vector<bool> rank;
    rank.reserve(gates.size());

    for (auto const& gate : gates) {
        if (gate.kind != TMergeGate::Kind::t) {
            prepend_clifford(tab, gate, false);
            continue;
        }
        rank.push_back(diagonalize_pauli_rotation(tab, gate.q0));
    }
    return rank;
}

TMergePlan bb_merge_decisions(
    std::vector<TMergeGate> const& gates,
    size_t n_qubits,
    std::vector<bool> const& rank) {
    return run_merge(gates, n_qubits, rank, false);
}

TMergePlan fast_t_merge_decisions(
    std::vector<TMergeGate> const& gates,
    size_t n_qubits,
    std::vector<bool> const& rank) {
    return run_merge(gates, n_qubits, rank, true);
}

}  // namespace qsyn::experimental
