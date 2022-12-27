/****************************************************************************
  FileName     [ m2.cpp ]
  PackageName  [ m2 ]
  Synopsis     [ Define class m2 member functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "m2.h"

#include <algorithm>
#include <cmath>
extern size_t verbose;
using namespace std;

namespace std {
template <>
struct hash<vector<unsigned char>> {
    size_t operator()(const vector<unsigned char>& k) const {
        size_t ret = hash<unsigned char>()(k[0]);
        for (size_t i = 1; i < k.size(); i++) {
            ret ^= hash<unsigned char>()(k[i] << (i % sizeof(size_t)));
        }

        return ret;
    }
};
}  // namespace std

Row operator+(Row& lhs, const Row& rhs) {
    lhs += rhs;
    return lhs;
}

Row& Row::operator+=(const Row& rhs) {
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
void Row::printRow() const {
    for (auto e : _row) {
        cout << unsigned(e) << " ";
    }
    cout << endl;
}

/**
 * @brief @brief Check is one hot
 *
 * @return true if one hot
 * @return false if not
 */
bool Row::isOneHot() const {
    size_t cnt = 0;
    for (auto& i : _row) {
        if (i == 1) {
            if (cnt == 1) return false;
            ++cnt;
        }
    }
    return (cnt == 1);
}

bool Row::isZeros() const {
    for (auto& i : _row) {
        if (i == 1) {
            return false;
        }
    }
    return true;
}
void M2::reset() {
    _matrix.clear();
    _opStorage.clear();
}
/**
 * @brief @brief Print matrix
 *
 */
void M2::printMatrix() const {
    cout << "M2 matrix:" << endl;
    for (const auto& row : _matrix) {
        row.printRow();
    }
    cout << endl;
}

/**
 * @brief Print the operation track
 *
 */
void M2::printTrack() const {
    cout << "Track:" << endl;
    for (size_t i = 0; i < _opStorage.size(); i++) {
        cout << "Step " << i + 1 << ": " << _opStorage[i].first << " to " << _opStorage[i].second << endl;
    }
    cout << endl;
}

/**
 * @brief initialize a test matrix
 *
 */
