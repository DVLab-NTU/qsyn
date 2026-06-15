/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#pragma once

#include "./tableau.hpp"
#include "tableau/optimize/reorder_smt.hpp"
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
#include <string_view>
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
    std::vector<TouchingTermComparison> per_qubit_comparisons;
};

std::vector<SignatureComparisonResult> compare_pp(
    std::vector<std::vector<PauliRotation>> const& unified_pr_history,
    std::vector<PauliRotation> const& pr_pmc_ij,
    std::vector<size_t> const& x_qubits);

struct BlockingSignatureInfo {
    SignatureTensor signature;
    bool ancilla_in_signature = false;
    bool has_ancilla_x_overlap = false;
};

BlockingSignatureInfo analyze_blocking_signature(
    std::vector<PauliRotation> const& pr_blocking,
    std::vector<size_t> const& x_qubits,
    size_t ancilla_qubit);

/** PMC degadgetizable iff no PR column blocks ancilla against any PMC x-qubit. */
struct PmcPrBlockingAnalysis {
    size_t reference_qubit = 0;
    size_t ancilla_qubit = 0;
    std::vector<size_t> x_qubits;
    std::vector<PauliRotation> pr_blocking;
    bool is_degadgetizable = false;
};

std::vector<size_t> extract_pmc_x_qubits(ClassicalControlTableau const& pmc);
std::vector<PauliRotation> find_pr_blocking_rotations(
    std::vector<PauliRotation> const& pr,
    size_t ancilla_qubit,
    std::vector<size_t> const& x_qubits);
PmcPrBlockingAnalysis analyze_pmc_pr_blocking(
    ClassicalControlTableau const& pmc,
    std::vector<PauliRotation> const& unified_pr);

/** PR columns split for PMC commute (x-qubit Z vs ancilla Z); see signature_check.cpp. */
struct UnifiedPrPmcSplit {
    std::vector<PauliRotation> commuting;
    std::vector<PauliRotation> commuting_rest;
    std::vector<PauliRotation> ancilla_group;
    std::vector<PauliRotation> ancilla_rest;
    std::vector<PauliRotation> blocking;
};

UnifiedPrPmcSplit split_unified_pr_for_pmc(
    std::vector<PauliRotation> const& unified_pr,
    size_t ancilla_qubit,
    std::vector<size_t> const& x_qubits);

/** After swap_along_test(block, pr_idx, 1), count Z on ancilla in the group columns. */
struct PrGroupPushFrontZCheck {
    std::string group_name;
    size_t group_size           = 0;
    size_t z_on_ancilla_count   = 0;
    bool all_z_on_ancilla       = false;
};

PrGroupPushFrontZCheck check_pr_group_z_on_ancilla_after_push_front(
    std::vector<PauliRotation> const& group,
    std::vector<PauliRotation> const& rest,
    Tableau const& tableau,
    size_t pr_idx,
    size_t ancilla_qubit,
    std::string_view group_name);

/** PR columns grouped by Z support on PMC x-lines vs ancilla (for push-front experiments). */
struct PrColumnThreeGroupSplit {
    std::vector<PauliRotation> x_only;
    std::vector<PauliRotation> ancilla_only;
    std::vector<PauliRotation> x_and_ancilla;
    std::vector<PauliRotation> other;
};

PrColumnThreeGroupSplit classify_pr_three_column_groups(
    std::vector<PauliRotation> const& unified_pr,
    size_t ancilla_qubit,
    std::vector<size_t> const& x_qubits);

struct PrSplitPushFrontTestReport {
    size_t ancilla_qubit = 0;
    std::vector<size_t> x_qubits;
    PrGroupPushFrontZCheck x_only_check;
    PrGroupPushFrontZCheck ancilla_only_check;
    PrGroupPushFrontZCheck x_and_ancilla_check;
};

/** Push each column group to circuit front (swap_along_test to idx 1); log Z@ancilla counts. */
void test_pr_column_groups_push_front_z_on_ancilla(
    Tableau const& tableau,
    std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr,
    std::string_view circuit_label);

