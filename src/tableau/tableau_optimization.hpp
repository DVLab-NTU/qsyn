/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#pragma once

#include "./tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace qsyn {

namespace experimental {

void full_optimize(Tableau& tableau);

void collapse(Tableau& tableau);
void commute_classical(Tableau& tableau);
void commute_and_merge_rotations(Tableau& tableau);
void collapse_with_classical(Tableau& tableau);

void remove_identities(std::vector<PauliRotation>& rotation);
void remove_identities(Tableau& tableau);

void absorb_clifford_rotations(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations);
void properize(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations);

void properize(Tableau& tableau);

void merge_rotations(std::vector<PauliRotation>& rotation);
void merge_rotations(Tableau& tableau);

// hadamard minimization
// implemented in ./optimize/internal_h_opt.cpp

void minimize_internal_hadamards(Tableau& tableau);
void minimize_internal_hadamards_n_gadgetize(Tableau& tableau);

// H gadgetization - replaces H gates with gadgets using ancilla qubits and measurements
std::pair<Tableau, StabilizerTableau> minimize_hadamards_n_gadgetize(Tableau tableau, StabilizerTableau context);

// Commute classical operations and collapse

// Re-establish CCC-PMC pairing after moves (internal use)
void reestablish_hadamard_gadget_pairing(Tableau& tableau);

struct CircuitStructureInfo {
    size_t ccc_count;
    size_t pr_column_count;
    bool is_valid;
};

CircuitStructureInfo properize_for_degadgetization(Tableau& tableau);
void reorder_n_degadgetize(Tableau& tableau);
// Constraint graph for topological ordering constraints
struct ConstraintGraph {
    // H-gadget pair structure for degadgetization
    struct HadamardGadgetPair {
        size_t ccc_index;              // Index of CCC in tableau
        size_t pmc_index;              // Index of PMC in tableau
        size_t ancilla_qubit;          // Ancilla qubit (b)
        std::optional<size_t> reference_qubit;  // Reference qubit (a)
    };
    
    // PR information structure
    struct PRInfo {
        size_t tableau_index;
        size_t pr_vector_index;
        size_t global_pr_index;
        PauliRotation const* pr_ptr;
    };
    
    // Vertex types
    enum class VertexType {
        GADGET,  // Represents a gadget (CCC)
        PR       // Represents a Pauli rotation
    };
    
    struct Vertex {
        VertexType type;
        size_t id;  // Sequential vertex ID (0, 1, 2, ...) - gadgets first, then PRs
        HadamardGadgetPair hadamard_gadget_pair;
        PRInfo pr_info;
        bool removed;  // Whether this vertex is removed (for cycle breaking)
        
        Vertex(VertexType t, size_t i) : type(t), id(i), removed(false) {}
        Vertex(VertexType t, size_t i, HadamardGadgetPair const& hg) 
            : type(t), id(i), hadamard_gadget_pair(hg), removed(false) {}
        Vertex(VertexType t, size_t i, PRInfo const& pr) 
            : type(t), id(i), pr_info(pr), removed(false) {}
    };
    
    std::vector<Vertex> vertices;
    std::vector<std::vector<size_t>> outgoing_edges;  // outgoing_edges[i] = list of vertex indices that vertex i points to
    std::vector<std::vector<size_t>> incoming_edges;  // incoming_edges[i] = list of vertex indices that point to vertex i
    
    // Unified vertex creation: auto-detects type from input
    // Returns the sequential vertex ID (same as vertex index in vertices array)
    size_t create_vertex(HadamardGadgetPair const& hg);
    size_t create_vertex(PRInfo const& pr);
    
    // Add a directed edge from vertex u to vertex v
    void add_edge(size_t u, size_t v);
    
    // Get topological ordering (considering removed vertices)
    std::optional<std::vector<size_t>> topological_sort() const;
    
    // Break cycles by removing HadamardGadget vertices
    // Returns the number of vertices removed
    size_t break_cycles();
};

// Build constraint graph from tableau
ConstraintGraph build_constraint_graph(Tableau& tableau);

// Export all H-gadget pairs from tableau
std::vector<ConstraintGraph::HadamardGadgetPair> export_hadamard_gadget_pairs(Tableau& tableau);

// Classical T optimization: minimize internal H, gadgetize, commute classical, and optimize with FastTodd
void minimize_ancillary_t_opt(Tableau& tableau);

struct PhasePolynomialOptimizationStrategy {
    using Polynomial                               = std::vector<PauliRotation>;
    virtual ~PhasePolynomialOptimizationStrategy() = default;

    virtual std::pair<StabilizerTableau, Polynomial> optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const = 0;
};

struct ToddPhasePolynomialOptimizationStrategy : public PhasePolynomialOptimizationStrategy {
    std::pair<StabilizerTableau, Polynomial> optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const override;
};

struct FastToddPhasePolynomialOptimizationStrategy : public PhasePolynomialOptimizationStrategy {
    std::pair<StabilizerTableau, Polynomial> optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const override;
};

void optimize_phase_polynomial(StabilizerTableau& clifford, std::vector<PauliRotation>& polynomial, PhasePolynomialOptimizationStrategy const& strategy);
void optimize_phase_polynomial(Tableau& tableau, PhasePolynomialOptimizationStrategy const& strategy);
void optimize_phase_polynomial_with_classical(Tableau& tableau, PhasePolynomialOptimizationStrategy const& strategy);

struct MatroidPartitionStrategy {
    using Polynomial                    = std::vector<PauliRotation>;
    using Partitions                    = std::vector<Polynomial>;
    virtual ~MatroidPartitionStrategy() = default;

    virtual Partitions partition(Polynomial const& polynomial, size_t num_ancillae) const = 0;

    bool is_independent(Polynomial const& polynomial, size_t num_ancillae) const;
};

/**
 * @brief partitions the given polynomial by naively picking terms until the matroid independence condition is violated
 *
 */
struct NaiveMatroidPartitionStrategy : public MatroidPartitionStrategy {
    Partitions partition(Polynomial const& polynomial, size_t num_ancillae) const override;
};

inline bool is_phase_polynomial(std::vector<PauliRotation> const& polynomial) noexcept {
    return std::ranges::all_of(polynomial, [](PauliRotation const& rotation) { return rotation.is_diagonal(); }) &&
           std::ranges::all_of(polynomial, [n_qubits = polynomial.front().n_qubits()](PauliRotation const& rotation) { return rotation.n_qubits() == n_qubits; });
}

std::optional<std::vector<std::vector<PauliRotation>>> matroid_partition(std::vector<PauliRotation> const& polynomial, MatroidPartitionStrategy const& strategy, size_t num_ancillae = 0);
std::optional<Tableau> matroid_partition(Tableau const& tableau, MatroidPartitionStrategy const& strategy, size_t num_ancillae = 0);

}  // namespace experimental

}  // namespace qsyn
