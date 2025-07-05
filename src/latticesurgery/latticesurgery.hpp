/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define class LatticeSurgery structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "latticesurgery/latticesurgery_gate.hpp"
#include "latticesurgery/latticesurgery_grid.hpp"
#include "latticesurgery/latticesurgery_qubit.hpp"
#include "qsyn/qsyn_type.hpp"
#include "spdlog/common.h"
#include "util/ordered_hashmap.hpp"

namespace qsyn::latticesurgery {

class LatticeSurgery {
public:
    using QubitIdType = qsyn::QubitIdType;
    
    LatticeSurgery() = default;
    LatticeSurgery(size_t n_qubits) { add_qubits(n_qubits); }
    LatticeSurgery(size_t rows, size_t cols) : _grid(rows, cols) { add_qubits(rows * cols); }
    ~LatticeSurgery() = default;
    LatticeSurgery(LatticeSurgery const& other);
    LatticeSurgery(LatticeSurgery&& other) noexcept = default;

    LatticeSurgery& operator=(LatticeSurgery copy) {
        copy.swap(*this);
        return *this;
    }

    void swap(LatticeSurgery& other) noexcept {
        std::swap(_gate_id, other._gate_id);
        std::swap(_dirty, other._dirty);
        std::swap(_filename, other._filename);
        std::swap(_qubits, other._qubits);
        std::swap(_gate_list, other._gate_list);
        std::swap(_id_to_gates, other._id_to_gates);
        std::swap(_predecessors, other._predecessors);
        std::swap(_successors, other._successors);
        std::swap(_grid, other._grid);
    }

    friend void swap(LatticeSurgery& a, LatticeSurgery& b) noexcept { a.swap(b); }

    // Access functions
    size_t get_num_qubits() const { return _qubits.size(); }
    size_t get_num_gates() const { return _id_to_gates.size(); }
    size_t calculate_depth() const;
    std::vector<LatticeSurgeryQubit> const& get_qubits() const { return _qubits; }
    std::vector<LatticeSurgeryGate*> const& get_gates() const {
        _update_topological_order();
        return _gate_list;
    }

    LatticeSurgeryGate* get_gate(std::optional<size_t> gid) const;
    std::string get_filename() const { return _filename; }
    bool is_empty() const { return _qubits.empty() || _id_to_gates.empty(); }

    void set_filename(std::string f) { _filename = std::move(f); }
    void reset();

    // Circuit construction methods
    void push_qubit() { _qubits.emplace_back(); }
    void insert_qubit(QubitIdType id);
    void add_qubits(size_t num);
    bool remove_qubit(QubitIdType qid);
    size_t append(LatticeSurgeryGate const& gate);
    size_t prepend(LatticeSurgeryGate const& gate);
    bool remove_gate(size_t id);
    void add_logical_patch(size_t col, size_t row){
        fmt::println("add logical patch: ({},{})", col, row);
        get_patch(col, row)->set_occupied(true);
        get_patch(col, row)->set_logical_id(get_patch(col, row)->get_id()+1);
        // get_patch(col, row)->set_logical_id(_grid.get_max_id()+1);
        // _grid.set_max_id(_grid.get_max_id()+1);
    }; // TODO


    // Grid access methods
    LatticeSurgeryGrid& get_grid() { return _grid; }
    LatticeSurgeryGrid const& get_grid() const { return _grid; }
    size_t get_grid_rows() const { return _grid.get_rows(); }
    size_t get_grid_cols() const { return _grid.get_cols(); }
    LatticeSurgeryQubit* get_patch(size_t col, size_t row) { return _grid.get_patch(col, row); }
    LatticeSurgeryQubit* get_patch(size_t col, size_t row) const { return _grid.get_patch(col, row); }
    LatticeSurgeryQubit* get_patch(size_t id) { return _grid.get_patch(id); }
    LatticeSurgeryQubit* get_patch(size_t id) const { return _grid.get_patch(id); }
    size_t get_patch_id(size_t col, size_t row) const;
    bool are_patches_adjacent(size_t id1, size_t id2) const { return _grid.are_adjacent(id1, id2); }
    std::vector<size_t> get_adjacent_patches(size_t id) const { return _grid.get_adjacent_patches(id); }

    // Initialize logical qubit tracking for a grid of patches
    void init_logical_tracking(size_t num_patches) { _init_logical_tracking(num_patches); }

