/****************************************************************************
  FileName     [ zxTraverse.cpp ]
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph traversal functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <list>
#include <stack>

#include "./zxGraph.hpp"
#include "util/logger.hpp"

using namespace std;
extern dvlab::utils::Logger logger;

/**
 * @brief Update Topological Order
 *
 */
void ZXGraph::updateTopoOrder() const {
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
    logger.trace("Topological order from first input: {}", fmt::join(_topoOrder | views::transform([](auto const& v) { return v->getId(); }), " "));
    logger.trace("Size of topological order: {}", _topoOrder.size());
}

/**
 * @brief Performing DFS from currentVertex
 *
 * @param currentVertex
 */
void ZXGraph::DFS(ZXVertex* currentVertex) const {
    stack<pair<bool, ZXVertex*>> dfs;

    if (!currentVertex->isVisited(_globalTraCounter)) {
        dfs.push(make_pair(false, currentVertex));
    }
    while (!dfs.empty()) {
        pair<bool, ZXVertex*> node = dfs.top();
        dfs.pop();
        if (node.first) {
            _topoOrder.emplace_back(node.second);
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
void ZXGraph::updateBreadthLevel() const {
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
void ZXGraph::BFS(ZXVertex* currentVertex) const {
    list<ZXVertex*> queue;

    currentVertex->setVisited(_globalTraCounter);
    queue.emplace_back(currentVertex);

    while (!queue.empty()) {
        ZXVertex* s = queue.front();

        _topoOrder.emplace_back(s);
        queue.pop_front();

        for (auto [adjecent, _] : s->getNeighbors()) {
            if (!(adjecent->isVisited(_globalTraCounter))) {
                adjecent->setVisited(_globalTraCounter);
                queue.emplace_back(adjecent);
            }
        }
    }
}