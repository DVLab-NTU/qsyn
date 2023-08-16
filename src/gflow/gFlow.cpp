/****************************************************************************
  FileName     [ gFlow.cpp ]
  PackageName  [ gflow ]
  Synopsis     [ Define class GFlow member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./gFlow.hpp"

#include <cassert>
#include <cstddef>
#include <ranges>

#include "simplifier/simplify.hpp"
#include "util/textFormat.hpp"

using namespace std;
extern size_t verbose;

constexpr auto vertex2id = [](ZXVertex* v) { return v->getId(); };

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

        _levels.emplace_back();

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
                    fmt::println("Skipping vertex {} : connected to current level", v->getId());
                }
                continue;
            }

            M2 augmentedMatrix = prepareMatrix(v, i);

            if (verbose >= 8) {
                fmt::println("Before solving:");
                augmentedMatrix.printMatrix();
            }

            if (augmentedMatrix.gaussianElimAugmented(false)) {
                if (verbose >= 8) {
                    fmt::println("Solved, adding {} to this level", v->getId());
                }
                _taken.insert(v);
                _levels.back().insert(v);
                setCorrectionSetFromMatrix(v, augmentedMatrix);
            } else if (verbose >= 8) {
                fmt::println("No solution for {}.", v->getId());
            }

            if (verbose >= 8) {
                fmt::println("After solving:");
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

    _levels.emplace_back(_zxgraph->getOutputs());

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
            toRemove.emplace_back(v);
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
    fmt::println("GFlow of the graph:");
    for (size_t i = 0; i < _levels.size(); ++i) {
        fmt::println("Level {}", i);
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
    fmt::println("GFlow levels of the graph:");
    for (size_t i = 0; i < _levels.size(); ++i) {
        fmt::println("Level {:>4}: {}", i, fmt::join(_levels[i] | views::transform(vertex2id), " "));
    }
}

/**
 * @brief Print correction set of v
 *
 * @param v correction set of whom
 */
void GFlow::printXCorrectionSet(ZXVertex* v) const {
    fmt::print("{:>4} ({}): ", v->getId(), _measurementPlanes.at(v));
    if (_xCorrectionSets.contains(v)) {
        if (_xCorrectionSets.at(v).empty()) {
            fmt::println("(None)");
        } else {
            fmt::println("{}", fmt::join(_xCorrectionSets.at(v) | views::transform(vertex2id), " "));
        }
    } else {
        fmt::println("Does not exist");
    }
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
    using namespace dvlab_utils;
    if (_valid) {
        fmt::println("{}", fmt_ext::styled_if_ANSI_supported("GFlow exists.", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
        fmt::println("#Levels: {}", _levels.size());
    } else {
        fmt::println("{}", fmt_ext::styled_if_ANSI_supported("No GFlow exists.", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
        fmt::println("The flow breaks at level {}.", _levels.size());
    }
}

/**
 * @brief Print frontier
 *
 */
void GFlow::printFrontier() const {
    fmt::println("Frontier: {}", fmt::join(_frontier | views::transform(vertex2id), " "));
}

/**
 * @brief Print neighbors
 *
 */
void GFlow::printNeighbors() const {
    fmt::println("Neighbors: {}", fmt::join(_neighbors | views::transform(vertex2id), " "));
}

/**
 * @brief Print the vertices with no correction sets
 *
 */
void GFlow::printFailedVertices() const {
    fmt::println("No correction sets found for the following vertices:");
    fmt::println("{}", fmt::join(_neighbors | views::transform(vertex2id), " "));
}

std::ostream& operator<<(std::ostream& os, GFlow::MeasurementPlane const& plane) {
    return os << fmt::format("{}", plane);
}