/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <optional>
#include <set>

// #include "device/device.hpp"
// #include "duostra/duostra.hpp"
#include "qcir/qcir_gate.hpp"
#include "qsyn/qsyn_type.hpp"
#include "spdlog/common.h"
#include "util/boolean_matrix.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"

namespace qsyn {

namespace qcir {
class QCir;
}
namespace zx {
class ZXGraph;
}

namespace extractor {

struct ExtractorConfig {
    bool sort_frontier;         // sort frontier by the qubit IDs
    bool sort_neighbors;        // sort neighbors by the vertex IDs
    bool permute_qubits;        // synthesize permutation circuits at
                                // the end of extraction
    bool filter_duplicate_cxs;  // filters duplicate CXs during extraction
    bool reduce_czs;            // tries to reduce the number of CZs by
                                // feeding them into the biadjacency matrix
    bool dynamic_order;         // dynamically decides the order of gadget
                                // removal and CZ extraction
    size_t block_size;          // the block size for block Gaussian
                                // elimination. Only used in optimization
                                // level 0
    size_t optimize_level;      // the strategy for biadjacency elimination.
                                // 0: fixed block size, 1: all block sizes,
                                // 2: greedy reduction, 3: best of 1 and 2
    float pred_coeff;           // hyperparameter for the dynamic extraction
                                // routine. If
                                // #CZs > #(edge reduced) * coeff,
                                // eagerly extract CZs
};

class Extractor {
public:
    using Target      = std::unordered_map<size_t, size_t>;
    using ConnectInfo = std::vector<std::set<size_t>>;
    using Overlap     = std::pair<std::pair<size_t, size_t>, std::vector<size_t>>;

    Extractor(
        zx::ZXGraph* graph,
        ExtractorConfig config,
        qcir::QCir* qcir = nullptr,
        bool random      = false);

    qcir::QCir* get_logical() { return _logical_circuit; }

    void initialize();
    qcir::QCir* extract();
    bool extraction_loop(std::optional<size_t> max_iter = std::nullopt);
    bool remove_gadget(bool check = false);
    bool biadjacency_eliminations(bool check = false);
    void column_optimal_swap();
    void extract_singles();
    bool extract_czs(bool check = false);
    void extract_cxs();
    size_t extract_hadamards_from_matrix(bool check = false);
    void clean_frontier();
    void permute_qubits();

    void update_neighbors();
    void update_graph_by_matrix(qsyn::zx::EdgeType et = qsyn::zx::EdgeType::hadamard);
    void update_matrix();

    void prepend_series_gates(std::vector<qcir::QCirGate> const& logical /*, std::vector<Operation> const& physical = {} */);
    void prepend_swap_gate(QubitIdType q0, QubitIdType q1, qcir::QCir* circuit);
    bool frontier_is_cleaned();
    bool axel_in_neighbors();
    bool contains_single_neighbor();
    void print_cxs() const;
    void print_frontier(spdlog::level::level_enum lvl = spdlog::level::off) const;
    void print_neighbors(spdlog::level::level_enum lvl = spdlog::level::off) const;
    void print_axels(spdlog::level::level_enum lvl = spdlog::level::off) const;
    void print_matrix() const { _biadjacency.print_matrix(); }

    std::vector<size_t> find_minimal_sums(dvlab::BooleanMatrix& matrix);
    std::vector<dvlab::BooleanMatrix::RowOperation> greedy_reduction(dvlab::BooleanMatrix& m);

private:
    size_t _num_cz_rms              = 0;
    size_t _num_cz_rms_after_gadget = 0;
    size_t _num_cx_rms              = 0;
    size_t _num_cx_iterations       = 0;
    zx::ZXGraph* _graph;
    qcir::QCir* _logical_circuit;
    bool _random;
    bool _previous_gadget = false;
    zx::ZXVertexList _frontier;
    zx::ZXVertexList _neighbors;
    zx::ZXVertexList _axels;
    std::unordered_map<QubitIdType, QubitIdType> _qubit_map;  // zx to qc

    dvlab::BooleanMatrix _biadjacency;
    std::vector<dvlab::BooleanMatrix::RowOperation> _cnots;

    void _block_elimination(dvlab::BooleanMatrix& matrix, size_t& min_n_cxs, size_t block_size);
    void _filter_duplicate_cxs();
    // NOTE - Use only in column optimal swap
    Target _find_column_swap(Target target);
    ConnectInfo _row_info;
    ConnectInfo _col_info;

    Overlap _max_overlap(dvlab::BooleanMatrix& matrix);

    int _calculate_diff_pivot_edges_if_extracting_cz(zx::ZXVertex* frontier, zx::ZXVertex* axel, zx::ZXVertex* cz_target);

    size_t _num_cx_filtered = 0;
    size_t _num_swaps       = 0;
    size_t _cnt_print       = 0;
    size_t _max_axel        = 0;

    std::vector<size_t> _initial_placement;

    ExtractorConfig _config;
};

}  // namespace extractor

}  // namespace qsyn
