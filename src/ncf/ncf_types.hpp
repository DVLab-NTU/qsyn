/**
 * @file ncf_types.hpp
 * @brief Shared types for NcfProgram (costs, graphs, reports, junctions, schedules).
 *        Printing: ncf_report.cpp — CLI: ncf print --summary|--cost
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "tableau/pauli_rotation.hpp"

namespace qsyn::experimental {

enum class NcfBlockKind { singleton, anti_pair, anti_triple, commuting_singleton, unknown };

struct NcfCxEdge {
    size_t control = 0;
    size_t target  = 0;
    size_t seq_index = 0;
};

struct NcfCxGraph {
    enum class Topology { star, ladder, general };
    std::vector<NcfCxEdge> edges;
    Topology topology = Topology::general;
    std::optional<size_t> root;
    std::optional<size_t> ancilla;
    std::vector<std::pair<size_t, size_t>> self_inverse_pairs;
};

struct NcfGateCost {
    size_t cx = 0, rz = 0, h = 0, s = 0, sdg = 0, x = 0, y = 0, z = 0;
    size_t clifford_total = 0;
    size_t non_clifford   = 0;
    size_t total_gates    = 0;
    size_t depth          = 0;

    void print(std::string const& label) const;
};

struct NcfFusionReport {
    size_t n_pauli_terms = 0;
    size_t n_blocks      = 0;
    NcfGateCost before;
    NcfGateCost after;
    std::string synth_before = "naive";
    std::string synth_after  = "ncf";
    void print() const;
};

struct NcfJunction {
    size_t left = 0;
    size_t right = 0;
    bool exactly_identity = false;
    size_t cx_count = 0;
    size_t cx_saved_if_merged = 0;
    size_t partial_cx_saved = 0;
    std::vector<std::pair<NcfCxEdge, NcfCxEdge>> cancel_pairs;
    double structural_overlap = 0.0;
};

struct NcfScheduleCandidate {
    std::string mode;
    std::vector<size_t> order;
    size_t predicted_cx = 0;
    size_t seam_cx_saved = 0;
    bool commute_valid  = true;
};

}  // namespace qsyn::experimental
