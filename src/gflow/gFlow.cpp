/****************************************************************************
  FileName     [ gFlow.cpp ]
  PackageName  [ gflow ]
  Synopsis     [ Define class GFlow member functions ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "gFlow.h"

using namespace std;

/**
 * @brief reset the gflow calculator
 *
 */
void GFlow::reset() {
    _levels.clear();
    _correctionSets.clear();
    clearTemporaryStorage();
}

/**
 * @brief calculate the GFlow to the ZXGraph
 *
 */
bool GFlow::calculate() {
    reset();

    calculateZerothLayer();

    while (!_levels.back().empty()) {
        _levels.push_back(ZXVertexList());
        updateNeighborsByFrontier();

        _coefficientMatrix.fromZXVertices(_neighbors, _frontier);

        size_t i = 0;
        for (auto& v : _neighbors) {
            M2 augmentedMatrix = _coefficientMatrix;
            augmentedMatrix.appendOneHot(i);
            augmentedMatrix.gaussianElim(false);

            if (augmentedMatrix.isSolvedForm()) {
                _taken.insert(v);
                _levels.back().insert(v);
                setCorrectionSetFromMatrix(v, augmentedMatrix);
            }
            ++i;
        }

        updateFrontier();
    }

    _levels.pop_back(); // the back is always empty
    _dirty = false;

    _valid = (_taken.size() == _zxgraph->getNumVertices());

    clearTemporaryStorage();

    return _valid;
}

void GFlow::clearTemporaryStorage() {
    _frontier.clear();
    _neighbors.clear();
    _taken.clear();
    _coefficientMatrix.clear();
}

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

void GFlow::updateNeighborsByFrontier() {
    _neighbors.clear();

    for (auto& v : _frontier) {
        for (auto& [nb, _] : v->getNeighbors()) {
            if (!_taken.contains(nb)) _neighbors.insert(nb);
        }
    }
}

void GFlow::setCorrectionSetFromMatrix(ZXVertex* v, const M2& matrix) {
    _correctionSets[v] = ZXVertexList();

    size_t j = 0;
    for (auto& f : _frontier) {
        if (matrix[j].back() == 1) {
            _correctionSets[v].insert(f);
        }
        j++;
    }
}

void GFlow::updateFrontier() {
    // remove vertex that are not frontiers anymore 
    vector<ZXVertex*> toRemove;
    for (auto& v : _frontier) {
        bool removing = true;
        for (auto& [nb, _]: v->getNeighbors()) {
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
        _frontier.insert(v);
    }
}

void GFlow::print() const {
    printFlagInfo();
    cout << "GFlow of the graph: \n";
    for (size_t i = 0; i < _levels.size(); ++i) {
        cout << "Level " << i << endl;
        for (const auto& v : _levels[i]) {
            cout << "  - " << v->getId() << ":";
            if (_correctionSets.contains(v)) {
                for (const auto& w : _correctionSets.at(v)) {
                    cout << " " << w->getId();
                }
            }
            cout << endl;
        }
    }
}

void GFlow::printLevels() const {
    printFlagInfo();
    cout << "GFlow levels of the graph: \n";
    for (size_t i = 0; i < _levels.size(); ++i) {
        cout << "Level " << right << setw(4) << i << ":";
        for (auto& v : _levels[i]) {
            cout << " " << v->getId();
        }
        cout << endl;
    }
}

void GFlow::printFlagInfo() const {
    if (_dirty) {
        cout << "Warning: The gflow is either not calculated yet or outdated, please recalculate for accurate info!" << endl;
    }
    if (!_valid) {
        cout << "Warning: This graph does not have a gflow. Belows shows the gflow upto where the flow breaks. " << endl;
    }
}