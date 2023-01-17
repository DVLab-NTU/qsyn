/****************************************************************************
  FileName     [ gFlow.cpp ]
  PackageName  [ gflow ]
  Synopsis     [ Define class GFlow member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "gFlow.h"

#include <cassert>  // for assert
#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>

#include "textFormat.h"  // for TextFormat
class ZXVertex;

namespace TF = TextFormat;

using namespace std;
extern size_t verbose;
/**
 * @brief Reset the gflow calculator
 *
 */
void GFlow::reset() {
    _levels.clear();
    _correctionSets.clear();
    clearTemporaryStorage();
}

/**
 * @brief Calculate the GFlow to the ZXGraph
 *
 */
bool GFlow::calculate(bool disjointNeighbors) {
    // REVIEW - exclude boundary nodes
    reset();

    calculateZerothLayer();

    while (!_levels.back().empty()) {
        _levels.push_back(ZXVertexList());
        updateNeighborsByFrontier();

        _coefficientMatrix.fromZXVertices(_neighbors, _frontier);

        size_t i = 0;
        if (verbose >= 8) printFrontier();
        if (verbose >= 8) printNeighbors();

        for (auto& v : _neighbors) {
            if (disjointNeighbors &&
                any_of(v->getNeighbors().begin(), v->getNeighbors().end(), [this](const NeighborPair& nbpair) {
                    return this->_levels.back().contains(nbpair.first);
                })) {
                if (verbose >= 8) {
                    cout << "Skipping vertex " << v->getId() << ": connected to current level" << endl;
                }
                continue;
            }

            M2 augmentedMatrix = _coefficientMatrix;
            augmentedMatrix.appendOneHot(i);

            if (verbose >= 8) {
                cout << "Before solving: " << endl;
                augmentedMatrix.printMatrix();
            }

            if (augmentedMatrix.gaussianElimAugmented(false)) {
                if (verbose >= 8) {
                    cout << "Solved, adding " << v->getId() << " to this level" << endl;
                }
                _taken.insert(v);
                _levels.back().insert(v);
                setCorrectionSetFromMatrix(v, augmentedMatrix);
            } else if (verbose >= 8) {
                cout << "No solution for " << v->getId() << "." << endl;
            }

            if (verbose >= 8) {
                cout << "After solving: " << endl;
                augmentedMatrix.printMatrix();
            }
            ++i;
        }
        updateFrontier();
    }

    _valid = (_taken.size() == _zxgraph->getNumVertices());
    _levels.pop_back();  // the back is always empty

    vector<pair<size_t, ZXVertex*>> inputsToMove;
    for (size_t i = 0; i < _levels.size() - 1; ++i) {
        for (auto& v : _levels[i]) {
            if (_zxgraph->getInputs().contains(v)) {
                inputsToMove.emplace_back(i, v);
            }
        }
    }

    for (auto& [level, v] : inputsToMove) {
        _levels[level].erase(v);
        _levels.back().insert(v);
    }
    return _valid;
}

/**
 * @brief Clean frontier, neighbors, taken, and coefficeint matrix
 *
 */
void GFlow::clearTemporaryStorage() {
    _frontier.clear();
    _neighbors.clear();
    _taken.clear();
    _coefficientMatrix.clear();
}

/**
 * @brief Calculate 0th layer
 *
 */
void GFlow::calculateZerothLayer() {
    // initialize the 0th layer to be output
    _frontier = _zxgraph->getOutputs();

    _levels.push_back(_zxgraph->getOutputs());

    for (auto& v : _zxgraph->getOutputs()) {
        assert(!_correctionSets.contains(v));
        _correctionSets[v] = ZXVertexList();
        _taken.insert(v);
    }
}

/**
 * @brief Update neighbors by frontier
 *
 */
void GFlow::updateNeighborsByFrontier() {
    _neighbors.clear();

    for (auto& v : _frontier) {
        for (auto& [nb, _] : v->getNeighbors()) {
            if (_taken.contains(nb))
                continue;

            _neighbors.insert(nb);
        }
    }
}

