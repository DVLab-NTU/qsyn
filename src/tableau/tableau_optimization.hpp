/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#pragma once

#include "./tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace qsyn {

namespace experimental {

void full_optimize(Tableau& tableau);

void collapse(Tableau& tableau);
void commute_classical(Tableau& tableau);
struct PmcUnifiedPrRelation {
    std::vector<size_t> x_qubits;
    std::vector<std::vector<PauliRotation>> unified_pr_history;
};

struct SignatureTensor {
    struct PairTerm {
        size_t i{};
        size_t j{};
        static PairTerm canonical(size_t a, size_t b) {
            return (a <= b) ? PairTerm{a, b} : PairTerm{b, a};
        }
        bool operator==(PairTerm const& rhs) const { return i == rhs.i && j == rhs.j; }
    };

    struct PairTermHash {
        size_t operator()(PairTerm const& t) const {
            return std::hash<size_t>{}(t.i) ^ (std::hash<size_t>{}(t.j) << 1);
        }
    };

    struct TripleTerm {
        size_t i{};
        size_t j{};
        size_t k{};
        static TripleTerm canonical(size_t a, size_t b, size_t c) {
            auto sorted = std::array<size_t, 3>{a, b, c};
            std::ranges::sort(sorted);
            return {sorted[0], sorted[1], sorted[2]};
        }
        bool operator==(TripleTerm const& rhs) const { return i == rhs.i && j == rhs.j && k == rhs.k; }
    };

    struct TripleTermHash {
        size_t operator()(TripleTerm const& t) const {
            return std::hash<size_t>{}(t.i) ^ (std::hash<size_t>{}(t.j) << 1) ^ (std::hash<size_t>{}(t.k) << 2);
        }
    };

    size_t n_qubits = 0;
    std::vector<uint8_t> linear_mod8;
    std::unordered_map<PairTerm, uint8_t, PairTermHash> quadratic_mod4;
    std::unordered_set<TripleTerm, TripleTermHash> cubic_mod2;

    std::unordered_map<size_t, std::unordered_set<PairTerm, PairTermHash>> quadratic_by_qubit;
    std::unordered_map<size_t, std::unordered_set<TripleTerm, TripleTermHash>> cubic_by_qubit;

    struct TouchingTerms {
        std::optional<uint8_t> linear_mod8;
        std::vector<std::pair<PairTerm, uint8_t>> quadratic_terms;
        std::vector<TripleTerm> cubic_terms;
    };

