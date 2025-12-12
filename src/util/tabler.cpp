/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Implementation of the Tabler class that replaces the fort::utf8_table]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tabler.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <ranges>
#include <span>

#include "unicode/display_width.hpp"

namespace dvlab {

Tabler::Tabler() : _cell_left_padding(0), _cell_right_padding(0), _left_margin(0) {}

size_t Tabler::_get_string_width(std::string_view str) const {
    return unicode::display_width(std::string(str));
}

size_t Tabler::n_columns() const {
    return _table.empty()
               ? 0
               : std::ranges::max(_table | std::views::transform(
                                               [](auto const& row) { return row.size(); }));
}

void Tabler::add_row(std::span<std::string> const& row) {
    _table.emplace_back(row.begin(), row.end());

    if (row.size() > _column_widths.size()) {
        _column_widths.resize(row.size());
    }

    for (size_t i = 0; i < row.size(); ++i) {
        _column_widths[i] = std::max(
            _column_widths[i], _get_string_width(row[i]));
    }
}

void Tabler::add_column(std::span<std::string> const& column) {
    if (column.empty()) return;

    // if the column to insert is longer than the number of rows,
    // first append empty rows
    if (column.size() > n_rows()) {
        _table.resize(column.size());
    }

    // then, get the longest row, and fill the shorter rows with empty strings.
    size_t const size_longest_row = n_columns();
    for (auto& row : _table) {
        if (row.size() < size_longest_row) {
            row.resize(size_longest_row);
        }
    }

    // finally, add the column to the table
    for (size_t i = 0; i < column.size(); ++i) {
        _table[i].emplace_back(column[i]);
    }

    // update column widths
    size_t const new_column_width =
        std::ranges::max(column | std::views::transform(
                                      [this](auto const& str) {
                                          return _get_string_width(str);
                                      }));
    _column_widths.emplace_back(new_column_width);
}

std::string Tabler::to_string() const {
    std::string ret;

    // print the table
    for (size_t i = 0; i < n_rows(); ++i) {
        ret += std::string(left_margin(), ' ');
        for (size_t j = 0; j < n_columns(); ++j) {
            if (i >= _table.size()) continue;
            if (j >= _table[i].size()) continue;
            ret += std::string(cell_left_padding(), ' ');
            ret += fmt::format("{}", _table[i][j]);
            size_t const diff = _column_widths[j] - _get_string_width(_table[i][j]);
            ret += std::string(cell_right_padding() + diff, ' ');
        }
        ret += "\n";
    }
    return ret;
}

}  // namespace dvlab
