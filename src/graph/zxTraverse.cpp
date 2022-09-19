/****************************************************************************
  FileName     [ zxMapping.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Mapping function for ZX ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>
#include <algorithm>
#include "zxGraph.h"
#include "util.h"

using namespace std;
extern size_t verbose;

void ZXGraph::updateTopoOrder()
{
    _topoOrder.clear();
    _globalDFScounter++;
    for(size_t i=0; i<_inputs.size(); i++){
        if(!_inputs[i]->isVisited(_globalDFScounter))
            DFS(_inputs[i]);
    }
    for(size_t i=0; i<_outputs.size(); i++){
        if(!_outputs[i]->isVisited(_globalDFScounter))
            DFS(_outputs[i]);
    }
    reverse(_topoOrder.begin(), _topoOrder.end());
    if (verbose >=7) cout << "Topological order from first input: " << endl;
    for(size_t j=0; j<_topoOrder.size();j++){
        if (verbose >=7) cout << _topoOrder[j]->getId()<<" ";
    }
    if (verbose >=7) cout << endl;
    if (verbose >=7) cout << "Size of topological order: " <<_topoOrder.size() << endl;
    assert(_topoOrder.size() == _vertices.size());
}
void ZXGraph::DFS(ZXVertex *currentVertex)
{
    currentVertex->setVisited(_globalDFScounter);
    vector<NeighborPair> neighbors = currentVertex -> getNeighbors();
    for (size_t i = 0; i < neighbors.size(); i++)
    {
        if (!(neighbors[i].first->isVisited(_globalDFScounter)))
            DFS(neighbors[i].first);
    }
    _topoOrder.push_back(currentVertex);
}