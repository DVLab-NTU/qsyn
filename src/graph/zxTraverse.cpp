/****************************************************************************
  FileName     [ zxTraverse.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Traversal functions for ZX ]
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
    for(const auto& v: _inputs){
      if(!(v->isVisited(_globalDFScounter))) 
        DFS(v);
    }
    for(const auto& v: _outputs){
      if(!(v->isVisited(_globalDFScounter))) 
        DFS(v);
    }
    reverse(_topoOrder.begin(), _topoOrder.end());
    if (verbose >= 7) {
        cout << "Topological order from first input: ";
        for (size_t j = 0; j < _topoOrder.size(); j++){
            cout << _topoOrder[j]->getId() << " ";
        }
        cout << "\nSize of topological order: " << _topoOrder.size() << endl;
    }
}
void ZXGraph::DFS(ZXVertex *currentVertex)
{
    currentVertex->setVisited(_globalDFScounter);

    Neighbors neighbors = currentVertex->getNeighbors();
    for(const auto& v: neighbors){
       if(!(v.first->isVisited(_globalDFScounter))) DFS(v.first);
    }

    _topoOrder.push_back(currentVertex);
}