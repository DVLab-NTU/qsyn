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

#include "device/device.hpp"
#include "util/boolean_matrix.hpp"
#include "zx/zx_def.hpp"

extern bool SORT_FRONTIER;
extern bool SORT_NEIGHBORS;
extern bool PERMUTE_QUBITS;
extern bool FILTER_DUPLICATED_CXS;
extern size_t BLOCK_SIZE;
extern size_t OPTIMIZE_LEVEL;

class QCir;
class ZXGraph;

class Extractor {
public:
    using Target = std::unordered_map<size_t, size_t>;
    using ConnectInfo = std::vector<std::set<size_t>>;
    Extractor(ZXGraph*, QCir* = nullptr, std::optional<Device> = std::nullopt);
    ~Extractor() {}

    bool to_physical() { return _device.has_value(); }
    QCir* get_logical() { return _logical_circuit; }

    void initialize(bool from_empty_qcir = true);
    QCir* extract();
    bool extraction_loop(size_t = size_t(-1));
    bool remove_gadget(bool check = false);
    bool biadjacency_eliminations(bool check = false);
    void column_optimal_swap();
    void extract_singles();
    bool extract_czs(bool check = false);
    void extract_cxs(size_t = 1);
    size_t extract_hadamards_from_matrix(bool check = false);
    void clean_frontier();
    void permute_qubits();

    void update_neighbors();
    void update_graph_by_matrix(EdgeType = EdgeType::hadamard);
    void create_matrix();

    void prepend_single_qubit_gate(std::string, size_t, Phase);
    void prepend_double_qubit_gate(std::string, std::vector<size_t> const&, Phase);
    void prepend_series_gates(std::vector<Operation> const&, std::vector<Operation> const& = {});
    void prepend_swap_gate(size_t, size_t, QCir*);
    bool frontier_is_cleaned();
    bool axel_in_neighbors();
    bool contains_single_neighbor();
    void print_cxs();
    void print_frontier();
    void print_neighbors();
    void print_axels();
    void print_matrix() { _biadjacency.print_matrix(); }

    std::vector<size_t> find_minimal_sums(BooleanMatrix& matrix, bool do_reversed_search = false);
    std::vector<BooleanMatrix::RowOperation> greedy_reduction(BooleanMatrix&);

private:
    size_t _num_cx_iterations = 0;
    ZXGraph* _graph;
    QCir* _logical_circuit;
    QCir* _physical_circuit;
    std::optional<Device> _device;
    std::optional<Device> _device_backup;
    ZXVertexList _frontier;
    ZXVertexList _neighbors;
    ZXVertexList _axels;
    std::unordered_map<size_t, size_t> _qubit_map;  // zx to qc

    BooleanMatrix _biadjacency;
    std::vector<BooleanMatrix::RowOperation> _cnots;

    void _block_elimination(BooleanMatrix& matrix, size_t& min_n_cxs, size_t block_size);
    void _block_elimination(size_t& best_block, BooleanMatrix& best_matrix, size_t& min_cost, size_t block_size);
    std::vector<Operation> _duostra_assigned;
    std::vector<Operation> _duostra_mapped;
    // NOTE - Use only in column optimal swap
    Target _find_column_swap(Target);
    ConnectInfo _row_info;
    ConnectInfo _col_info;

    size_t _num_cx_filtered = 0;
    size_t _num_swaps = 0;

    std::vector<size_t> _initial_placement;
};
