/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph traversal functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <list>
#include <stack>

#include "./zxgraph.hpp"
#include "util/logger.hpp"

extern dvlab::Logger LOGGER;

namespace qsyn::zx {

/**
 * @brief Update Topological Order
 *
 */
void ZXGraph::update_topological_order() const {
    _topological_order.clear();
    _global_traversal_counter++;
    for (auto const& v : _inputs) {
        if (!(v->is_visited(_global_traversal_counter)))
            _dfs(v);
    }
    for (auto const& v : _outputs) {
        if (!(v->is_visited(_global_traversal_counter)))
            _dfs(v);
    }
    reverse(_topological_order.begin(), _topological_order.end());
    LOGGER.trace("Topological order from first input: {}", fmt::join(_topological_order | std::views::transform([](auto const& v) { return v->get_id(); }), " "));
    LOGGER.trace("Size of topological order: {}", _topological_order.size());
}

/**
 * @brief Performing DFS from currentVertex
 *
 * @param currentVertex
 */
void ZXGraph::_dfs(ZXVertex* curr_vertex) const {
    std::stack<std::pair<bool, ZXVertex*>> dfs;

    if (!curr_vertex->is_visited(_global_traversal_counter)) {
        dfs.emplace(false, curr_vertex);
    }
    while (!dfs.empty()) {
        std::pair<bool, ZXVertex*> node = dfs.top();
        dfs.pop();
        if (node.first) {
            _topological_order.emplace_back(node.second);
            continue;
        }
        if (node.second->is_visited(_global_traversal_counter)) {
            continue;
        }
        node.second->mark_as_visited(_global_traversal_counter);
        dfs.emplace(true, node.second);

        for (auto const& v : node.second->get_neighbors()) {
            if (!(v.first->is_visited(_global_traversal_counter))) {
                dfs.emplace(false, v.first);
            }
        }
    }
}

/**
 * @brief Update BFS information
 *
 */
void ZXGraph::update_breadth_level() const {
    for (auto const& v : _inputs) {
        if (!(v->is_visited(_global_traversal_counter)))
            _bfs(v);
    }
    for (auto const& v : _outputs) {
        if (!(v->is_visited(_global_traversal_counter)))
            _bfs(v);
    }
}

/**
 * @brief Performing BFS from currentVertex
 *
 * @param currentVertex
 */
void ZXGraph::_bfs(ZXVertex* curr_vertex) const {
    std::list<ZXVertex*> queue;

    curr_vertex->mark_as_visited(_global_traversal_counter);
    queue.emplace_back(curr_vertex);

    while (!queue.empty()) {
        ZXVertex* s = queue.front();

        _topological_order.emplace_back(s);
        queue.pop_front();

        for (auto [adjecent, _] : s->get_neighbors()) {
            if (!(adjecent->is_visited(_global_traversal_counter))) {
                adjecent->mark_as_visited(_global_traversal_counter);
                queue.emplace_back(adjecent);
            }
        }
    }
}

}  // namespace qsyn::zx