/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define class BitMatrix member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./bit_matrix.hpp"

#include <cassert>
#include <cmath>
#include <gsl/util>
#include <tl/enumerate.hpp>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fmt/core.h"
#include "util/util.hpp"

namespace dvlab::bit_matrix {

/**
 * @brief Overload operator + for Row
 *
 * @param lhs
 * @param rhs
 * @return Row
 */
BitMatrix::Row operator+(BitMatrix::Row lhs, BitMatrix::Row const& rhs) {
    lhs += rhs;
    return lhs;
}

/**
 * @brief Overload operator += for Row
 *
 * @param rhs
 * @return Row&
 */
BitMatrix::Row& BitMatrix::Row::operator+=(Row const& rhs) {
    assert(_row.size() == rhs._row.size());
    for (size_t i = 0; i < _row.size(); i++) {
        _row[i] = (_row[i] + rhs._row[i]) % 2;
    }
    return *this;
}

/**
 * @brief Print row
 *
 */
void BitMatrix::Row::print_row(spdlog::level::level_enum lvl) const {
    spdlog::log(lvl, "{}", fmt::join(_row, " "));
}

/**
 * @brief @brief Check if the row is one hot
 *
 * @return true
 * @return false
 */
bool BitMatrix::Row::is_one_hot() const {
    // we don't use count_if here because we want to stop early if we find a second 1
    auto first_one = std::ranges::find(_row, 1);
    if (first_one == _row.end()) return false;
    return std::ranges::find(first_one + 1, _row.end(), 1) == _row.end();
}

/**
 * @brief Check if the row contains all zeros
 *
 * @return true
 * @return false
 */
bool BitMatrix::Row::is_zeros() const {
    for (auto& i : _row) {
        if (i == 1) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Sum the values of the row
 *
 * @return Sum of the row
 */
size_t BitMatrix::Row::sum() const {
    size_t sum = 0;
    for (auto& i : _row) {
        if (i == 1) {
            ++sum;
        }
    }
    return sum;
}

/**
 * @brief Clear matrix and operations
 *
 */
void BitMatrix::reset() {
    _matrix.clear();
    _row_operations.clear();
}

/**
 * @brief Print matrix
 *
 */
void BitMatrix::print_matrix(spdlog::level::level_enum lvl) const {
    for (auto const& row : _matrix) {
        row.print_row(lvl);
    }
}

/**
 * @brief Print track of operations
 *
 */
void print_row_ops(BitMatrix::RowOperations const& row_ops) {
    fmt::println("Track:");
    for (auto const& [i, row_op] : tl::views::enumerate(row_ops)) {
        auto const& [row_src, row_dest] = row_op;
        fmt::println("Step {}: {} to {}", i + 1, row_src, row_dest);
    }
    fmt::println("");
}

/**
 * @brief Perform XOR operation
 * @param ctrl the control
 * @param targ the target
 * @param track if true, record the process to operation track
 *
 * @return true if successfully XORed,
 * @return false if not
 */
void row_operation(BitMatrix& matrix, size_t ctrl, size_t targ) {
    DVLAB_ASSERT(ctrl < matrix.num_rows(), fmt::format("Wrong dimension {}", ctrl));
    DVLAB_ASSERT(targ < matrix.num_rows(), fmt::format("Wrong dimension {}", targ));
    matrix[targ] += matrix[ctrl];
    matrix.get_row_operations().emplace_back(ctrl, targ);
}

/**
 * @brief A temporary method to filter duplicated operations
 *
 * @return size_t
 */
size_t BitMatrix::filter_duplicate_row_operations() {
    // for self-documentation
    using RowIdxType = size_t;
    using OpIdxType  = size_t;
    std::vector<OpIdxType> dups;
    struct RowAndOp {
        RowIdxType row_idx;
        OpIdxType op_idx;
    };
    std::unordered_map<RowIdxType, RowAndOp> last_used;  // NOTE - bit, (another bit, gateId)
    for (size_t ith_row_op = 0; ith_row_op < _row_operations.size(); ith_row_op++) {
        auto& [row_src, row_dest] = _row_operations[ith_row_op];
        auto const first_match    = last_used.contains(row_src) &&
                                 last_used[row_src].row_idx == row_dest &&
                                 _row_operations[last_used[row_src].op_idx].first == row_src;  // make sure the destinations are matched

        auto const second_match = last_used.contains(row_dest) &&
                                  last_used[row_dest].row_idx == row_src &&
                                  _row_operations[last_used[row_dest].op_idx].second == row_dest;  // make sure the destinations are matched

        if (first_match && second_match) {
            dups.emplace_back(ith_row_op);
            dups.emplace_back(last_used[row_dest].op_idx);
            last_used.erase(row_src);
            last_used.erase(row_dest);
        } else {
            last_used[row_src]  = {row_dest, ith_row_op};
            last_used[row_dest] = {row_src, ith_row_op};
        }
    }
    sort(dups.begin(), dups.end());

    for (auto const& op : dups | std::views::reverse) {
        _row_operations.erase(dvlab::iterator::next(_row_operations.begin(), op));
    }

    return dups.size();
}

/**
 * @brief Get depth of operations
 *
 * @return size_t
 */
size_t row_operation_depth(BitMatrix::RowOperations const& row_ops) {
    std::unordered_map<size_t, size_t> row_depth;
    if (row_ops.empty()) {
        return 0;
    }
    for (auto const& [a, b] : row_ops) {
        if (!row_depth.contains(a)) row_depth[a] = 0;
        if (!row_depth.contains(b)) row_depth[b] = 0;
        auto const max_depth = std::max(row_depth[a], row_depth[b]);
        row_depth[a]         = max_depth + 1;
        row_depth[b]         = max_depth + 1;
    }
    return std::ranges::max(row_depth | std::views::values);
}

/**
 * @brief Get dense ratio of operations
 *
 * @return float
 */
double dense_ratio(BitMatrix::RowOperations const& row_ops) {
    auto const depth = row_operation_depth(row_ops);
    if (depth == 0)
        return 0;
    auto const ratio = float(depth) / float(row_ops.size());
    return round(ratio * 100) / 100;
}

/**
 * @brief Push a new column at the end of the matrix
 *
 */
void BitMatrix::push_zeros_column() {
    for_each(_matrix.begin(), _matrix.end(), [](Row& r) { r.emplace_back(0); });
}

}  // namespace dvlab::bit_matrix