/** Load adder_8.qc, run gadgetize + FastTODD, then run the three-group push-front test. */
bool test_adder_8_pr_push_front_z_on_ancilla_after_topt(
    std::optional<std::filesystem::path> phase_export_path = std::nullopt);

/** Column block w.r.t. one PMC ancilla and its x-qubits (Z support, not phase angle). */
enum class PrColumnBlockKind : std::uint8_t {
    x_only,
    ancilla_only,
    x_and_ancilla,
    other
};

char const* pr_column_block_kind_str(PrColumnBlockKind kind);

PrColumnBlockKind pr_column_block_kind_for_pmc(
    PauliRotation const& rotation,
    size_t ancilla_qubit,
    std::vector<size_t> const& x_qubits);

/** Whole unified PR: swap_along_test(pr_idx, 1) with no column split / reorder. */
struct PrWholeBlockPhaseExport {
    size_t pr_idx = 0;
    std::vector<PauliRotation> before;
    std::vector<PauliRotation> after;
};

std::optional<size_t> find_unified_pr_block_index(Tableau const& tableau);

PrWholeBlockPhaseExport export_whole_pr_phases_after_swap_to_front(Tableau const& tableau);

/** Per (ancilla, column): phases + block kind before/after whole-PR swap to idx 1. */
struct PrAncillaColumnBlockRow {
    size_t ancilla_qubit         = 0;
    size_t column_index          = 0;
    std::string phase_before;
    std::string phase_after;
    std::string pauli_before;
    std::string pauli_after;
    PrColumnBlockKind block_before = PrColumnBlockKind::other;
    PrColumnBlockKind block_after  = PrColumnBlockKind::other;
    bool z_on_ancilla_before       = false;
    bool z_on_ancilla_after        = false;
};

bool write_pr_whole_swap_phase_export_csv(
    Tableau const& tableau,
    std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr,
    PrWholeBlockPhaseExport const& pr_export,
    std::filesystem::path const& out_path);

void log_pr_block_summary_per_ancilla(
    Tableau const& tableau,
    std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr,
    PrWholeBlockPhaseExport const& pr_export);

/** Per-gadget PR pid lists for minimal SAT export (whole-PR front commute). */
struct GadgetPrBlockLists {
    size_t gid                   = 0;
    size_t ancilla_qubit         = 0;
    /** PR column i after swap_along_test(pr_idx, 1) has Z on this gadget ancilla; pid = G + i. */
    std::vector<size_t> block_left;
    /** Unified PR column i before commute has Z on this gadget ancilla; pid = G + i. */
    std::vector<size_t> block_right;
    /**
     * True iff no column is in both block_left and block_right for this gadget,
     * and the gadget has no overlap with any gadget on a lower ancilla qubit.
     */
    bool degadgetizable = true;
};

struct SatSignatureExport {
    size_t qubit_count   = 0;
    size_t ancilla_count = 0;
    size_t pauli_count   = 0;
    /** Gadget gids in fixed schedule order (ancilla index ascending). */
    std::vector<size_t> gadget_order;
    /** Indexed by gid; size = |G|. */
    std::vector<GadgetPrBlockLists> blocks_by_gid;
};

/** Build SAT block_left/block_right from PR and PR after swap_along_test(pr_idx, 1). */
SatSignatureExport compute_sat_signature_blocks(Tableau const& tableau);

/**
 * Gadget overlap from PR bridge columns: for column i and gadgets a < b,
 * block_right(i,a) & block_left(i,b) implies a and b must overlap in schedule.
 */
struct GadgetOverlapConstraints {
    size_t gadget_count = 0;
    /** Sorted unique overlapping gadget gids for each gid. */
    std::vector<std::vector<size_t>> overlap_neighbors;
    /** Bridge PR column indices per (gid, other_gid). */
    std::vector<std::unordered_map<size_t, std::vector<size_t>>> bridge_columns_by_neighbor;
    /**
     * Per PR column: max of
     *   (block_left gadgets at/after min-ancilla block_right gadget, inclusive),
     *   (block_right gadgets at/before max-ancilla block_left gadget, inclusive).
     */
    std::vector<size_t> overlap_gadget_count_by_column;