void M2::defaultInit() {
    _matrix.push_back(Row(0, vector<unsigned char>{1, 0, 1, 1, 0, 1}));
    _matrix.push_back(Row(1, vector<unsigned char>{0, 1, 1, 1, 0, 0}));
    _matrix.push_back(Row(2, vector<unsigned char>{0, 1, 1, 0, 1, 0}));
    _matrix.push_back(Row(3, vector<unsigned char>{1, 0, 0, 1, 1, 0}));
    _matrix.push_back(Row(4, vector<unsigned char>{1, 1, 0, 1, 1, 0}));
    _matrix.push_back(Row(5, vector<unsigned char>{0, 0, 0, 1, 0, 1}));
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
bool M2::xorOper(size_t ctrl, size_t targ, bool track) {
    if (ctrl >= _matrix.size()) {
        cerr << "Error: wrong dimension " << ctrl << endl;
        return false;
    }
    if (targ >= _matrix.size()) {
        cerr << "Error: wrong dimension " << targ << endl;
        return false;
    }
    _matrix[targ] += _matrix[ctrl];
    if (track) _opStorage.emplace_back(ctrl, targ);
    return true;
}

/**
 * @brief Perform Gaussian Elimination. Skip the column if it is dupicated.
 *
 * @param track
 * @return true
 * @return false
 */
bool M2::gaussianElimSkip(bool track) {
    vector<size_t> pivot_cols, pivot_cols_backup;
    size_t pivot_row = 0;
    unordered_map<vector<unsigned char>, size_t> duplicated;
    for (size_t i = 0; i < numRows(); i++) {
        if (_matrix[i].isZeros()) continue;
        if (duplicated.contains(_matrix[i].getRow())) {
            xorOper(duplicated[_matrix[i].getRow()], i, track);
        } else {
            duplicated[_matrix[i].getRow()] = i;
        }
    }
    size_t p = 0;
    while (p < numCols()) {
        for (size_t r0 = pivot_row; r0 < numRows(); r0++) {
            if (_matrix[r0].getRow()[p] != 0) {
                if (r0 != pivot_row) {
                    xorOper(r0, pivot_row, track);
                }

                for (size_t r1 = pivot_row + 1; r1 < numRows(); r1++) {
                    if (_matrix[r1].getRow()[p] != 0) {
                        xorOper(pivot_row, r1, track);
                    }
                }
                pivot_cols.push_back(p);
                pivot_row++;
                break;
            }
        }
        p++;
    }

    pivot_row--;
    pivot_cols_backup = pivot_cols;

    duplicated.clear();
    for (size_t i = pivot_row; i > 0; i--) {
        if (_matrix[i].isZeros()) continue;
        if (duplicated.contains(_matrix[i].getRow())) {
            xorOper(duplicated[_matrix[i].getRow()], i, track);
        } else {
            duplicated[_matrix[i].getRow()] = i;
        }
    }
    // NOTE - 0 <= pivot_cols_backup[i] < numRows() is true
    while (pivot_cols_backup.size() > 0) {
        size_t pcol = pivot_cols_backup[pivot_cols_backup.size() - 1];
        pivot_cols_backup.pop_back();
        for (size_t r = 0; r < pivot_row; r++) {
            if (_matrix[r].getRow()[pcol] != 0) {
                xorOper(pivot_row, r, track);
            }
        }
        pivot_row--;
    }

    return true;
}

/**
 * @brief Perform Gaussian Elimination
 *
 * @param track if true, record the process to operation track
 */
bool M2::gaussianElim(bool track, bool isAugmentedMatrix) {
    if (verbose >= 5) cout << "Performing Gaussian Elimination..." << endl;
    if (verbose >= 8) printMatrix();
    _opStorage.clear();

    size_t numVariables = numCols() - ((isAugmentedMatrix) ? 1 : 0);

    /**
     * @brief If _matrix[i][i] is 0, greedily perform row operations
     * to make the number 1
     *
     * @return true on success, false on failures
     */
    auto makeMainDiagonalOne = [this, &track](size_t i) -> bool {
        if (_matrix[i][i] == 1) return true;
        for (size_t j = i + 1; j < numRows(); j++) {
            if (_matrix[j][i] == 1) {
                xorOper(j, i, track);
                if (verbose >= 8) {
                    cout << "Diag Add " << j << " to " << i << endl;
                    printMatrix();
                }
                return true;
            }
        }
        return false;
    };

    // convert to upper-triangle matrix
    for (size_t i = 0; i < min(numRows() - 1, numVariables); i++) {
        // the system of equation is not solvable if the
        // main diagonal cannot be made 1
        // REVIEW - I comment out this line since no routine in Gaussian do this?
        if (!makeMainDiagonalOne(i)) return false;

        for (size_t j = i + 1; j < numRows(); j++) {
            if (_matrix[j][i] == 1 && _matrix[i][i] == 1) {
                xorOper(i, j, track);
                if (verbose >= 8) {
                    cout << "Add " << i << " to " << j << endl;
                    printMatrix();
                }
            }
        }
    }

    // for augmented matrix, if any rows looks like [0 ... 0 1],
    // the system has no solution
    if (isAugmentedMatrix) {
        for (size_t i = numVariables; i < numRows(); ++i) {
            if (_matrix[i].back() == 1) return false;
        }
    }

    // convert to identity matrix on the leftmost numRows() matrix
    for (size_t i = 0; i < numRows(); i++) {
        for (size_t j = numRows() - i; j < numRows(); j++) {
            if (_matrix[numRows() - i - 1][j] == 1) {
                xorOper(j, numRows() - i - 1, track);
                if (verbose >= 8) {
                    cout << "Add " << j << " to " << numRows() - i - 1 << endl;
                    printMatrix();
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
 * @return true or false
 */
bool M2::isSolvedForm() const {
    for (size_t i = 0; i < numRows(); ++i) {
        for (size_t j = 0; j < min(numRows(), numCols()); ++j) {
            if (i == j && _matrix[i][j] != 1) return false;
            if (i != j && _matrix[i][j] != 0) return false;
        }
    }

    return true;
}

bool M2::gaussianElimAugmented(bool track) {
    if (verbose >= 5) cout << "Performing Gaussian Elimination..." << endl;
    if (verbose >= 8) printMatrix();
    _opStorage.clear();

    size_t numVariables = numCols() - 1;

    size_t curRow = 0, curCol = 0;

    while (curRow < numRows() && curCol < numVariables) {
        // skip columns of all zeros
        if (all_of(_matrix.begin(), _matrix.end(), [&curCol](const Row& row) -> bool {
                return row[curCol] == 0;
            })) {
            curCol++;
            continue;
        }

        // make current element a 1
        if (_matrix[curRow][curCol] == 0) {
            size_t theFirstRowWithOne = find_if(_matrix.begin() + curRow, _matrix.end(), [&curCol](const Row& row) -> bool {
                                            return row[curCol] == 1;
                                        }) -
                                        _matrix.begin();

            if (theFirstRowWithOne >= numRows()) {  // cannot find rows with independent equation for current variable
                curCol++;
                continue;
            }

            xorOper(theFirstRowWithOne, curRow, track);
            if (verbose >= 8) {
                cout << "Add " << theFirstRowWithOne << " to " << curRow << endl;
                printMatrix();
            }
        }

        // make other elements on the same column 0
        for (size_t r = 0; r < numRows(); ++r) {
            if (r != curRow && _matrix[r][curCol] == 1) {
                xorOper(curRow, r, track);
                if (verbose >= 8) {
                    cout << "Add " << curRow << " to " << r << endl;
                    printMatrix();
                }
            }
        }

        curRow++;
        curCol++;
    }

    return none_of(_matrix.begin() + curRow, _matrix.end(), [](const Row& row) -> bool {
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
bool M2::isAugmentedSolvedForm() const {
    size_t n = min(numRows(), numCols() - 1);
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i == j && _matrix[i][j] != 1) return false;
            if (i != j && _matrix[i][j] != 0) return false;
        }
    }
    for (size_t i = n; i < numRows(); ++i) {
        for (size_t j = 0; j < numCols(); ++j) {
            if (_matrix[i][j] != 0) return false;
        }
    }

    return true;
}

/**
 * @brief Build matrix from ZX-graph (according to the given order)
 *
 * @param frontier
 * @param neighbors
 * @return true if successfully built,
 * @return false if not
 */
bool M2::fromZXVertices(const ZXVertexList& frontier, const ZXVertexList& neighbors) {
    // if (frontier.size() != neighbors.size()) {
    //     cout << "Numbers of elements in frontier and neighbors mismatch!" << endl;
    //     return false;
    // }
    // NOTE - assign row by calculating a Frontier's connecting status to Neighbors, e.g. 10010 = connect to qubit 0 and 3.
    reset();
    unordered_map<ZXVertex*, size_t> table;
    size_t cnt = 0;
    for (auto& v : neighbors) {
        table[v] = cnt;
        cnt++;
    }
    for (auto& v : frontier) {
        vector<unsigned char> storage = vector<unsigned char>(neighbors.size(), 0);
        for (auto& [vt, _] : v->getNeighbors()) {
            if (neighbors.contains(vt)) {
                // REVIEW - Assume no space in #qubit (0,2,3,4,5 is not allowed)
                storage[table[vt]] = 1;
                // in Neighbors
            }
        }
        _matrix.push_back(Row(1, storage));
    }

    return true;
}

void M2::appendOneHot(size_t idx) {
    assert(idx < _matrix.size());
    for (size_t i = 0; i < _matrix.size(); ++i) {
        _matrix[i].push_back((i == idx) ? 1 : 0);
    }
}
