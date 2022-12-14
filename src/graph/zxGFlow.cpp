/****************************************************************************
  FileName     [ zxGflow.h ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGFlow member functions ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxGFlow.h"

using namespace std;
/**
 * @brief calculate the GFlow to the ZXGraph
 * 
 */
void ZXGFlow::calculate() {
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

