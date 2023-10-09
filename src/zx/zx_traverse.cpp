/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph traversal functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cstddef>
#include <list>
#include <stack>
#include <unordered_map>

#include "./zxgraph.hpp"

namespace qsyn::zx {

/**
 * @brief Update Topological Order
 *
 */
std::vector<ZXVertex*> ZXGraph::create_topological_order() const {
    std::vector<ZXVertex*> topological_order;
    size_t global_traversal_counter = 0;
    std::unordered_set<ZXVertex*> dfs_counters;
    for (auto const& v : _inputs) {
        if (!dfs_counters.contains(v))
            _dfs(dfs_counters, topological_order, v);
    }
    for (auto const& v : _outputs) {
        if (!dfs_counters.contains(v))
            _dfs(dfs_counters, topological_order, v);
    }
    reverse(topological_order.begin(), topological_order.end());
    spdlog::trace("Topological order from first input: {}", fmt::join(topological_order | std::views::transform([](auto const& v) { return v->get_id(); }), " "));
    spdlog::trace("Size of topological order: {}", topological_order.size());

    return topological_order;
}

/**
 * @brief Performing DFS from currentVertex
 *
 * @param currentVertex
 */
void ZXGraph::_dfs(std::unordered_set<ZXVertex*>& visited_vertices, std::vector<ZXVertex*>& topological_order, ZXVertex* curr_vertex) const {
    std::stack<std::pair<bool, ZXVertex*>> dfs;

    if (!visited_vertices.contains(curr_vertex)) {
        dfs.emplace(false, curr_vertex);
    }
    while (!dfs.empty()) {
        auto [is_visited, vertex] = dfs.top();
        dfs.pop();
        if (is_visited) {
            topological_order.emplace_back(vertex);
            continue;
        }
        if (visited_vertices.contains(vertex)) {
            continue;
        }
        visited_vertices.emplace(vertex);
        dfs.emplace(true, vertex);

        for (auto const& [nb, _] : this->get_neighbors(vertex)) {
            if (!visited_vertices.contains(nb)) {
                dfs.emplace(false, nb);
            }
        }
    }
}

/**
 * @brief Update BFS information
 *
 */
std::vector<ZXVertex*> ZXGraph::create_breadth_level() const {
    std::unordered_set<ZXVertex*> bfs_counters;
    std::vector<ZXVertex*> breadth_order;
    for (auto const& v : _inputs) {
        if (!bfs_counters.contains(v))
            _bfs(bfs_counters, breadth_order, v);
    }
    for (auto const& v : _outputs) {
        if (!bfs_counters.contains(v))
            _bfs(bfs_counters, breadth_order, v);
    }

    return breadth_order;
}

/**
 * @brief Performing BFS from currentVertex
 *
 * @param current_vertex
 */
void ZXGraph::_bfs(std::unordered_set<ZXVertex*>& visited_vertices, std::vector<ZXVertex*>& topological_order, ZXVertex* curr_vertex) const {
    std::list<ZXVertex*> queue;

    visited_vertices.emplace(curr_vertex);
    queue.emplace_back(curr_vertex);

    while (!queue.empty()) {
        ZXVertex* s = queue.front();

        topological_order.emplace_back(s);
        queue.pop_front();

        for (auto [adjecent, _] : this->get_neighbors(s)) {
            if (!visited_vertices.contains(adjecent)) {
                visited_vertices.emplace(adjecent);
                queue.emplace_back(adjecent);
            }
        }
    }
}

}  // namespace qsyn::zx