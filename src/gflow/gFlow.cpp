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
 * @brief calculate the GFlow to the ZXGraph
 *
 */
void GFlow::calculate() {
    _levels.clear();

    std::vector<ZXVertex*> vertexList;
    unordered_set<ZXVertex*> taken;

    auto visitOneVertex = [&vertexList, &taken](ZXVertex* v) {
        vertexList.push_back(v);
        taken.insert(v);
    };

    for (auto& v : _zxgraph->getOutputs()) visitOneVertex(v);

    while (vertexList.size()) {
        _levels.push_back(vertexList);
        vertexList.clear();

        for (auto& v : _levels.back()) {
            for (auto& [nb, _] : v->getNeighbors()) {
                if (taken.contains(nb)) continue;
                visitOneVertex(nb);
            }
        }
    }
}

void GFlow::print() const {
    cout << "GFlow of the current graph: \n";
    for (size_t i = 0; i < _levels.size(); ++i) {
        cout << "Level " << i << endl;
        for (auto& v : _levels[i]) {
            cout << "  - " << v->getId() << ": ";
            // TODO - Report correction set too
            cout << endl;
        }
    }
}

void GFlow::printLevels() const {
    cout << "GFlow levels of the current graph: \n";
    for (size_t i = 0; i < _levels.size(); ++i) {
        cout << "Level " << right << setw(4) << i << ":";
        for (auto& v : _levels[i]) {
            cout << " " << v->getId(); 
        }
        cout << endl;
    }
}