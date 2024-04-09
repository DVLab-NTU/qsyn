/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define class BooleanMatrix structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <tl/zip.hpp>
#include <utility>
#include <vector>

#include "util/util.hpp"

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

        auto begin() { return _row.begin(); }
        auto end() { return _row.end(); }
        auto begin() const { return _row.begin(); }
        auto end() const { return _row.end(); }

        Row& operator+=(Row const& rhs);
        friend Row operator+(Row lhs, Row const& rhs);

        Row& operator*=(unsigned char const& rhs);
        friend Row operator*(Row lhs, unsigned char const& rhs);
        friend Row operator*(unsigned char const& lhs, Row rhs);

        Row& operator*=(Row const& rhs);
        friend Row operator*(Row lhs, Row const& rhs);

        bool operator==(Row const& rhs) const;

        unsigned char& operator[](size_t const& i) {
            return _row[i];
        }
        unsigned char const& operator[](size_t const& i) const {
            return _row[i];
        }

        void reserve(size_t n) { _row.reserve(n); }

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
    std::vector<Row> const& get_matrix() const { return _matrix; }
    std::vector<RowOperation> const& get_row_operations() const { return _row_operations; }
    Row const& get_row(size_t r) const { return _matrix[r]; }
    std::optional<size_t> find_row(Row const& row) {
        auto it = std::find(_matrix.begin(), _matrix.end(), row);
        if (it == _matrix.end()) return std::nullopt;
        return std::distance(_matrix.begin(), it);
    }

    bool is_empty() const { return _matrix.empty(); }
    size_t num_rows() const { return _matrix.size(); }
    size_t num_cols() const { return _matrix[0].size(); }

    bool row_operation(size_t ctrl, size_t targ, bool track = false);
    size_t gaussian_elimination_skip(size_t block_size, bool do_fully_reduced, bool track = true);
    size_t matrix_rank() const;
    bool gaussian_elimination_augmented(bool track = false);
    void print_matrix(spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;
    size_t filter_duplicate_row_operations();
    size_t row_operation_depth();
    double dense_ratio();
    void push_zeros_column();
    void push_zeros_row() { _matrix.emplace_back(std::vector<unsigned char>(_matrix[0].size(), 0)); }
    void push_row(Row const& row) { _matrix.emplace_back(row); }
    void push_row(Row&& row) { _matrix.emplace_back(std::move(row)); }
    void erase_row(size_t r) { _matrix.erase(dvlab::iterator::next(_matrix.begin(), r)); };

    void reserve(size_t n_rows, size_t n_cols) {
        _matrix.reserve(n_rows);
        for (auto& row : _matrix) row.reserve(n_cols);
    }

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

private:
    std::vector<Row> _matrix;
    std::vector<RowOperation> _row_operations;
};

dvlab::BooleanMatrix vstack(dvlab::BooleanMatrix const& a, dvlab::BooleanMatrix const& b);

// variadic template for vstack
dvlab::BooleanMatrix vstack(
    dvlab::BooleanMatrix const& a,
    dvlab::BooleanMatrix const& b,
    std::convertible_to<dvlab::BooleanMatrix> auto const&... args) {
    return vstack(vstack(a, b), args...);
}

dvlab::BooleanMatrix hstack(dvlab::BooleanMatrix const& a, dvlab::BooleanMatrix const& b);

// variadic template for hstack
dvlab::BooleanMatrix hstack(
    dvlab::BooleanMatrix const& a,
    dvlab::BooleanMatrix const& b,
    std::convertible_to<dvlab::BooleanMatrix> auto const&... args) {
    return hstack(hstack(a, b), args...);
}

dvlab::BooleanMatrix transpose(dvlab::BooleanMatrix const& matrix);

dvlab::BooleanMatrix identity(size_t size);

struct BooleanMatrixRowHash {
    size_t operator()(std::vector<unsigned char> const& k) const;
    size_t operator()(dvlab::BooleanMatrix::Row const& k) const;
};

}  // namespace dvlab
