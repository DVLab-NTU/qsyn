/****************************************************************************
  PackageName  [ m2 ]
  Synopsis     [ Define class m2 (Boolean Matrix) structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "util/ordered_hashset.hpp"

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

class ZXVertex;
using ZXVertexList = ordered_hashset<ZXVertex*>;

// REVIEW - Change if bit > 64
class Row {
public:
    Row(size_t id, std::vector<unsigned char> const& r) : _row(r) {}

    std::vector<unsigned char> const& get_row() const { return _row; }
    void set_row(std::vector<unsigned char> row) { _row = row; }
    size_t size() const { return _row.size(); }
    unsigned char& back() { return _row.back(); }
    unsigned char const& back() const { return _row.back(); }
    size_t sum() const;

    bool is_one_hot() const;
    bool is_zeros() const;
    void print_row() const;

    void emplace_back(unsigned char i) { _row.emplace_back(i); }

    Row& operator+=(Row const& rhs);
    friend Row operator+(Row lhs, Row const& rhs);

    unsigned char& operator[](size_t const& i) {
        return _row[i];
    }
    unsigned char const& operator[](size_t const& i) const {
        return _row[i];
    }

private:
    std::vector<unsigned char> _row;
};

class BooleanMatrix {
public:
    using RowOperation = std::pair<size_t, size_t>;
    BooleanMatrix() {}

    void reset();
    bool from_zxvertices(ZXVertexList const& frontier, ZXVertexList const& neighbors);
    std::vector<Row> const& get_matrix() { return _matrix; }
    std::vector<RowOperation> const& get_row_operations() { return _row_operations; }
    Row const& get_row(size_t r) { return _matrix[r]; }

    size_t num_rows() const { return _matrix.size(); }
    size_t num_cols() const { return _matrix[0].size(); }

    void clear() {
        _matrix.clear();
        _row_operations.clear();
    }

    bool row_operation(size_t ctrl, size_t targ, bool track = false);
    size_t gaussian_elimination_skip(size_t block_size, bool fully_reduced, bool track = true);
    bool gaussian_elimination(bool track = false, bool is_augmented_matrix = false);
    bool gaussian_elimination_augmented(bool track = false);
    bool is_solved_form() const;
    bool is_augmented_solved_form() const;
    void print_matrix() const;
    void print_trace() const;
    size_t filter_duplicate_row_operations();
    size_t row_operation_depth();
    float dense_ratio();
    void append_one_hot_column(size_t idx);
    void push_zeros_column();

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
