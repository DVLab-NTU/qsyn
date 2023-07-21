/****************************************************************************
  FileName     [ zxTraverse.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph traversal functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t
#include <iostream>
#include <list>
#include <stack>

#include "zxGraph.h"  // for ZXGraph, ZXVertex

using namespace std;
extern size_t verbose;

/**
 * @brief Update Topological Order
 *
 */
void ZXGraph::updateTopoOrder() {
    _topoOrder.clear();
    _globalTraCounter++;
    for (const auto& v : _inputs) {
        if (!(v->isVisited(_globalTraCounter)))
            DFS(v);
    }
    for (const auto& v : _outputs) {
        if (!(v->isVisited(_globalTraCounter)))
            DFS(v);
    }
    reverse(_topoOrder.begin(), _topoOrder.end());
    if (verbose >= 7) {
        cout << "Topological order from first input: ";
        for (size_t j = 0; j < _topoOrder.size(); j++) {
            cout << _topoOrder[j]->getId() << " ";
        }
        cout << "\nSize of topological order: " << _topoOrder.size() << endl;
    }
}

/**
 * @brief Performing DFS from currentVertex
 *
 * @param currentVertex
 */
void ZXGraph::DFS(ZXVertex* currentVertex) {
    stack<pair<bool, ZXVertex*>> dfs;

    if (!currentVertex->isVisited(_globalTraCounter)) {
        dfs.push(make_pair(false, currentVertex));
    }
    while (!dfs.empty()) {
        pair<bool, ZXVertex*> node = dfs.top();
        dfs.pop();
        if (node.first) {
            _topoOrder.push_back(node.second);
            continue;
        }
        if (node.second->isVisited(_globalTraCounter)) {
            continue;
        }
        node.second->setVisited(_globalTraCounter);
        dfs.push(make_pair(true, node.second));

        for (const auto& v : node.second->getNeighbors()) {
            if (!(v.first->isVisited(_globalTraCounter))) {
                dfs.push(make_pair(false, v.first));
            }
        }
    }
}

/**
 * @brief Update BFS information
 *
 */
void ZXGraph::updateBreadthLevel() {
    for (const auto& v : _inputs) {
        if (!(v->isVisited(_globalTraCounter)))
            BFS(v);
    }
    for (const auto& v : _outputs) {
        if (!(v->isVisited(_globalTraCounter)))
            BFS(v);
    }
}

/**
 * @brief Performing BFS from currentVertex
 *
 * @param currentVertex
 */
void ZXGraph::BFS(ZXVertex* currentVertex) {
    list<ZXVertex*> queue;

    currentVertex->setVisited(_globalTraCounter);
    queue.push_back(currentVertex);

    while (!queue.empty()) {
        ZXVertex* s = queue.front();

        _topoOrder.push_back(s);
        queue.pop_front();

        for (auto [adjecent, _] : s->getNeighbors()) {
            if (!(adjecent->isVisited(_globalTraCounter))) {
                adjecent->setVisited(_globalTraCounter);
                queue.push_back(adjecent);
            }
        }
    }
}