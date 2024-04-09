/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define class BooleanMatrix member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./boolean_matrix.hpp"

#include <cassert>
#include <cmath>
#include <gsl/util>
#include <tl/enumerate.hpp>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fmt/core.h"
#include "util/util.hpp"

namespace dvlab {

/**
 * @brief Hash function for std::vector<unsigned char>
 *
 * @param k
 * @return size_t
 */
size_t BooleanMatrixRowHash::operator()(std::vector<unsigned char> const& k) const {
    size_t ret = std::hash<unsigned char>()(k[0]);
    for (size_t i = 1; i < k.size(); i++) {
        ret ^= std::hash<unsigned char>()(k[i] << (i % sizeof(size_t)));
    }

    return ret;
}

size_t BooleanMatrixRowHash::operator()(BooleanMatrix::Row const& k) const {
    return operator()(k.get_row());
}

/**
 * @brief Overload operator + for Row
 *
 * @param lhs
 * @param rhs
 * @return Row
 */
BooleanMatrix::Row operator+(BooleanMatrix::Row lhs, BooleanMatrix::Row const& rhs) {
    lhs += rhs;
    return lhs;
}

BooleanMatrix::Row operator*(BooleanMatrix::Row lhs, unsigned char const& rhs) {
    lhs *= rhs;
    return lhs;
}

BooleanMatrix::Row operator*(unsigned char const& lhs, BooleanMatrix::Row rhs) {
    rhs *= lhs;
    return rhs;
}

BooleanMatrix::Row operator*(BooleanMatrix::Row lhs, BooleanMatrix::Row const& rhs) {
    lhs *= rhs;
    return lhs;
}

/**
 * @brief Overload operator += for Row
 *
 * @param rhs
 * @return Row&
 */
BooleanMatrix::Row& BooleanMatrix::Row::operator+=(Row const& rhs) {
    assert(_row.size() == rhs._row.size());
    for (size_t i = 0; i < _row.size(); i++) {
        _row[i] = (_row[i] + rhs._row[i]) % 2;
    }
    return *this;
}

/**
 * @brief Overload operator *= for Row
 *
 * @param rhs
 * @return Row&
 */
BooleanMatrix::Row& BooleanMatrix::Row::operator*=(unsigned char const& rhs) {
    for (size_t i = 0; i < _row.size(); i++) {
        _row[i] = (_row[i] * rhs) % 2;
    }
    return *this;
}

BooleanMatrix::Row& BooleanMatrix::Row::operator*=(Row const& rhs) {
    assert(_row.size() == rhs._row.size());
    for (size_t i = 0; i < _row.size(); i++) {
        _row[i] = (_row[i] * rhs._row[i]) % 2;
    }
    return *this;
}

/**
 * @brief Print row
 *
 */
void BooleanMatrix::Row::print_row(spdlog::level::level_enum lvl) const {
    spdlog::log(lvl, "{}", fmt::join(_row, " "));
}

/**
 * @brief @brief Check if the row is one hot
 *
 * @return true
 * @return false
 */
bool BooleanMatrix::Row::is_one_hot() const {
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
bool BooleanMatrix::Row::is_zeros() const {
    return std::ranges::none_of(_row, [](unsigned char const& e) { return e == 1; });
}

/**
 * @brief Sum the values of the row
 *
 * @return Sum of the row
 */
size_t BooleanMatrix::Row::sum() const {
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
void BooleanMatrix::reset() {
    _matrix.clear();
    _row_operations.clear();
}

/**
 * @brief Print matrix
 *
 */
void BooleanMatrix::print_matrix(spdlog::level::level_enum lvl) const {
    if (!spdlog::should_log(lvl)) return;
    for (auto const& row : _matrix) {
        row.print_row(lvl);
    }
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
bool BooleanMatrix::row_operation(size_t ctrl, size_t targ, bool track) {
    if (ctrl >= _matrix.size()) {
        spdlog::error("Wrong dimension {}", ctrl);
        return false;
    }
    if (targ >= _matrix.size()) {
        spdlog::error("Wrong dimension {}", targ);
        return false;
    }
    _matrix[targ] += _matrix[ctrl];
    if (track) _row_operations.emplace_back(ctrl, targ);
    return true;
}

/**
 * @brief Perform Gaussian Elimination with `block size`. Skip the column if it is duplicated.
 *
 * @param blockSize
 * @param fullReduced if true, performing back-substitution from the echelon form
 * @param track if true, record the process to operation track
 * @return size_t (rank)
 */
size_t BooleanMatrix::gaussian_elimination_skip(size_t block_size, bool do_fully_reduced, bool track) {
    auto const get_section_range = [block_size, this](size_t section_idx) {
        auto section_begin = section_idx * block_size;
        auto section_end   = std::min(num_cols(), (section_idx + 1) * block_size);
        return std::make_pair(section_begin, section_end);
    };

    auto const get_sub_vec = [this](size_t row_idx, size_t section_begin, size_t section_end) {
        auto row_begin = dvlab::iterator::next(_matrix[row_idx].get_row().begin(), section_begin);
        auto row_end   = dvlab::iterator::next(_matrix[row_idx].get_row().begin(), section_end);
        // NOTE - not all vector, only consider [row_begin, row_end)
        return std::vector<unsigned char>(row_begin, row_end);
    };

    auto const clear_section_duplicates = [this, get_sub_vec, track](size_t section_begin, size_t section_end, auto row_range) {
        std::unordered_map<std::vector<unsigned char>, size_t, BooleanMatrixRowHash> duplicated;
        for (auto row_idx : row_range) {
            auto sub_vec = get_sub_vec(row_idx, section_begin, section_end);

            if (std::ranges::all_of(sub_vec, [](unsigned char const& e) { return e == 0; })) continue;

            if (duplicated.contains(sub_vec)) {
                row_operation(duplicated[sub_vec], row_idx, track);
            } else {
                duplicated[sub_vec] = row_idx;
            }
        }
    };

    auto const clear_all_1s_in_column = [this, track](size_t pivot_row_idx, size_t col_idx, auto row_range) {
        auto rows_to_clear = row_range | std::views::filter([this, col_idx](size_t row_idx) -> bool {
                                 return _matrix[row_idx][col_idx] == 1;
                             });
        std::ranges::for_each(rows_to_clear, [this, pivot_row_idx, track](size_t row_idx) { row_operation(pivot_row_idx, row_idx, track); });
    };

    auto n_sections = gsl::narrow_cast<size_t>(ceil(static_cast<double>(num_cols()) / static_cast<double>(block_size)));
    std::vector<size_t> pivots;  // the ith elements is the column index of the pivot of the ith row,
                                 // where a pivot is the first non-zero element in a row below the current row

    for (auto section_idx : std::views::iota(0u, n_sections)) {
        auto [section_begin, section_end] = get_section_range(section_idx);
        clear_section_duplicates(section_begin, section_end, std::views::iota(pivots.size(), num_rows()));

        for (auto col_idx : std::views::iota(section_begin, section_end)) {
            auto const row_idx =
                std::ranges::find_if(
                    dvlab::iterator::next(_matrix.begin(), pivots.size()), _matrix.end(),
                    [col_idx](Row const& row) -> bool {
                        return row[col_idx] == 1;
                    }) -
                _matrix.begin();

            if (std::cmp_greater_equal(row_idx, num_rows())) continue;

            // ensures that the pivot row has a 1 in the current column
            if (std::cmp_not_equal(row_idx, pivots.size())) {
                row_operation(row_idx, pivots.size(), track);
            }

            clear_all_1s_in_column(pivots.size(), col_idx, std::views::iota(pivots.size() + 1, num_rows()));

            // records the current columns for fully-reduced
            pivots.emplace_back(col_idx);
        }
    }
    auto const rank = pivots.size();

    // NOTE - at this point the matrix is in row echelon form
    //        https://en.wikipedia.org/wiki/Row_echelon_form

    if (!do_fully_reduced || rank == 0) return rank;

    for (auto section_idx : std::views::iota(0u, n_sections) | std::views::reverse) {
        auto [section_begin, section_end] = get_section_range(section_idx);

        clear_section_duplicates(section_begin, section_end, std::views::iota(0u, pivots.size()) | std::views::reverse);

        while (!pivots.empty() && section_begin <= pivots.back() && pivots.back() < section_end) {
            // retrieves the last pivot column. This column is guaranteed to have a 1 in the pivot row
            auto last = pivots.back();
            pivots.pop_back();

            clear_all_1s_in_column(pivots.size(), last, std::views::iota(0u, pivots.size()));

            if (pivots.empty()) return rank;
        }
    }

    return rank;
}

size_t BooleanMatrix::matrix_rank() const {
    auto copy = *this;
    return copy.gaussian_elimination_skip(num_cols(), false, false);
}

/**
 * @brief A temporary method to filter duplicated operations
 *
 * @return size_t
 */
size_t BooleanMatrix::filter_duplicate_row_operations() {
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
 * @brief Perform Gaussian Elimination with augmentation column(s)
 *
 * @param track if true, record the process to operation track
 * @return true
 * @return false
 */
bool BooleanMatrix::gaussian_elimination_augmented(bool track) {
    _row_operations.clear();

    auto const num_variables = num_cols() - 1;

    size_t cur_row = 0, cur_col = 0;

    while (cur_row < num_rows() && cur_col < num_variables) {
        // skip columns of all zeros
        if (all_of(_matrix.begin(), _matrix.end(), [&cur_col](Row const& row) -> bool {
                return row[cur_col] == 0;
            })) {
            cur_col++;
            continue;
        }

        // make current element a 1
        if (_matrix[cur_row][cur_col] == 0) {
            auto const the_first_row_with_one = find_if(_matrix.begin() + static_cast<ssize_t>(cur_row), _matrix.end(), [&cur_col](Row const& row) -> bool {
                                                    return row[cur_col] == 1;
                                                }) -
                                                _matrix.begin();

            if (std::cmp_greater_equal(the_first_row_with_one, num_rows())) {  // cannot find rows with independent equation for current variable
                cur_col++;
                continue;
            }

            row_operation(the_first_row_with_one, cur_row, track);
        }

        // make other elements on the same column 0
        for (size_t r = 0; r < num_rows(); ++r) {
            if (r != cur_row && _matrix[r][cur_col] == 1) {
                row_operation(cur_row, r, track);
            }
        }

        cur_row++;
        cur_col++;
    }

    return none_of(dvlab::iterator::next(_matrix.begin(), cur_row), _matrix.end(), [](Row const& row) -> bool {
        return row.back() == 1;
    });
}

/**
 * @brief Get depth of operations
 *
 * @return size_t
 */
size_t BooleanMatrix::row_operation_depth() {
    std::vector<size_t> row_depth;
    row_depth.resize(num_rows(), 0);
    if (_row_operations.empty()) {
        return 0;
    }
    for (auto const& [a, b] : _row_operations) {
        auto const max_depth = std::max(row_depth[a], row_depth[b]);
        row_depth[a]         = max_depth + 1;
        row_depth[b]         = max_depth + 1;
    }
    return *max_element(row_depth.begin(), row_depth.end());
}

/**
 * @brief Get dense ratio of operations
 *
 * @return float
 */
double BooleanMatrix::dense_ratio() {
    auto const depth = row_operation_depth();
    if (depth == 0)
        return 0;
    float const ratio = float(depth) / float(_row_operations.size());
    return std::round(ratio * 100) / 100;
}

/**
 * @brief Push a new column at the end of the matrix
 *
 */
void BooleanMatrix::push_zeros_column() {
    for_each(_matrix.begin(), _matrix.end(), [](Row& r) { r.emplace_back(0); });
}

bool BooleanMatrix::Row::operator==(Row const& rhs) const {
    if (_row.size() != rhs._row.size()) return false;
    for (size_t i = 0; i < _row.size(); i++) {
        if (_row[i] != rhs._row[i]) return false;
    }
    return true;
}

dvlab::BooleanMatrix vstack(dvlab::BooleanMatrix const& a, dvlab::BooleanMatrix const& b) {
    if (b.num_rows() == 0) return a;
    if (a.num_rows() == 0) return b;
    assert(a.num_cols() == b.num_cols());
    auto ret = dvlab::BooleanMatrix();
    ret.reserve(a.num_rows() + b.num_rows(), a.num_cols());
    for (auto const& row : a.get_matrix()) {
        ret.push_row(row);
    }
    for (auto const& row : b.get_matrix()) {
        ret.push_row(row);
    }
    return ret;
}

dvlab::BooleanMatrix hstack(dvlab::BooleanMatrix const& a, dvlab::BooleanMatrix const& b) {
    if (b.num_cols() == 0) return a;
    if (a.num_cols() == 0) return b;
    assert(a.num_rows() == b.num_rows());
    auto ret = dvlab::BooleanMatrix();
    ret.reserve(a.num_rows(), a.num_cols() + b.num_cols());
    for (size_t i = 0; i < a.num_rows(); i++) {
        auto row = a.get_row(i).get_row();
        row.insert(row.end(), b.get_row(i).get_row().begin(), b.get_row(i).get_row().end());
        ret.push_row(row);
    }
    return ret;
}

dvlab::BooleanMatrix transpose(dvlab::BooleanMatrix const& matrix) {
    auto ret = dvlab::BooleanMatrix();
    ret.reserve(matrix.num_cols(), matrix.num_rows());
    for (size_t i = 0; i < matrix.num_cols(); i++) {
        std::vector<unsigned char> row;
        for (size_t j = 0; j < matrix.num_rows(); j++) {
            row.push_back(matrix.get_row(j).get_row()[i]);
        }
        ret.push_row(row);
    }
    return ret;
}

dvlab::BooleanMatrix identity(size_t size) {
    auto ret = dvlab::BooleanMatrix(size, size);
    ret.reserve(size, size);
    for (size_t i = 0; i < size; i++) {
        ret[i][i] = 1;
    }
    return ret;
}

}  // namespace dvlab
