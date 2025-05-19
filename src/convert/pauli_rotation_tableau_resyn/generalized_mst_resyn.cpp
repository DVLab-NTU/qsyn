#include <gsl/narrow>
#include <stack>
#include <tl/adjacent.hpp>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>

#include "convert/tableau_to_qcir.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/graph/digraph.hpp"
#include "util/graph/minimum_spanning_arborescence.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::tableau {

namespace {

bool is_valid(PauliRotation const& rotation) {
    if (!rotation.is_diagonal()) {
        return false;
    }

    auto num_z = 0ul;
    for (auto i: std::views::iota(0ul, rotation.n_qubits())) {
        if (rotation.pauli_product().is_z_set(i)) {
            num_z++;
        }
    }
    return num_z == 1;
}

size_t qubit_hamming_weight(
    PauliRotation const& rotation) {
    auto const num_qubits = rotation.n_qubits();
    auto num_qubit_has_ones         = 0ul;
    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (rotation.pauli_product().is_z_set(i) || rotation.pauli_product().is_x_set(i)) {
            num_qubit_has_ones++;
        }
    }
    return num_qubit_has_ones;
}

dvlab::Digraph<size_t, int> get_dependency_graph(std::vector<PauliRotation> const& rotations, bool check = false) {
    size_t const num_rotations = rotations.size();
    dvlab::Digraph<size_t, int> dag{num_rotations};
    
    // Build dependency edges based on non-commutative pairs
    // size_t counter = 0;
    for (auto i : std::views::iota(0ul, num_rotations)) {
        for (auto j : std::views::iota(i+1, num_rotations)) {
            // if rotations[i] and rotations[j] don't commute, add an edge from i to j
            if (!is_commutative(rotations[i], rotations[j])) {
                dag.add_edge(i, j);
            }
        }
    }
    return dag;
}

}  // namespace

std::optional<PartialSynthesisResult>
GeneralizedMstSynthesisStrategy::partial_synthesize(
    PauliRotationTableau const& rotations) const {
    auto const num_rotations = rotations.size();
    auto const dag = get_dependency_graph(rotations);

    spdlog::info("PMST is not implemented yet");
    return std::nullopt;
}

std::optional<qcir::QCir>
GeneralizedMstSynthesisStrategy::synthesize(
    PauliRotationTableau const& rotations) const {

    spdlog::info("PMST is not implemented yet");
    return std::nullopt;
}

}  // namespace qsyn::tableau