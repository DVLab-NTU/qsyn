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

namespace qsyn {

namespace qcir {
class QCir;
}
namespace zx {
class ZXGraph;
}

namespace extractor {

extern bool SORT_FRONTIER;
extern bool SORT_NEIGHBORS;
extern bool PERMUTE_QUBITS;
extern bool FILTER_DUPLICATE_CXS;
extern size_t BLOCK_SIZE;
extern size_t OPTIMIZE_LEVEL;

class Extractor {
public:
    using Target      = std::unordered_map<size_t, size_t>;
    using ConnectInfo = std::vector<std::set<size_t>>;
    // using Device      = duostra::Duostra::Device;
    // using Operation   = duostra::Duostra::Operation;

    Extractor(zx::ZXGraph* g, qcir::QCir* c = nullptr /*, std::optional<Device> const& d = std::nullopt */);

    // bool to_physical() { return _device.has_value(); }
    qcir::QCir* get_logical() { return _logical_circuit; }

    void initialize(bool from_empty_qcir = true);
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
    size_t _num_cx_iterations = 0;
    zx::ZXGraph* _graph;
    qcir::QCir* _logical_circuit;
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

    size_t _num_cx_filtered = 0;
    size_t _num_swaps       = 0;

    std::vector<size_t> _initial_placement;
};

}  // namespace extractor

}  // namespace qsyn
