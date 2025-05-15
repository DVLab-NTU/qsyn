/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Implementation of LatticeSurgeryGrid class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "latticesurgery/latticesurgery_grid.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <string>

namespace qsyn::latticesurgery {

void LatticeSurgeryGrid::print_grid() const {
    fmt::println("Grid Layout ({}x{}):", _rows, _cols);
    
    // Print column headers
    fmt::print("    ");
    for (size_t col = 0; col < _cols; ++col) {
        fmt::print("{:4}", col);
    }
    fmt::println("");

    // Print each row
    for (size_t row = 0; row < _rows; ++row) {
        fmt::print("{:3} ", row);
        for (size_t col = 0; col < _cols; ++col) {
            size_t id = get_patch_id(row, col);
            auto const& patch = _patch_list[id];
            fmt::print("{:4}", patch->get_logical_id());
        }
        fmt::println("");
    }
}

void LatticeSurgeryGrid::print_patch_info(size_t id) const {
    if (id >= _patches.size()) {
        spdlog::error("Patch ID {} out of range (max: {})", id, _patches.size() - 1);
        return;
    }

    auto [row, col] = get_patch_position(id);
    auto const& patch = _patch_list[id];

    fmt::println("Patch {} at position ({}, {}):", id, row, col);
    fmt::println("  Physical ID: {}", patch->get_id());
    fmt::println("  Logical ID: {}", patch->get_logical_id());
    
    fmt::print("  Adjacent Patches: ");
    auto adjacents = get_adjacent_patches(id);
    fmt::println("{}", fmt::join(adjacents, ", "));
}

} // namespace qsyn::latticesurgery 