    // Merge/Split operations
    bool merge_patches(std::vector<QubitIdType> const& patch_ids); // TO CHECK
    // bool merge_patches(std::vector<QubitIdType> patch_ids, std::vector<MeasureType> measure_types){
    //     return merge_patches(patch_ids, measure_types, false, 0);
    // }
    bool merge_patches(std::vector<QubitIdType> patch_ids, std::vector<MeasureType> measure_types, bool color_flip, size_t depth); // TO CHECK
    bool split_patches(std::vector<QubitIdType> const& patch_ids); // TO CHECK
    bool check_connectivity(std::vector<QubitIdType> const& patch_ids) const;
    bool check_same_logical_id(std::vector<QubitIdType> const& patch_ids) const;
    QubitIdType get_smallest_logical_id(std::vector<QubitIdType> const& patch_ids) const;
    QubitIdType find_logical_id(QubitIdType patch_id) const;
    void union_logical_ids(QubitIdType id1, QubitIdType id2);
    void split_logical_ids(std::vector<QubitIdType> const& group1, std::vector<QubitIdType> const& group2);
    // void one_to_n(std::pair<size_t,size_t> start_id, std::vector<std::pair<size_t,size_t>>& id_lists); // TO CHECK
    // void n_to_one(std::vector<std::pair<size_t,size_t>>& init_patches, std::pair<size_t,size_t> dest_patch); // TO CHECK
    void n_to_n(std::vector<std::pair<size_t,size_t>>& start_list, std::vector<std::pair<size_t,size_t>>& dest_list); // TO CHECK
    void hadamard(size_t col, size_t row); // TO CHECK
    void hadamard(std::pair<size_t, size_t> start, std::vector<std::pair<size_t, size_t>>& dest_list, bool preserve_start, bool is_x); // TO CHECK start: the original patch needed hadamard, dest: the adjecent patch to adjust set back the orientation of the patch
    void discard_patch(QubitIdType id, MeasureType measure_type); // TO CHECK


    // I/O methods
    bool write_ls(std::filesystem::path const& filepath) const;
    bool read_ls(std::filesystem::path const& filepath);
    bool write_lasre(std::filesystem::path const& filepath) const;
    std::string to_lasre() const;

    // Reporting methods
    void print_gates(bool print_neighbors = false,
                    std::span<size_t> gate_ids = {}) const;
    void print_ls() const;
    void print_ls_info() const;
    void print_grid() const { _grid.print_grid(); }
    void print_occupied();

    // Gate connection methods
    std::optional<size_t> get_predecessor(std::optional<size_t> gate_id, size_t pin) const;
    std::optional<size_t> get_successor(std::optional<size_t> gate_id, size_t pin) const;
    std::vector<std::optional<size_t>> get_predecessors(std::optional<size_t> gate_id) const;
    std::vector<std::optional<size_t>> get_successors(std::optional<size_t> gate_id) const;

    LatticeSurgeryGate* get_first_gate(QubitIdType qubit) const;
    LatticeSurgeryGate* get_last_gate(QubitIdType qubit) const;

    std::unordered_map<size_t, size_t> calculate_gate_times() const;

private:
    size_t _gate_id = 0;
    std::string _filename;
    std::vector<LatticeSurgeryQubit> _qubits;
    dvlab::utils::ordered_hashmap<size_t, std::unique_ptr<LatticeSurgeryGate>> _id_to_gates;
    std::unordered_map<size_t, std::vector<std::optional<size_t>>> _predecessors;
    std::unordered_map<size_t, std::vector<std::optional<size_t>>> _successors;
    LatticeSurgeryGrid _grid;

    std::vector<LatticeSurgeryGate*> mutable _gate_list;
    bool mutable _dirty = true;

    // Logical qubit tracking
    std::vector<QubitIdType> _logical_parent;  // Parent array for union-find
    std::vector<size_t> _logical_rank;         // Rank array for union-find
    void _init_logical_tracking(size_t num_patches);
    QubitIdType _find_logical_id(QubitIdType id) const;
    QubitIdType _find_logical_id_with_compression(QubitIdType id);
    void _union_logical_ids(QubitIdType id1, QubitIdType id2);
    std::vector<std::vector<QubitIdType>> _get_connected_components(std::vector<QubitIdType> const& patch_ids) const;

    void _update_topological_order() const;
    void _set_predecessor(size_t gate_id, size_t pin, std::optional<size_t> pred = std::nullopt);
    void _set_successor(size_t gate_id, size_t pin, std::optional<size_t> succ = std::nullopt);
    void _set_predecessors(size_t gate_id, std::vector<std::optional<size_t>> const& preds);
    void _set_successors(size_t gate_id, std::vector<std::optional<size_t>> const& succs);
    void _connect(size_t gid1, size_t gid2, QubitIdType qubit);
};

} // namespace qsyn::latticesurgery 