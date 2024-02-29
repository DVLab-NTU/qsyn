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

struct UCharVectorHash {
    size_t operator()(std::vector<unsigned char> const& k) const {
        size_t ret = std::hash<unsigned char>()(k[0]);
        for (size_t i = 1; i < k.size(); i++) {
            ret ^= std::hash<unsigned char>()(k[i] << (i % sizeof(size_t)));
        }

        return ret;
    }
};

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
    for (auto const& row : _matrix) {
        row.print_row(lvl);
    }
}

/**
 * @brief Print track of operations
 *
 */
void BooleanMatrix::print_trace() const {
    fmt::println("Track:");
    for (auto const& [i, row_op] : tl::views::enumerate(_row_operations)) {
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
    auto get_section_range = [block_size, this](size_t section_idx) {
        auto section_begin = section_idx * block_size;
        auto section_end   = std::min(num_cols(), (section_idx + 1) * block_size);
        return std::make_pair(section_begin, section_end);
    };

    auto get_sub_vec = [this](size_t row_idx, size_t section_begin, size_t section_end) {
        auto row_begin = dvlab::iterator::next(_matrix[row_idx].get_row().begin(), section_begin);
        auto row_end   = dvlab::iterator::next(_matrix[row_idx].get_row().begin(), section_end);
        // NOTE - not all vector, only consider [row_begin, row_end)
        return std::vector<unsigned char>(row_begin, row_end);
    };

    auto clear_section_duplicates = [this, get_sub_vec, track](size_t section_begin, size_t section_end, auto row_range) {
        std::unordered_map<std::vector<unsigned char>, size_t, UCharVectorHash> duplicated;
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

    auto clear_all_1s_in_column = [this, track](size_t pivot_row_idx, size_t col_idx, auto row_range) {
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
            if (do_fully_reduced) pivots.emplace_back(col_idx);
        }
    }
    auto const rank = pivots.size();

    // NOTE - at this point the matrix is in row echelon form
    //        https://en.wikipedia.org/wiki/Row_echelon_form

    if (!do_fully_reduced || rank == 0) return rank;

    for (auto section_idx : std::views::iota(0u, n_sections) | std::views::reverse) {
        auto [section_begin, section_end] = get_section_range(section_idx);

        clear_section_duplicates(section_begin, section_end, std::views::iota(0u, pivots.size()) | std::views::reverse);

        while (pivots.size() > 0 && section_begin <= pivots.back() && pivots.back() < section_end) {
            // retrieves the last pivot column. This column is guaranteed to have a 1 in the pivot row
            auto last = pivots.back();
            pivots.pop_back();

            clear_all_1s_in_column(pivots.size(), last, std::views::iota(0u, pivots.size()));

            if (pivots.empty()) return rank;
        }
    }

    return rank;
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
 * @brief Perform Gaussian Elimination
 *
 * @param track if true, record the process to operation track
 * @param isAugmentedMatrix the target matrix is augmented or not
 * @return true
 * @return false
 */
bool BooleanMatrix::gaussian_elimination(bool track, bool is_augmented_matrix) {
    _row_operations.clear();

    auto const num_variables = num_cols() - ((is_augmented_matrix) ? 1 : 0);

    /**
     * @brief If _matrix[i][i] is 0, greedily perform row operations
     * to make the number 1
     *
     * @return true on success, false on failures
     */
    auto make_main_diagonal_one = [this, &track](size_t i) -> bool {
        if (_matrix[i][i] == 1) return true;
        for (size_t j = i + 1; j < num_rows(); j++) {
            if (_matrix[j][i] == 1) {
                row_operation(j, i, track);
                return true;
            }
        }
        return false;
    };

    // convert to upper-triangle matrix
    for (size_t i = 0; i < std::min(num_rows() - 1, num_variables); i++) {
        // the system of equation is not solvable if the
        // main diagonal cannot be made 1
        if (!make_main_diagonal_one(i)) return false;

        for (size_t j = i + 1; j < num_rows(); j++) {
            if (_matrix[j][i] == 1 && _matrix[i][i] == 1) {
                row_operation(i, j, track);
            }
        }
    }

    // for augmented matrix, if any rows looks like [0 ... 0 1],
    // the system has no solution
    if (is_augmented_matrix) {
        for (size_t i = num_variables; i < num_rows(); ++i) {
            if (_matrix[i].back() == 1) return false;
        }
    }

    // convert to identity matrix on the leftmost numRows() matrix
    for (size_t i = 0; i < num_rows(); i++) {
        for (size_t j = num_rows() - i; j < num_rows(); j++) {
            if (_matrix[num_rows() - i - 1][j] == 1) {
                row_operation(j, num_rows() - i - 1, track);
            }
        }
    }
    return true;
}

/**
 * @brief check if the matrix is of solved form. That is,
 *        (1) an identity matrix,
 *        (2) an identity matrix with an arbitrary matrix on the right, or
 *        (3) an identity matrix with an zero matrix on the bottom.
 *
 * @return true
 * @return false
 */
bool BooleanMatrix::is_solved_form() const {
    for (size_t i = 0; i < num_rows(); ++i) {
        for (size_t j = 0; j < std::min(num_rows(), num_cols()); ++j) {
            if (i == j && _matrix[i][j] != 1) return false;
            if (i != j && _matrix[i][j] != 0) return false;
        }
    }

    return true;
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
 * @brief check if the augmented matrix is of solved form. That is,
 *        an identity matrix with an arbitrary matrix on the right, and possibly
 *        an identity matrix with an zero matrix on the bottom.
 *
 * @return true or false
 */
bool BooleanMatrix::is_augmented_solved_form() const {
    auto const n = std::min(num_rows(), num_cols() - 1);
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i == j && _matrix[i][j] != 1) return false;
            if (i != j && _matrix[i][j] != 0) return false;
        }
    }
    for (size_t i = n; i < num_rows(); ++i) {
        for (size_t j = 0; j < num_cols(); ++j) {
            if (_matrix[i][j] != 0) return false;
        }
    }

    return true;
}

/**
 * @brief Append one hot
 *
 * @param idx the id to be one
 */
void BooleanMatrix::append_one_hot_column(size_t idx) {
    assert(idx < _matrix.size());
    for (size_t i = 0; i < _matrix.size(); ++i) {
        _matrix[i].emplace_back((i == idx) ? 1 : 0);
    }
}

/**
 * @brief Get depth of operations
 *
 * @return size_t
 */
size_t BooleanMatrix::row_operation_depth() {
    std::vector<size_t> row_depth;
    row_depth.resize(num_rows(), 0);
    if (_row_operations.size() == 0) {
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
    return round(ratio * 100) / 100;
}

/**
 * @brief Push a new column at the end of the matrix
 *
 */
void BooleanMatrix::push_zeros_column() {
    for_each(_matrix.begin(), _matrix.end(), [](Row& r) { r.emplace_back(0); });
}

}  // namespace dvlab
