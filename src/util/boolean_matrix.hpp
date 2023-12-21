/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define class BooleanMatrix structure ]
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

class BooleanMatrix {
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
    using RowOperation = std::pair<size_t, size_t>;

    BooleanMatrix() {}
    BooleanMatrix(std::vector<Row> const& matrix) : _matrix(matrix) {}
    BooleanMatrix(std::vector<Row>&& matrix) : _matrix(std::move(matrix)) {}
    BooleanMatrix(size_t rows, size_t cols, unsigned char val) : _matrix(rows, Row(cols, val)) {}
    BooleanMatrix(size_t rows, size_t cols) : _matrix(rows, Row(cols)) {}
    BooleanMatrix(size_t side_length) : _matrix(side_length, Row(side_length)) {}

    void reset();
    std::vector<Row> const& get_matrix() { return _matrix; }
    std::vector<RowOperation> const& get_row_operations() { return _row_operations; }
    Row const& get_row(size_t r) { return _matrix[r]; }
    std::optional<size_t> find_row(Row);

    size_t num_rows() const { return _matrix.size(); }
    size_t num_cols() const { return _matrix[0].size(); }

    void clear() {
        _matrix.clear();
        _row_operations.clear();
    }

    bool row_operation(size_t ctrl, size_t targ, bool track = false);
    size_t gaussian_elimination_skip(size_t block_size, bool do_fully_reduced, bool track = true);
    bool gaussian_elimination(bool track = false, bool is_augmented_matrix = false);
    bool gaussian_elimination_augmented(bool track = false);
    bool is_solved_form() const;
    bool is_augmented_solved_form() const;
    void print_matrix(spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;
    void print_trace() const;
    size_t filter_duplicate_row_operations();
    size_t row_operation_depth();
    double dense_ratio();
    void append_one_hot_column(size_t idx);
    void push_zeros_column();
    void push_zeros_row() { _matrix.emplace_back(std::vector<unsigned char>(_matrix[0].size(), 0)); }
    void push_row(Row const& row) { _matrix.emplace_back(row); }
    void push_wor(Row&& row) { _matrix.emplace_back(std::move(row)); }
    void erase_row(size_t r) { (_matrix.erase(_matrix.begin() + r)); };

    Row& operator[](size_t const& i) {
        return _matrix[i];
    }
    Row const& operator[](size_t const& i) const {
        return _matrix[i];
    }

private:
    std::vector<Row> _matrix;
    std::vector<RowOperation> _row_operations;
};

}  // namespace dvlab
