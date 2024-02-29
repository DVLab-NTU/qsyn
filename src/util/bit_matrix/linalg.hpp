/**
 * @file row_operation.hpp
 * @brief define row operation strategies for matrix-like data
 *
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 *
 */

#pragma once

#include "./bit_matrix.hpp"
#include "util/util.hpp"

namespace dvlab::bit_matrix {

struct UCharVectorHash {
    size_t operator()(std::vector<unsigned char> const& k) const {
        size_t ret = std::hash<unsigned char>()(k[0]);
        for (size_t i = 1; i < k.size(); i++) {
            ret ^= std::hash<unsigned char>()(k[i] << (i % sizeof(size_t)));
        }

        return ret;
    }
};

template <typename T>
concept RowOpAvailable = requires(T t) {
    { row_operation(t, 0ul, 1ul) };
};

bool gaussian_elimination(RowOpAvailable auto& matrix);
bool gaussian_elimination_augmented(RowOpAvailable auto& matrix);
size_t gaussian_elimination_skip(RowOpAvailable auto& matrix, size_t block_size, bool do_fully_reduced);

size_t matrix_rank(RowOpAvailable auto matrix /* copy on purpose */);

/**
 * @brief Perform Gaussian Elimination
 *
 * @param track if true, record the process to operation track
 * @param isAugmentedMatrix the target matrix is augmented or not
 * @return true
 * @return false
 */
bool gaussian_elimination(RowOpAvailable auto& matrix) {
    matrix.get_row_operations().clear();

    auto const num_variables = matrix.num_cols();

    /**
     * @brief If _matrix[i][i] is 0, greedily perform row operations
     * to make the number 1
     *
     * @return true on success, false on failures
     */
    auto make_main_diagonal_one = [&](size_t i) -> bool {
        if (matrix[i][i] == 1) return true;
        for (size_t j = i + 1; j < matrix.num_rows(); j++) {
            if (matrix[j][i] == 1) {
                row_operation(matrix, j, i);
                return true;
            }
        }
        return false;
    };

    // convert to upper-triangle matrix
    for (size_t i = 0; i < std::min(matrix.num_rows() - 1, num_variables); i++) {
        // the system of equation is not solvable if the
        // main diagonal cannot be made 1
        if (!make_main_diagonal_one(i)) return false;

        for (size_t j = i + 1; j < matrix.num_rows(); j++) {
            if (matrix[j][i] == 1 && matrix[i][i] == 1) {
                row_operation(matrix, i, j);
            }
        }
    }

    // convert to identity matrix on the leftmost numRows() matrix
    for (size_t i = 0; i < matrix.num_rows(); i++) {
        for (size_t j = matrix.num_rows() - i; j < matrix.num_rows(); j++) {
            if (matrix[matrix.num_rows() - i - 1][j] == 1) {
                row_operation(matrix, j, matrix.num_rows() - i - 1);
            }
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
bool gaussian_elimination_augmented(RowOpAvailable auto& matrix) {
    matrix.get_row_operations().clear();

    auto const num_variables = matrix.num_cols() - 1;

    size_t cur_row = 0, cur_col = 0;

    while (cur_row < matrix.num_rows() && cur_col < num_variables) {
        // skip columns of all zeros
        if (std::ranges::all_of(matrix, [&cur_col](BitMatrix::Row const& row) -> bool {
                return row[cur_col] == 0;
            })) {
            cur_col++;
            continue;
        }

        // make current element a 1
        if (matrix[cur_row][cur_col] == 0) {
            auto const the_first_row_with_one = find_if(
                                                    matrix.begin() + static_cast<ssize_t>(cur_row),
                                                    matrix.end(),
                                                    [&cur_col](BitMatrix::Row const& row) -> bool {
                                                        return row[cur_col] == 1;
                                                    }) -
                                                matrix.begin();

            if (std::cmp_greater_equal(the_first_row_with_one, matrix.num_rows())) {  // cannot find rows with independent equation for current variable
                cur_col++;
                continue;
            }

            row_operation(matrix, the_first_row_with_one, cur_row);
        }

        // make other elements on the same column 0
        for (size_t r = 0; r < matrix.num_rows(); ++r) {
            if (r != cur_row && matrix[r][cur_col] == 1) {
                row_operation(matrix, cur_row, r);
            }
        }

        cur_row++;
        cur_col++;
    }

    return std::none_of(dvlab::iterator::next(matrix.begin(), cur_row), matrix.end(), [](BitMatrix::Row const& row) -> bool {
        return row.back() == 1;
    });
}

/**
 * @brief Perform Gaussian Elimination with `block size`. Skip the column if it is duplicated.
 *
 * @param blockSize
 * @param fullReduced if true, performing back-substitution from the echelon form
 * @param track if true, record the process to operation track
 * @return size_t (rank)
 */
size_t gaussian_elimination_skip(RowOpAvailable auto& matrix, size_t block_size, bool do_fully_reduced) {
    auto const get_section_range = [block_size, &matrix](size_t section_idx) {
        auto const section_begin = section_idx * block_size;
        auto const section_end   = std::min(matrix.num_cols(), (section_idx + 1) * block_size);
        return std::make_pair(section_begin, section_end);
    };

    auto const get_sub_vec = [&matrix](size_t row_idx, size_t section_begin, size_t section_end) {
        auto const row_begin = dvlab::iterator::next(matrix[row_idx].begin(), section_begin);
        auto const row_end   = dvlab::iterator::next(matrix[row_idx].begin(), section_end);
        // NOTE - not all vector, only consider [row_begin, row_end)
        return std::vector<unsigned char>(row_begin, row_end);
    };

    auto const clear_section_duplicates = [&matrix, get_sub_vec](size_t section_begin, size_t section_end, auto row_range) {
        std::unordered_map<std::vector<unsigned char>, size_t, UCharVectorHash> duplicated;
        for (auto row_idx : row_range) {
            auto const sub_vec = get_sub_vec(row_idx, section_begin, section_end);

            if (std::ranges::all_of(sub_vec, [](unsigned char const& e) { return e == 0; })) continue;

            if (duplicated.contains(sub_vec)) {
                row_operation(matrix, duplicated[sub_vec], row_idx);
            } else {
                duplicated[sub_vec] = row_idx;
            }
        }
    };

    auto const clear_all_1s_in_column = [&matrix](size_t pivot_row_idx, size_t col_idx, auto row_range) {
        auto const rows_to_clear = row_range | std::views::filter([&matrix, col_idx](size_t row_idx) -> bool {
                                       return matrix[row_idx][col_idx] == 1;
                                   });
        std::ranges::for_each(rows_to_clear, [&matrix, pivot_row_idx](size_t row_idx) { row_operation(matrix, pivot_row_idx, row_idx); });
    };

    auto const n_sections = gsl::narrow_cast<size_t>(ceil(static_cast<double>(matrix.num_cols()) / static_cast<double>(block_size)));
    std::vector<size_t> pivots;  // the ith elements is the column index of the pivot of the ith row,
                                 // where a pivot is the first non-zero element in a row below the current row

    for (auto const section_idx : std::views::iota(0u, n_sections)) {
        auto const [section_begin, section_end] = get_section_range(section_idx);
        clear_section_duplicates(section_begin, section_end, std::views::iota(pivots.size(), matrix.num_rows()));

        for (auto col_idx : std::views::iota(section_begin, section_end)) {
            auto const row_idx =
                std::ranges::find_if(
                    dvlab::iterator::next(matrix.begin(), pivots.size()), matrix.end(),
                    [col_idx](BitMatrix::Row const& row) -> bool {
                        return row[col_idx] == 1;
                    }) -
                matrix.begin();

            if (std::cmp_greater_equal(row_idx, matrix.num_rows())) continue;

            // ensures that the pivot row has a 1 in the current column
            if (std::cmp_not_equal(row_idx, pivots.size())) {
                row_operation(matrix, row_idx, pivots.size());
            }

            clear_all_1s_in_column(pivots.size(), col_idx, std::views::iota(pivots.size() + 1, matrix.num_rows()));

            // records the current columns for fully-reduced
            if (do_fully_reduced) pivots.emplace_back(col_idx);
        }
    }
    auto const rank = pivots.size();

    // NOTE - at this point the matrix is in row echelon form
    //        https://en.wikipedia.org/wiki/Row_echelon_form

    if (!do_fully_reduced || rank == 0) return rank;

    for (auto const section_idx : std::views::iota(0u, n_sections) | std::views::reverse) {
        auto const [section_begin, section_end] = get_section_range(section_idx);

        clear_section_duplicates(section_begin, section_end, std::views::iota(0u, pivots.size()) | std::views::reverse);

        while (pivots.size() > 0 && section_begin <= pivots.back() && pivots.back() < section_end) {
            // retrieves the last pivot column. This column is guaranteed to have a 1 in the pivot row
            auto const last = pivots.back();
            pivots.pop_back();

            clear_all_1s_in_column(pivots.size(), last, std::views::iota(0u, pivots.size()));

            if (pivots.empty()) return rank;
        }
    }

    return rank;
}

size_t matrix_rank(RowOpAvailable auto matrix /* copy on purpose */) {
    gaussian_elimination(matrix);
    return std::ranges::count_if(matrix, [](BitMatrix::Row const& row) -> bool {
        return std::ranges::any_of(row, [](unsigned char const& e) -> bool {
            return e == 1;
        });
    });
}

}  // namespace dvlab::bit_matrix
