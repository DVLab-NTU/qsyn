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
    LatticeSurgeryGrid(size_t rows, size_t cols) : _rows(rows), _cols(cols) {
        _patches.resize(rows * cols);
        for (size_t i = 0; i < rows * cols; ++i) {
            _patches[i] = LatticeSurgeryQubit(i);
        }
    }

    // Basic access methods
    size_t get_rows() const { return _rows; }
    size_t get_cols() const { return _cols; }
    size_t get_num_patches() const { return _rows * _cols; }
    LatticeSurgeryQubit& get_patch(size_t row, size_t col) { return _patches[row * _cols + col]; }
    LatticeSurgeryQubit const& get_patch(size_t row, size_t col) const { return _patches[row * _cols + col]; }
    LatticeSurgeryQubit& get_patch(size_t id) { return _patches[id]; }
    LatticeSurgeryQubit const& get_patch(size_t id) const { return _patches[id]; }

    // Grid operations
    bool is_valid_position(size_t row, size_t col) const {
        return row < _rows && col < _cols;
    }
    size_t get_patch_id(size_t row, size_t col) const {
        return row * _cols + col;
    }
    std::pair<size_t, size_t> get_patch_position(size_t id) const {
        return {id / _cols, id % _cols};
    }

    // Adjacency operations
    bool are_adjacent(size_t id1, size_t id2) const {
        auto [row1, col1] = get_patch_position(id1);
        auto [row2, col2] = get_patch_position(id2);
        return (std::abs(static_cast<int>(row1) - static_cast<int>(row2)) == 1 && col1 == col2) ||
               (std::abs(static_cast<int>(col1) - static_cast<int>(col2)) == 1 && row1 == row2);
    }
    std::vector<size_t> get_adjacent_patches(size_t id) const {
        std::vector<size_t> adjacents;
        auto [row, col] = get_patch_position(id);
        
        // Check all four directions
        if (row > 0) adjacents.push_back(get_patch_id(row - 1, col));           // Up
        if (row < _rows - 1) adjacents.push_back(get_patch_id(row + 1, col));   // Down
        if (col > 0) adjacents.push_back(get_patch_id(row, col - 1));           // Left
        if (col < _cols - 1) adjacents.push_back(get_patch_id(row, col + 1));   // Right
        
        return adjacents;
    }

    // Print methods
    void print_grid() const;
    void print_patch_info(size_t id) const;

private:
    size_t _rows = 0;
    size_t _cols = 0;
    std::vector<LatticeSurgeryQubit> _patches;
};

} // namespace qsyn::latticesurgery 