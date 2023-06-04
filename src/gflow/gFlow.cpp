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
#include <ranges>

#include "simplify.h"
#include "textFormat.h"  // for TextFormat
class ZXVertex;

namespace TF = TextFormat;

using namespace std;
extern size_t verbose;

/**
 * @brief Calculate the Z correction set of a vertex,
 *        i.e., Odd(g(v))
 *
 * @param v
 * @return ZXVertexList
 */
ZXVertexList GFlow::getZCorrectionSet(ZXVertex* v) const {
    ZXVertexList out;

    ordered_hashmap<ZXVertex*, size_t> numOccurences;

    for (auto const& gv : getXCorrectionSet(v)) {
        // FIXME - should count neighbor!
        for (auto const& [nb, et] : gv->getNeighbors()) {
            if (numOccurences.contains(nb)) {
                numOccurences[nb]++;
            } else {
                numOccurences.emplace(nb, 1);
            }
        }
    }

    for (auto const& [odd_gv, n] : numOccurences) {
        if (n % 2 == 1) out.emplace(odd_gv);
    }

    return out;
}

/**
 * @brief Initialize the gflow calculator
 *
 */
void GFlow::initialize() {
    _levels.clear();
    _xCorrectionSets.clear();
    _measurementPlanes.clear();
    _frontier.clear();
    _neighbors.clear();
    _taken.clear();
    _coefficientMatrix.clear();
    _vertex2levels.clear();
    using MP = MeasurementPlane;

    // Measurement planes - See Table 1, p.10 of the paper
    // M. Backens, H. Miller-Bakewell, G. de Felice, L. Lobski, & J. van de Wetering (2021). There and back again: A circuit extraction tale. Quantum, 5, 421.
    // https://quantum-journal.org/papers/q-2021-03-25-421/
    for (auto const& v : _zxgraph->getVertices()) {
        _measurementPlanes.emplace(v, MP::XY);
    }
    // if calculating extended gflow, modify some of the measurment plane
    if (_doExtended) {
        for (auto const& v : _zxgraph->getVertices()) {
            if (_zxgraph->isGadgetLeaf(v)) {
                _measurementPlanes[v] = MP::NOT_A_QUBIT;
                _taken.insert(v);
            } else if (_zxgraph->isGadgetAxel(v))
                _measurementPlanes[v] = v->hasNPiPhase() ? MP::YZ
                                        : v->getPhase().denominator() == 2
                                            ? MP::XZ
                                            : MP::ERROR;
            assert(_measurementPlanes[v] != MP::ERROR);
        }
    }
}

/**
 * @brief Calculate the GFlow to the ZXGraph
 *
 */
bool GFlow::calculate() {
    // REVIEW - exclude boundary nodes
    initialize();

    calculateZerothLayer();

    while (!_levels.back().empty()) {
        updateNeighborsByFrontier();

        _levels.push_back(ZXVertexList());

        _coefficientMatrix.fromZXVertices(_neighbors, _frontier);

        size_t i = 0;
        if (verbose >= 8) printFrontier();
        if (verbose >= 8) printNeighbors();

        for (auto& v : _neighbors) {
            if (_doIndependentLayers &&
                any_of(v->getNeighbors().begin(), v->getNeighbors().end(), [this](const NeighborPair& nbpair) {
                    return this->_levels.back().contains(nbpair.first);
                })) {
                if (verbose >= 8) {
                    cout << "Skipping vertex " << v->getId() << ": connected to current level" << endl;
                }
                continue;
            }

            M2 augmentedMatrix = prepareMatrix(v, i);

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

        for (auto& v : _levels.back()) {
            _vertex2levels.emplace(v, _levels.size() - 1);
        }
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
 * @brief Calculate 0th layer
 *
 */
void GFlow::calculateZerothLayer() {
    // initialize the 0th layer to be output
    _frontier = _zxgraph->getOutputs();

    _levels.push_back(_zxgraph->getOutputs());

    for (auto& v : _zxgraph->getOutputs()) {
        assert(!_xCorrectionSets.contains(v));
        _vertex2levels.emplace(v, 0);
        _xCorrectionSets[v] = ZXVertexList();
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
            if (_measurementPlanes[nb] == MeasurementPlane::NOT_A_QUBIT) {
                _taken.insert(nb);
                continue;
            }

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
    assert(!_xCorrectionSets.contains(v));
    _xCorrectionSets[v] = ZXVertexList();

    for (size_t r = 0; r < matrix.numRows(); ++r) {
        if (matrix[r].back() == 0) continue;
        size_t c = 0;
        for (auto& f : _frontier) {
            if (matrix[r][c] == 1) {
                _xCorrectionSets[v].insert(f);
                break;
            }
            c++;
        }
    }
    if (isXError(v)) _xCorrectionSets[v].insert(v);

    assert(_xCorrectionSets[v].size());
}

/**
 * @brief prepare the matrix to solve depending on the measurement plane.
 *
 */
M2 GFlow::prepareMatrix(ZXVertex* v, size_t i) {
    // cout << "preparing matrix: v = " << v->getId() << ", i = " << i << endl;
    // printFrontier();
    // printNeighbors();

    M2 augmentedMatrix = _coefficientMatrix;
    augmentedMatrix.pushColumn();

    auto itr = _neighbors.begin();
    for (size_t j = 0; j < augmentedMatrix.numRows(); ++j) {
        if (isZError(v)) {
            augmentedMatrix[j][augmentedMatrix.numCols() - 1] += (i == j) ? 1 : 0;
        }
        if (isXError(v)) {
            if ((*itr)->isNeighbor(v)) {
                augmentedMatrix[j][augmentedMatrix.numCols() - 1] += 1;
            }
        }
        ++itr;
    }

    for (size_t j = 0; j < augmentedMatrix.numRows(); ++j) {
        augmentedMatrix[j][augmentedMatrix.numCols() - 1] %= 2;
    }

    return augmentedMatrix;
}

/**
 * @brief Update frontier
 *
 */
void GFlow::updateFrontier() {
    // remove vertex that are not frontiers anymore
    vector<ZXVertex*> toRemove;
    for (auto& v : _frontier) {
        if (all_of(v->getNeighbors().begin(), v->getNeighbors().end(),
                   [this](NeighborPair const& nbp) {
                       return _taken.contains(nbp.first);
                   })) {
            toRemove.push_back(v);
        }
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
            printXCorrectionSet(v);
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
void GFlow::printXCorrectionSet(ZXVertex* v) const {
    cout << right << setw(4) << v->getId() << " (" << _measurementPlanes.at(v) << "):";
    if (_xCorrectionSets.contains(v)) {
        if (_xCorrectionSets.at(v).empty()) {
            cout << " (None)";
        } else {
            for (const auto& w : _xCorrectionSets.at(v)) {
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
void GFlow::printXCorrectionSets() const {
    for (auto& v : _zxgraph->getVertices()) {
        printXCorrectionSet(v);
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

std::ostream& operator<<(std::ostream& os, GFlow::MeasurementPlane const& plane) {
    using MP = GFlow::MeasurementPlane;
    switch (plane) {
        case MP::XY:
            return os << "XY";
        case MP::YZ:
            return os << "YZ";
        case MP::XZ:
            return os << "XZ";
        case MP::NOT_A_QUBIT:
            return os << "not a qubit";
        case MP::ERROR:
        default:
            return os << "ERROR";
    }
}