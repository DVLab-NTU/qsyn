/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Definition of the Tabler class that replaces the fort::utf8_table]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <span>
#include <string>
#include <vector>

namespace dvlab {

class Tabler {
public:
    using size_t = std::size_t;
    Tabler();
    ~Tabler() = default;

    size_t& cell_left_padding() { return _cell_left_padding; }
    size_t const& cell_left_padding() const { return _cell_left_padding; }
    size_t& cell_right_padding() { return _cell_right_padding; }
    size_t const& cell_right_padding() const { return _cell_right_padding; }
    size_t& left_margin() { return _left_margin; }
    size_t const& left_margin() const { return _left_margin; }

    size_t n_rows() const { return _table.size(); }
    size_t n_columns() const;

    void add_row(std::span<std::string> const& row);
    void add_column(std::span<std::string> const& column);

    std::string to_string() const;

private:
    std::vector<std::vector<std::string>> _table;
    std::vector<size_t> _column_widths;
    size_t _cell_left_padding;
    size_t _cell_right_padding;
    size_t _left_margin;

    size_t _get_string_width(std::string_view str) const;
};

}  // namespace dvlab
