/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define class LatticeSurgeryGrid structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "latticesurgery/latticesurgery_qubit.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::latticesurgery {

class LatticeSurgeryGrid {
public:
    LatticeSurgeryGrid() = default;
    LatticeSurgeryGrid(size_t cols, size_t rows) : _rows(rows), _cols(cols) {
        _patches.resize(_cols, std::vector<LatticeSurgeryQubit*>(_rows, nullptr));
        for (size_t row = 0; row < _rows; ++row) {
            for (size_t col = 0; col < _cols; ++col) {
            auto* qubit = new LatticeSurgeryQubit();
            qubit->set_id(_max_patch_id);
            _max_patch_id++;
            _patches[col][row] = qubit;
            _patch_list.push_back(qubit);
            }
        }
        fmt::println("create patch with size (col, row)=({},{})", _cols, _rows);
    }

    // Basic access methods
    size_t get_rows() const { return _rows; }
    size_t get_cols() const { return _cols; }
    size_t get_num_patches() const { return _patch_list.size(); }
    LatticeSurgeryQubit* get_patch(size_t col, size_t row) { return _patches[col][row]; }
    LatticeSurgeryQubit* get_patch(size_t col, size_t row) const { return _patches[col][row]; }
    LatticeSurgeryQubit* get_patch(size_t id) { return _patch_list[id]; }
    LatticeSurgeryQubit* get_patch(size_t id) const { return _patch_list[id]; }

    // Grid operations
    bool is_valid_position(size_t col, size_t row) const {
        return row < _rows && col < _cols;
    }
    size_t get_patch_id(size_t col, size_t row) const {
        if (col >= _cols || row >= _rows) {
            fmt::println("ERROR: Invalid position ({}, {}), grid size is {}x{}", col, row, _cols, _rows);
            return 0; // Return a safe default
        }
        return _patches[col][row]->get_id();
    }
    std::pair<size_t, size_t> get_patch_position(size_t id) const {
        // Return position as (col, row) to be consistent
        size_t col = id % _cols;
        size_t row = id / _cols;
        return {col, row};
    }

    // Get current ID
    size_t get_max_id() const { return _max_patch_id; }
    // void set_max_id(size_t s) { _max_patch_id=s; }

    // Adjacency operations
    bool are_adjacent(size_t id1, size_t id2) const {
        auto [col1, row1] = get_patch_position(id1);
        auto [col2, row2] = get_patch_position(id2);
        return (std::abs(static_cast<int>(row1) - static_cast<int>(row2)) == 1 && col1 == col2) ||
               (std::abs(static_cast<int>(col1) - static_cast<int>(col2)) == 1 && row1 == row2);
    }
    std::vector<size_t> get_adjacent_patches(size_t id) const {
        std::vector<size_t> adjacents;
        auto [col, row] = get_patch_position(id);
        
        // Check all four directions
        if (row > 0) adjacents.push_back(get_patch_id(col, row - 1));           // Up
        if (row < _rows - 1) adjacents.push_back(get_patch_id(col, row + 1));   // Down
        if (col > 0) adjacents.push_back(get_patch_id(col - 1, row));           // Left
        if (col < _cols - 1) adjacents.push_back(get_patch_id(col + 1, row));   // Right
        
        return adjacents;
    }

    // Print methods
    void print_grid() const;
    void print_patch_info(size_t id) const;

private:
    size_t _rows = 0;
    size_t _cols = 0;
    size_t _max_patch_id = 0;
    std::vector<std::vector<LatticeSurgeryQubit*>> _patches; // (col, row)
    std::vector<LatticeSurgeryQubit*> _patch_list;
};

} // namespace qsyn::latticesurgery 