    bool equivalent(SignatureTensor const& other) const;
    TouchingTerms get_touching(size_t qubit) const;
    std::vector<std::pair<PairTerm, uint8_t>> get_quadratic_terms_touching(size_t qubit) const;
    std::vector<TripleTerm> get_cubic_terms_touching(size_t qubit) const;
};

SignatureTensor get_signature(std::vector<PauliRotation> const& rotations);
struct TouchingTermComparison {
    size_t qubit = 0;
    SignatureTensor::TouchingTerms unified_terms;
    SignatureTensor::TouchingTerms reduced_terms;
    bool equivalent = false;
};

struct SignatureComparisonResult {
    size_t history_index = 0;
    bool full_signature_equivalent = false;
    std::vector<TouchingTermComparison> per_qubit_comparisons;
};

std::vector<SignatureComparisonResult> compare_pp(
    std::vector<std::vector<PauliRotation>> const& unified_pr_history,
    std::vector<PauliRotation> const& pr_pmc_ij,
    std::vector<size_t> const& x_qubits);

std::unordered_map<size_t, PmcUnifiedPrRelation> commute_and_merge_rotations(Tableau& tableau);
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

/**
 * @brief Rewrite one merged Pauli-rotation block into Z-basis using only H and S†.
 */
void z_basisify_rotations_h_s_only(Tableau& tableau);

// Push non-H part of each intermediate Clifford through subsequent Pauli Rotations
// to the end; implemented in ./optimize/hadamard_gadgetize.cpp
void push_z_stabilizers(Tableau& tableau);

std::unordered_map<size_t, PmcUnifiedPrRelation> minimize_internal_hadamards_n_gadgetize(Tableau& tableau);

/**
 * @brief Run block-wise ancillary-T optimization on internal PR-ST-PR windows.
 */
void blockwise_gadgetize_optimize(Tableau& tableau);

// Internal helper called by the wrapper.
void blockwise_gadgetize(Tableau& tableau);

// H gadgetization - replaces H gates with gadgets using ancilla qubits and measurements
std::pair<Tableau, StabilizerTableau> minimize_hadamards_n_gadgetize(Tableau tableau, StabilizerTableau context);

// Commute classical operations and collapse

struct CircuitStructureInfo {
    size_t ccc_count;
    size_t pr_column_count;
    bool is_valid;
};

CircuitStructureInfo inspect_degadgetization_structure(Tableau const& tableau);
void reorder_n_degadgetize(Tableau& tableau);

bool sat_reorder_export(Tableau& tableau, std::filesystem::path const& work_dir);
bool sat_reorder_run_solver(std::filesystem::path const& work_dir, std::filesystem::path const& sat_formulation_py);
/** Reorder PR blocks and CCCs per gadget_ordering; does not degadgetize (circuit stays gadgetized). */
bool sat_reorder_apply(Tableau& tableau, std::filesystem::path const& ordering_path);
/** Export → Z3 (sat_formulation.py) → apply; on failure falls back to full reorder_n_degadgetize (with degadgetize). */
void sat_reorder(Tableau& tableau);

void check_redundant_ancilla(Tableau& tableau);
bool run_commute_test_from_file(std::filesystem::path const& txt_path);
void move_pmcs_with_reduced_PR(Tableau const& tableau, std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr);
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

// Build constraint graph from tableau.
ConstraintGraph build_constraint_graph(Tableau& tableau);

// Export all H-gadget pairs from tableau
std::vector<ConstraintGraph::HadamardGadgetPair> export_hadamard_gadget_pairs(Tableau& tableau);

/** Diagonal-phase PR through CCC: swap Pauli support on (reference, ancilla); refresh CZ flag. Defined in optimize/minimize_ancilla.cpp. */
void swap_gadget_phase_slots(PauliRotation& r, size_t reference, size_t ancilla);

// Classical T optimization: minimize internal H, gadgetize, commute classical, and optimize with FastTodd
void minimize_ancillary_t_opt(Tableau& tableau, std::optional<std::string> export_filename = std::nullopt);
void minimize_ancillary_t_opt_with_degadgetization(Tableau& tableau, std::optional<std::string> export_filename = std::nullopt);

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

/**
 * @brief Reference FastTODD strategy kept as an explicit selectable optimization mode.
 */
struct FastToddReferencePhasePolynomialOptimizationStrategy : public PhasePolynomialOptimizationStrategy {
    std::pair<StabilizerTableau, Polynomial> optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const override;
};

struct TohpeOnlyPhasePolynomialOptimizationStrategy : public PhasePolynomialOptimizationStrategy {
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
    if (polynomial.empty()) {
        return true;
    }
    size_t const n_qubits = polynomial.front().n_qubits();
    return std::ranges::all_of(polynomial, [](PauliRotation const& rotation) { return rotation.is_diagonal(); }) &&
           std::ranges::all_of(polynomial, [n_qubits](PauliRotation const& rotation) { return rotation.n_qubits() == n_qubits; });
}

std::optional<std::vector<std::vector<PauliRotation>>> matroid_partition(std::vector<PauliRotation> const& polynomial, MatroidPartitionStrategy const& strategy, size_t num_ancillae = 0);
std::optional<Tableau> matroid_partition(Tableau const& tableau, MatroidPartitionStrategy const& strategy, size_t num_ancillae = 0);

}  // namespace experimental

}  // namespace qsyn
