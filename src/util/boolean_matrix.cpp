/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define class BooleanMatrix member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./boolean_matrix.hpp"

#include <cassert>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "zx/zxgraph.hpp"

extern size_t VERBOSE;

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
void BooleanMatrix::Row::print_row() const {
    for (auto e : _row) {
        std::cout << unsigned(e) << " ";
    }
    std::cout << std::endl;
}

/**
 * @brief @brief Check if the row is one hot
 *
 * @return true
 * @return false
 */
bool BooleanMatrix::Row::is_one_hot() const {
    size_t cnt = 0;
    for (auto& i : _row) {
        if (i == 1) {
            if (cnt == 1) return false;
            ++cnt;
        }
    }
    return (cnt == 1);
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
void BooleanMatrix::print_matrix() const {
    std::cout << "M2 matrix:" << std::endl;
    for (auto const& row : _matrix) {
        row.print_row();
    }
    std::cout << std::endl;
}

/**
 * @brief Print track of operations
 *
 */
void BooleanMatrix::print_trace() const {
    std::cout << "Track:" << std::endl;
    for (size_t i = 0; i < _row_operations.size(); i++) {
        std::cout << "Step " << i + 1 << ": " << _row_operations[i].first << " to " << _row_operations[i].second << std::endl;
    }
    std::cout << std::endl;
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
        std::cerr << "Error: wrong dimension " << ctrl << std::endl;
        return false;
    }
    if (targ >= _matrix.size()) {
        std::cerr << "Error: wrong dimension " << targ << std::endl;
        return false;
    }
    _matrix[targ] += _matrix[ctrl];
    if (track) _row_operations.emplace_back(ctrl, targ);
    return true;
}

/**
 * @brief Perform Gaussian Elimination with different block sizes. Skip the column if it is dupicated.
 *
 * @param blockSize
 * @param fullReduced if true, performing back-substitution from the echelon form
 * @param track if true, record the process to operation track
 * @return size_t (rank)
 */
