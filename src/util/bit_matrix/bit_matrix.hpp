/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define class BitMatrix structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <utility>
#include <vector>

#include "util/ordered_hashset.hpp"

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

namespace dvlab {

namespace bit_matrix {

class BitMatrix {
public:
    class Row {
    public:
        Row(std::vector<unsigned char> const& r) : _row(r) {}
        Row(size_t size, unsigned char val) : _row(size, val) {}
        Row(size_t size) : _row(size, 0) {}

        std::vector<unsigned char> const& get_row() const { return _row; }
        void set_row(std::vector<unsigned char> row) { _row = std::move(row); }
        size_t size() const { return _row.size(); }
        unsigned char& back() { return _row.back(); }
        unsigned char const& back() const { return _row.back(); }
        size_t sum() const;

        bool is_one_hot() const;
        bool is_zeros() const;
        void print_row(spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;

        void emplace_back(unsigned char i) { _row.emplace_back(i); }

        auto begin() { return _row.begin(); }
        auto end() { return _row.end(); }
        auto begin() const { return _row.begin(); }
        auto end() const { return _row.end(); }

        Row& operator+=(Row const& rhs);
        friend Row operator+(Row lhs, Row const& rhs);

        bool operator==(Row const& rhs) const;

        unsigned char& operator[](size_t const& i) {
            return _row[i];
        }
        unsigned char const& operator[](size_t const& i) const {
            return _row[i];
        }

    private:
        std::vector<unsigned char> _row;
    };
    using RowOperation  = std::pair<size_t, size_t>;
    using RowOperations = std::vector<RowOperation>;

    BitMatrix() {}
    BitMatrix(std::vector<Row> const& matrix) : _matrix(matrix) {}
    BitMatrix(std::vector<Row>&& matrix) : _matrix(std::move(matrix)) {}
    BitMatrix(size_t rows, size_t cols, unsigned char val) : _matrix(rows, Row(cols, val)) {}
    BitMatrix(size_t rows, size_t cols) : _matrix(rows, Row(cols)) {}
    BitMatrix(size_t side_length) : _matrix(side_length, Row(side_length)) {}

    void reset();
    std::vector<Row> const& get_matrix() { return _matrix; }
    RowOperations const& get_row_operations() const { return _row_operations; }
    RowOperations& get_row_operations() { return _row_operations; }
    Row const& get_row(size_t r) { return _matrix[r]; }
    std::optional<size_t> find_row(Row);

    size_t num_rows() const { return _matrix.size(); }
    size_t num_cols() const { return _matrix[0].size(); }

    void clear() {
        _matrix.clear();
        _row_operations.clear();
    }

    void push_zeros_column();
    void push_zeros_row() { _matrix.emplace_back(std::vector<unsigned char>(_matrix[0].size(), 0)); }
    void push_row(Row const& row) { _matrix.emplace_back(row); }
    void push_row(Row&& row) { _matrix.emplace_back(std::move(row)); }
    void erase_row(size_t r) { (_matrix.erase(_matrix.begin() + r)); };

    Row& operator[](size_t const& i) {
        return _matrix[i];
    }
    Row const& operator[](size_t const& i) const {
        return _matrix[i];
    }

    auto begin() { return _matrix.begin(); }
    auto end() { return _matrix.end(); }
    auto begin() const { return _matrix.begin(); }
    auto end() const { return _matrix.end(); }

    void print_matrix(spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;
    // used by extractor
    size_t filter_duplicate_row_operations();

private:
    std::vector<Row> _matrix;
    RowOperations _row_operations;
};

void row_operation(BitMatrix& matrix, size_t ctrl, size_t targ);

size_t row_operation_depth(BitMatrix::RowOperations const& row_ops);
double dense_ratio(BitMatrix::RowOperations const& row_ops);
void print_row_ops(BitMatrix::RowOperations const& row_ops);

}  // namespace bit_matrix

}  // namespace dvlab
