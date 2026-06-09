/**
 * @file t_merge.hpp
 * @brief BBMerge / FastTMerge core
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tableau/pauli_rotation.hpp"

namespace qsyn::experimental {

enum class TMergeOutcome : std::uint8_t {
    remove           = 0,
    keep             = 1,
    replace_with_s   = 2,
    replace_with_sdg = 3,
};

struct TMergeGate {
    enum class Kind : std::uint8_t { h, x, z, sdg, s, cx, t };
    Kind kind;
    size_t q0   = 0;
    size_t q1   = 0;
    bool t_neg  = false;
};

struct TMergePlan {
    std::vector<TMergeOutcome> t_outcomes;
};

struct PauliAxisKey {
    sul::dynamic_bitset<> bits;

    bool operator==(PauliAxisKey const& other) const { return bits == other.bits; }
};

PauliAxisKey pauli_axis_key(PauliProduct const& pauli);

std::vector<bool> rank_vector(std::vector<TMergeGate> const& gates, size_t n_qubits);

TMergePlan bb_merge_decisions(
    std::vector<TMergeGate> const& gates,
    size_t n_qubits,
    std::vector<bool> const& rank);

TMergePlan fast_t_merge_decisions(
    std::vector<TMergeGate> const& gates,
    size_t n_qubits,
    std::vector<bool> const& rank);

}  // namespace qsyn::experimental