size_t BooleanMatrix::gaussian_elimination_skip(size_t block_size, bool fully_reduced, bool track) {
    std::vector<size_t> pivot_cols, pivot_cols_backup;
    size_t pivot_row = 0;

    for (size_t section = 0; section < ceil(num_cols() / (double)block_size); section++) {
        size_t start = section * block_size;
        size_t end = std::min(num_cols(), (section + 1) * block_size);

        std::unordered_map<std::vector<unsigned char>, size_t, UCharVectorHash> duplicated;
        for (size_t i = pivot_row; i < num_rows(); i++) {
            auto first = _matrix[i].get_row().begin() + start;
            auto last = _matrix[i].get_row().begin() + end;
            // NOTE - not all vector, only consider Row[first:last]
            std::vector<unsigned char> sub_vec(first, last);
            bool zeros = true;
            for (auto& i : sub_vec) {
                if (i == 1) {
                    zeros = false;
                    break;
                }
            }
            if (zeros) continue;
            if (duplicated.contains(sub_vec)) {
                row_operation(duplicated[sub_vec], i, track);
            } else {
                duplicated[sub_vec] = i;
            }
        }

        size_t p = start;

        while (p < end) {
            for (size_t r0 = pivot_row; r0 < num_rows(); r0++) {
                if (_matrix[r0].get_row()[p] != 0) {
                    if (r0 != pivot_row) {
                        row_operation(r0, pivot_row, track);
                    }

                    for (size_t r1 = pivot_row + 1; r1 < num_rows(); r1++) {
                        if (pivot_row != r1 && _matrix[r1].get_row()[p] != 0) {
                            row_operation(pivot_row, r1, track);
                        }
                    }
                    pivot_cols.emplace_back(p);
                    pivot_row++;
                    break;
                }
            }
            p++;
        }
    }
    size_t rank = pivot_row;
    // NOTE - echelon form already

    if (fully_reduced) {
        pivot_row--;
        pivot_cols_backup = pivot_cols;
        for (int section = ceil(num_cols() / (double)block_size) - 1; section >= 0; section--) {
            size_t start = section * block_size;
            size_t end = std::min(num_cols(), (section + 1) * block_size);

            std::unordered_map<std::vector<unsigned char>, size_t, UCharVectorHash> duplicated;
            for (int i = pivot_row; i >= 0; i--) {
                auto first = _matrix[i].get_row().begin() + start;
                auto last = _matrix[i].get_row().begin() + end;
                // NOTE - not all vector, only consider Row[first:last]
                std::vector<unsigned char> sub_vec(first, last);
                bool zeros = true;
                for (auto& i : sub_vec) {
                    if (i == 1) {
                        zeros = false;
                        break;
                    }
                }
                if (zeros) continue;
                if (duplicated.contains(sub_vec)) {
                    row_operation(duplicated[sub_vec], i, track);
                } else {
                    duplicated[sub_vec] = i;
                }
            }

            while (pivot_cols_backup.size() > 0 && start <= pivot_cols_backup.back() && pivot_cols_backup.back() < end) {
                size_t pcol = pivot_cols_backup[pivot_cols_backup.size() - 1];
                pivot_cols_backup.pop_back();
                for (size_t r = 0; r < pivot_row; r++) {
                    if (_matrix[r].get_row()[pcol] != 0) {
                        row_operation(pivot_row, r, track);
                    }
                }
                pivot_row--;
            }
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
    auto ops_copy = _row_operations;
    std::vector<size_t> dups;
    std::unordered_map<size_t, std::pair<size_t, size_t>> last_used;  // NOTE - bit, (another bit, gateId)
    for (size_t i = 0; i < ops_copy.size(); i++) {
        bool first_match = false, second_match = false;
        if (last_used.contains(ops_copy[i].first) && last_used[ops_copy[i].first].first == ops_copy[i].second && ops_copy[last_used[ops_copy[i].first].second].first == ops_copy[i].first) first_match = true;
        if (last_used.contains(ops_copy[i].second) && last_used[ops_copy[i].second].first == ops_copy[i].first && ops_copy[last_used[ops_copy[i].second].second].second == ops_copy[i].second) second_match = true;
        if (first_match && second_match) {
            dups.emplace_back(i);
            dups.emplace_back(last_used[ops_copy[i].second].second);
            last_used.erase(ops_copy[i].first);
            last_used.erase(ops_copy[i].second);

        } else {
            last_used[ops_copy[i].first] = std::make_pair(ops_copy[i].second, i);
            last_used[ops_copy[i].second] = std::make_pair(ops_copy[i].first, i);
        }
    }
    sort(dups.begin(), dups.end());

    for (size_t i = 0; i < dups.size(); i++) {
        _row_operations.erase(_row_operations.begin() + (dups[i] - i));
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
    if (VERBOSE >= 5) std::cout << "Performing Gaussian Elimination..." << std::endl;
    if (VERBOSE >= 8) print_matrix();
    _row_operations.clear();

    size_t num_variables = num_cols() - ((is_augmented_matrix) ? 1 : 0);

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
                if (VERBOSE >= 8) {
                    std::cout << "Diag Add " << j << " to " << i << std::endl;
                    print_matrix();
                }
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
                if (VERBOSE >= 8) {
                    std::cout << "Add " << i << " to " << j << std::endl;
                    print_matrix();
                }
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
                if (VERBOSE >= 8) {
                    std::cout << "Add " << j << " to " << num_rows() - i - 1 << std::endl;
                    print_matrix();
                }
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
    if (VERBOSE >= 5) std::cout << "Performing Gaussian Elimination..." << std::endl;
    if (VERBOSE >= 9) print_matrix();
    _row_operations.clear();

    size_t num_variables = num_cols() - 1;

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
            size_t the_first_row_with_one = find_if(_matrix.begin() + cur_row, _matrix.end(), [&cur_col](Row const& row) -> bool {
                                                return row[cur_col] == 1;
                                            }) -
                                            _matrix.begin();

            if (the_first_row_with_one >= num_rows()) {  // cannot find rows with independent equation for current variable
                cur_col++;
                continue;
            }

            row_operation(the_first_row_with_one, cur_row, track);
            if (VERBOSE >= 9) {
                std::cout << "Add " << the_first_row_with_one << " to " << cur_row << std::endl;
                print_matrix();
            }
        }

        // make other elements on the same column 0
        for (size_t r = 0; r < num_rows(); ++r) {
            if (r != cur_row && _matrix[r][cur_col] == 1) {
                row_operation(cur_row, r, track);
                if (VERBOSE >= 9) {
                    std::cout << "Add " << cur_row << " to " << r << std::endl;
                    print_matrix();
                }
            }
        }

        cur_row++;
        cur_col++;
    }

    return none_of(_matrix.begin() + cur_row, _matrix.end(), [](Row const& row) -> bool {
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
    size_t n = std::min(num_rows(), num_cols() - 1);
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
 * @brief Build matrix from ZXGraph (according to the given order)
 *
 * @param frontier
 * @param neighbors
 * @return true if successfully built,
 * @return false if not
 */
bool BooleanMatrix::from_zxvertices(qsyn::zx::ZXVertexList const& frontier, qsyn::zx::ZXVertexList const& neighbors) {
    // NOTE - assign row by calculating a Frontier's connecting status to Neighbors, e.g. 10010 = connect to qubit 0 and 3.
    reset();
    std::unordered_map<qsyn::zx::ZXVertex*, size_t> table;
    size_t cnt = 0;
    for (auto& v : neighbors) {
        table[v] = cnt;
        cnt++;
    }
    for (auto& v : frontier) {
        auto storage = std::vector<unsigned char>(neighbors.size(), 0);
        for (auto& [vt, _] : v->get_neighbors()) {
            if (neighbors.contains(vt)) storage[table[vt]] = 1;
        }
        _matrix.emplace_back(storage);
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
        std::cout << "Warning: no row operation" << std::endl;
        return 0;
    }
    for (auto const& [a, b] : _row_operations) {
        size_t max_depth = std::max(row_depth[a], row_depth[b]);
        row_depth[a] = max_depth + 1;
        row_depth[b] = max_depth + 1;
    }
    return *max_element(row_depth.begin(), row_depth.end());
}

/**
 * @brief Get dense of operations
 *
 * @return float
 */
float BooleanMatrix::dense_ratio() {
    size_t depth = row_operation_depth();
    if (depth == 0)
        return 0;
    float ratio = float(depth) / float(_row_operations.size());
    return round(ratio * 100) / 100;
}

/**
 * @brief Push a new column at the end of the matrix
 *
 */
void BooleanMatrix::push_zeros_column() {
    for_each(_matrix.begin(), _matrix.end(), [](Row& r) { r.emplace_back(0); });
}