/**
 * @brief Set the correction set to v by the matrix
 *
 * @param v correction set of whom
 * @param matrix
 */
void GFlow::setCorrectionSetFromMatrix(ZXVertex* v, const M2& matrix) {
    _correctionSets[v] = ZXVertexList();

    for (size_t r = 0; r < matrix.numRows(); ++r) {
        if (matrix[r].back() == 0) continue;
        size_t c = 0;
        for (auto& f : _frontier) {
            if (matrix[r][c] == 1) {
                _correctionSets[v].insert(f);
                break;
            }
            c++;
        }
    }
}

/**
 * @brief Update frontier
 *
 */
void GFlow::updateFrontier() {
    // remove vertex that are not frontiers anymore
    vector<ZXVertex*> toRemove;
    for (auto& v : _frontier) {
        bool removing = true;
        for (auto& [nb, _] : v->getNeighbors()) {
            if (!_taken.contains(nb)) {
                removing = false;
                break;
            }
        }
        if (removing) toRemove.push_back(v);
    }

    for (auto& v : toRemove) {
        _frontier.erase(v);
    }

    // add the last layer to the frontier
    for (auto& v : _levels.back()) {
        if (!_zxgraph->getInputs().contains(v)) {
            _frontier.insert(v);
        }
    }
}

/**
 * @brief Print gflow
 *
 */
void GFlow::print() const {
    cout << "GFlow of the graph: \n";
    for (size_t i = 0; i < _levels.size(); ++i) {
        cout << "Level " << i << endl;
        for (const auto& v : _levels[i]) {
            printCorrectionSet(v);
        }
    }
}

/**
 * @brief Print gflow according to levels
 *
 */
void GFlow::printLevels() const {
    cout << "GFlow levels of the graph: \n";
    for (size_t i = 0; i < _levels.size(); ++i) {
        cout << "Level " << right << setw(4) << i << ":";
        for (auto& v : _levels[i]) {
            cout << " " << v->getId();
        }
        cout << endl;
    }
}

/**
 * @brief Print correction set of v
 *
 * @param v correction set of whom
 */
void GFlow::printCorrectionSet(ZXVertex* v) const {
    cout << right << setw(4) << v->getId() << ":";
    if (_correctionSets.contains(v)) {
        if (_correctionSets.at(v).empty()) {
            cout << " (None)";
        } else {
            for (const auto& w : _correctionSets.at(v)) {
                cout << " " << w->getId();
            }
        }
    } else {
        cout << " Does not exist";
    }
    cout << endl;
}

/**
 * @brief Print correction sets
 *
 */
void GFlow::printCorrectionSets() const {
    for (auto& v : _zxgraph->getVertices()) {
        printCorrectionSet(v);
    }
}

/**
 * @brief Print if gflow exists. If not, print which level it breaks
 *
 */
void GFlow::printSummary() const {
    if (_valid) {
        cout << TF::BOLD(TF::GREEN("GFlow exists.\n"))
             << "#Levels: " << _levels.size() << endl;
    } else {
        cout << TF::BOLD(TF::RED("No GFlow exists.\n"))
             << "The flow breaks at level " << _levels.size() << "." << endl;
    }
}

/**
 * @brief Print frontier
 *
 */
void GFlow::printFrontier() const {
    cout << "Frontier :";
    for (auto& v : _frontier) {
        cout << " " << v->getId();
    }
    cout << endl;
}

/**
 * @brief Print neighbors
 *
 */
void GFlow::printNeighbors() const {
    cout << "Neighbors:";
    for (auto& v : _neighbors) {
        cout << " " << v->getId();
    }
    cout << endl;
}

/**
 * @brief Print the vertices with no correction sets
 *
 */
void GFlow::printFailedVertices() const {
    cout << "No correction sets found for the following vertices:" << endl;
    for (auto& v : _neighbors) {
        cout << v->getId() << " ";
    }
    cout << endl;
}