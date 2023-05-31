/****************************************************************************
  FileName     [ m2.cpp ]
  PackageName  [ m2 ]
  Synopsis     [ Define class m2 member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "m2.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "zxGraph.h"

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

/**
 * @brief Overload operator + for Row
 *
 * @param lhs
 * @param rhs
 * @return Row
 */
Row operator+(Row& lhs, const Row& rhs) {
    lhs += rhs;
    return lhs;
}

/**
 * @brief Overload operator += for Row
 *
 * @param rhs
 * @return Row&
 */
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
 * @brief @brief Check if the row is one hot
 *
 * @return true
 * @return false
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

/**
 * @brief Check if the row contains all zeros
 *
 * @return true
 * @return false
 */
bool Row::isZeros() const {
    for (auto& i : _row) {
        if (i == 1) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Clear matrix and operations
 *
 */
void M2::reset() {
    _matrix.clear();
    _opStorage.clear();
}

/**
 * @brief Print matrix
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
 * @brief Print track of operations
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
 * @brief Initialize a test matrix
 *
 */
void M2::defaultInit() {
    // _matrix.push_back(Row(0, vector<unsigned char> {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0}));
    // _matrix.push_back(Row(1, vector<unsigned char> {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}));
    // _matrix.push_back(Row(2, vector<unsigned char> {0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0}));
    // _matrix.push_back(Row(3, vector<unsigned char> {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
    // _matrix.push_back(Row(4, vector<unsigned char> {0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0}));
    // _matrix.push_back(Row(5, vector<unsigned char> {0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}));
    // _matrix.push_back(Row(6, vector<unsigned char> {0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0}));
    // _matrix.push_back(Row(7, vector<unsigned char> {0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1}));
    // _matrix.push_back(Row(8, vector<unsigned char> {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
    // _matrix.push_back(Row(9, vector<unsigned char> {0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}));
    // _matrix.push_back(Row(10, vector<unsigned char>{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0}));
    // _matrix.push_back(Row(11, vector<unsigned char>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}));
    // _matrix.push_back(Row(12, vector<unsigned char>{1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0}));
    // _matrix.push_back(Row(13, vector<unsigned char>{0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}));

    _matrix.push_back(Row(0, vector<unsigned char>{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}));
    _matrix.push_back(Row(1, vector<unsigned char>{0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}));
    _matrix.push_back(Row(2, vector<unsigned char>{0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}));
    _matrix.push_back(Row(3, vector<unsigned char>{0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}));
    _matrix.push_back(Row(4, vector<unsigned char>{0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}));
    _matrix.push_back(Row(5, vector<unsigned char>{0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
    _matrix.push_back(Row(6, vector<unsigned char>{0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}));
    _matrix.push_back(Row(7, vector<unsigned char>{0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}));
    _matrix.push_back(Row(8, vector<unsigned char>{0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}));
    _matrix.push_back(Row(9, vector<unsigned char>{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}));
    _matrix.push_back(Row(10, vector<unsigned char>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}));
    _matrix.push_back(Row(11, vector<unsigned char>{1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0}));
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
 * @brief Perform Gaussian Elimination with different block sizes. Skip the column if it is dupicated.
 *
 * @param blockSize
 * @param fullReduced if true, performing back-substitution from the echelon form
 * @param track if true, record the process to operation track
 * @return size_t (rank)
 */
size_t M2::gaussianElimSkip(size_t blockSize, bool fullReduced, bool track) {
    vector<size_t> pivot_cols, pivot_cols_backup;
    size_t pivot_row = 0;

    for (size_t section = 0; section < ceil(numCols() / (double)blockSize); section++) {
        size_t start = section * blockSize;
        size_t end = min(numCols(), (section + 1) * blockSize);

        unordered_map<vector<unsigned char>, size_t> duplicated;
        for (size_t i = pivot_row; i < numRows(); i++) {
            vector<unsigned char>::const_iterator first = _matrix[i].getRow().begin() + start;
            vector<unsigned char>::const_iterator last = _matrix[i].getRow().begin() + end;
            // NOTE - not all vector, only consider Row[first:last]
            vector<unsigned char> subVec(first, last);
            bool zeros = true;
            for (auto& i : subVec) {
                if (i == 1) {
                    zeros = false;
                    break;
                }
            }
            if (zeros) continue;
            if (duplicated.contains(subVec)) {
                xorOper(duplicated[subVec], i, track);
            } else {
                duplicated[subVec] = i;
            }
        }

        size_t p = start;

        while (p < end) {
            for (size_t r0 = pivot_row; r0 < numRows(); r0++) {
                if (_matrix[r0].getRow()[p] != 0) {
                    if (r0 != pivot_row) {
                        xorOper(r0, pivot_row, track);
                    }

                    for (size_t r1 = pivot_row + 1; r1 < numRows(); r1++) {
                        if (pivot_row != r1 && _matrix[r1].getRow()[p] != 0) {
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
    }
    size_t rank = pivot_row;
    // NOTE - echelon form already

    if (fullReduced) {
        pivot_row--;
        pivot_cols_backup = pivot_cols;
        for (int section = ceil(numCols() / (double)blockSize) - 1; section >= 0; section--) {
            size_t start = section * blockSize;
            size_t end = min(numCols(), (section + 1) * blockSize);

            unordered_map<vector<unsigned char>, size_t> duplicated;
            for (int i = pivot_row; i >= 0; i--) {
                vector<unsigned char>::const_iterator first = _matrix[i].getRow().begin() + start;
                vector<unsigned char>::const_iterator last = _matrix[i].getRow().begin() + end;
                // NOTE - not all vector, only consider Row[first:last]
                vector<unsigned char> subVec(first, last);
                bool zeros = true;
                for (auto& i : subVec) {
                    if (i == 1) {
                        zeros = false;
                        break;
                    }
                }
                if (zeros) continue;
                if (duplicated.contains(subVec)) {
                    xorOper(duplicated[subVec], i, track);
                } else {
                    duplicated[subVec] = i;
                }
            }

            while (pivot_cols_backup.size() > 0 && start <= pivot_cols_backup.back() && pivot_cols_backup.back() < end) {
                size_t pcol = pivot_cols_backup[pivot_cols_backup.size() - 1];
                pivot_cols_backup.pop_back();
                for (size_t r = 0; r < pivot_row; r++) {
                    if (_matrix[r].getRow()[pcol] != 0) {
                        xorOper(pivot_row, r, track);
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
size_t M2::filterDuplicatedOps() {
    vector<Oper> opsCopy = _opStorage;
    vector<size_t> dups;
    unordered_map<size_t, pair<size_t, size_t>> lastUsed;  // NOTE - bit, (another bit, gateId)
    for (size_t i = 0; i < opsCopy.size(); i++) {
        bool firstMatch = false, secondMatch = false;
        if (lastUsed.contains(opsCopy[i].first) && lastUsed[opsCopy[i].first].first == opsCopy[i].second && opsCopy[lastUsed[opsCopy[i].first].second].first == opsCopy[i].first) firstMatch = true;
        if (lastUsed.contains(opsCopy[i].second) && lastUsed[opsCopy[i].second].first == opsCopy[i].first && opsCopy[lastUsed[opsCopy[i].second].second].second == opsCopy[i].second) secondMatch = true;
        if (firstMatch && secondMatch) {
            dups.push_back(i);
            dups.push_back(lastUsed[opsCopy[i].second].second);
            lastUsed.erase(opsCopy[i].first);
            lastUsed.erase(opsCopy[i].second);

        } else {
            lastUsed[opsCopy[i].first] = make_pair(opsCopy[i].second, i);
            lastUsed[opsCopy[i].second] = make_pair(opsCopy[i].first, i);
        }
    }
    sort(dups.begin(), dups.end());

    for (size_t i = 0; i < dups.size(); i++) {
        _opStorage.erase(_opStorage.begin() + (dups[i] - i));
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
 * @return true
 * @return false
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

/**
 * @brief Perform Gaussian Elimination with augmentation column(s)
 *
 * @param track if true, record the process to operation track
 * @return true
 * @return false
 */
bool M2::gaussianElimAugmented(bool track) {
    if (verbose >= 5) cout << "Performing Gaussian Elimination..." << endl;
    if (verbose >= 9) printMatrix();
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
            if (verbose >= 9) {
                cout << "Add " << theFirstRowWithOne << " to " << curRow << endl;
                printMatrix();
            }
        }

        // make other elements on the same column 0
        for (size_t r = 0; r < numRows(); ++r) {
            if (r != curRow && _matrix[r][curCol] == 1) {
                xorOper(curRow, r, track);
                if (verbose >= 9) {
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
            if (neighbors.contains(vt)) storage[table[vt]] = 1;
        }
        _matrix.push_back(Row(1, storage));
    }

    return true;
}

/**
 * @brief Append one hot
 *
 * @param idx the id to be one
 */
void M2::appendOneHot(size_t idx) {
    assert(idx < _matrix.size());
    for (size_t i = 0; i < _matrix.size(); ++i) {
        _matrix[i].push_back((i == idx) ? 1 : 0);
    }
}

/**
 * @brief Get depth of operations
 *
 * @return size_t
 */
size_t M2::opDepth() {
    vector<size_t> rowDepth;
    rowDepth.resize(numRows(), 0);
    if (_opStorage.size() == 0) {
        cout << "Warning: no row operation" << endl;
        return 0;
    }
    for (const auto& [a, b] : _opStorage) {
        size_t maxDepth = max(rowDepth[a], rowDepth[b]);
        rowDepth[a] = maxDepth + 1;
        rowDepth[b] = maxDepth + 1;
    }
    return *max_element(rowDepth.begin(), rowDepth.end());
}

/**
 * @brief Get dense of operations
 *
 * @return float
 */
float M2::denseRatio() {
    size_t depth = opDepth();
    if (depth == 0)
        return 0;
    float ratio = float(depth) / float(_opStorage.size());
    return round(ratio * 100) / 100;
}

/**
 * @brief Push a new column at the end of the matrix
 *
 */
void M2::pushColumn() {
    for_each(_matrix.begin(), _matrix.end(), [](Row& r) { r.push_back(0); });
}
