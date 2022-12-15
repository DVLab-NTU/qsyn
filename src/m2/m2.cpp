/****************************************************************************
  FileName     [ m2.cpp ]
  PackageName  [ m2 ]
  Synopsis     [ Define class m2 member functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "m2.h"

extern size_t verbose;
using namespace std;

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

void M2::reset(){
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
 * @brief Perform Gaussian Elimination
 *
 * @param track if true, record the process to operation track
 */
void M2::gaussianElim(bool track) {
    if (verbose >= 5) cout << "Performing Gaussian Elimination..." << endl;
    if (verbose >= 8) printMatrix();
    _opStorage.clear();
    for (size_t i = 0; i < _matrix.size() - 1; i++) {
        // If a number on main diagonal is zero, greedily perform row operations to make them 1
        if (_matrix[i][i] == 0) {
            bool noSol = true;
            for (size_t j = i + 1; j < _matrix.size(); j++) {
                if (_matrix[j][i] == 1) {
                    xorOper(j, i, track);
                    if (verbose >= 8) {
                        cout << "Diag Add " << j << " to " << i << endl;
                        printMatrix();
                    }
                    noSol = false;
                    break;
                }
            }
            if (noSol) {
                cout << "No Solution" << endl;
                return;
            }
        }

        // convert to upper-triangle matrix
        for (size_t j = i + 1; j < _matrix.size(); j++) {
            if (_matrix[j][i] == 1 && _matrix[i][i] == 1) {
                xorOper(i, j, track);
                if (verbose >= 8) {
                    cout << "Add " << i << " to " << j << endl;
                    printMatrix();
                }
            }
        }
    }

    // convert to identity matrix on the leftmost _matrix.size() matrix
    for (size_t i = 0; i < _matrix.size(); i++) {
        for (size_t j = _matrix.size() - i; j < _matrix.size(); j++) {
            if (_matrix[_matrix.size() - i - 1][j] == 1) {
                xorOper(j, _matrix.size() - i - 1, track);
                if (verbose >= 8) {
                    cout << "Add " << j << " to " << _matrix.size() - i - 1 << endl;
                    printMatrix();
                }
            }
        }
    }
}

/// @brief Check m2 is identity
/// @return
bool M2::isIdentity() const {
    // REVIEW - Only check one 1 in a row
    for (const auto& row : _matrix) {
        if (!row.isOneHot()) return false;
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