    [[nodiscard]] size_t overlapping_pair_count() const;
    [[nodiscard]] size_t max_overlap_among_columns() const;
    [[nodiscard]] size_t max_overlap_among_gadgets() const;
};

GadgetOverlapConstraints compute_gadget_overlap_constraints(SatSignatureExport const& sig);

/** Gadget gids excluded from degadgetization: overlap a gadget on a lower ancilla qubit. */
std::unordered_set<size_t> collect_lower_ancilla_overlap_excluded_gids(
    std::vector<size_t> const&      gadget_ancilla_qubit,
    GadgetOverlapConstraints const& overlap);

void log_sat_reorder_preprocess(SatSignatureExport const& sig);

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

/** Rewrite one merged Pauli-rotation block into Z-basis using only H and S†. */
void z_basisify_rotations_h_s_only(Tableau& tableau);

// Push non-H part of each intermediate Clifford through subsequent Pauli Rotations
// to the end; implemented in ./optimize/hadamard_gadgetize.cpp
void push_z_stabilizers(Tableau& tableau);

std::unordered_map<size_t, PmcUnifiedPrRelation> minimize_internal_hadamards_n_gadgetize(Tableau& tableau);

/** Run block-wise ancillary-T optimization on internal PR-ST-PR windows. */
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

/** Validate CCC/PMC/PR layout for degadgetization. */
CircuitStructureInfo inspect_degadgetization_structure(Tableau const& tableau);
/** Build constraint graph, reorder PRs/CCC, degadgetize graph-active gadgets. */
void reorder_n_degadgetize(Tableau& tableau);

/** Move PMCs, validate, and degadgetize the given ancilla candidates. Returns count applied. */
size_t hadamard_degadgetize(Tableau& tableau,
                            std::vector<size_t> const& ancilla_candidates,
                            std::vector<size_t>* removed_ancillae = nullptr);

bool sat_reorder_apply_ordering(Tableau& tableau,
                                ParsedGadgetOrdering const& ord);
/** Build qsyn SAT signature → native Z3 SMT schedule → apply; on failure leaves the tableau unchanged. */
void sat_reorder(Tableau& tableau);

void check_redundant_ancilla(Tableau& tableau);
bool run_commute_test_from_file(std::filesystem::path const& txt_path);
void move_pmcs_with_reduced_PR(Tableau const& tableau, std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr);
// Constraint graph for topological ordering constraints
struct ConstraintGraph {
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

    /** Detach PR<->gadget edges; gadget-gadget ordering edges are kept. */
    void disconnect_gadget_pr_edges(size_t gadget_idx);

    /** Gadget topo order plus PR columns grouped by slot index (0..G). */
    struct ReorderPlan {
        std::vector<size_t> gadget_order;
        std::vector<std::vector<size_t>> slot_to_prs;
    };

    /** Full-graph topo sort; returns nullopt if a cycle remains. */
    std::optional<ReorderPlan> topological_sort() const;

    /** Break PR-involved cycles by disconnecting PR edges; returns gadgets detached. */
    size_t break_cycles();
};

/** Build gadget/PR constraint graph from tableau and eligibility set. */
ConstraintGraph build_constraint_graph(
    Tableau const& tableau,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    std::unordered_set<size_t> const& degadgetizable_gadget_indices);

/** Export validated H-gadget (CCC, PMC) pairs from a tableau. */
std::vector<ConstraintGraph::HadamardGadgetPair> export_hadamard_gadget_pairs(Tableau& tableau);

// Classical T optimization: minimize internal H, gadgetize, commute classical, and optimize with FastTODD
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

struct TohpePhasePolynomialOptimizationStrategy : public PhasePolynomialOptimizationStrategy {
    std::pair<StabilizerTableau, Polynomial> optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const override;
};

/** FastTODD: loop { full TOHPE, one fast_todd step } until no move; Clifford phase merged in optimize_phase_polynomial_with_classical. */
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

/** Naively partition the polynomial until matroid independence is violated. */
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
