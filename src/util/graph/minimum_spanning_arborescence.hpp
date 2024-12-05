#pragma once

#include <fmt/format.h>

#include <algorithm>
#include <unordered_set>

#include "util/graph/digraph.hpp"
#include "util/util.hpp"

namespace dvlab {

namespace detail {

// Find a cycle in the graph if there is one.
// Assumes the graph has at most n - 1 edges.
template <typename VertexAttr, typename CostType>
std::optional<std::vector<typename Digraph<VertexAttr, CostType>::Vertex>>
find_cycle(Digraph<VertexAttr, CostType> const& g) {
    using VertexT = typename Digraph<VertexAttr, CostType>::Vertex;
    for (auto const& v : g.vertices()) {
        auto visited = std::unordered_set<VertexT>{};
        auto stack   = std::vector<VertexT>{v};
        while (!stack.empty()) {
            auto w = stack.back();
            stack.pop_back();
            if (visited.contains(w)) {
                auto cycle = std::vector<VertexT>{};
                while (cycle.empty() || cycle.front() != w) {
                    cycle.push_back(w);
                    w = *g.in_neighbors(w).begin();
                }
                return cycle;
            }
            visited.insert(w);
            for (auto const& u : g.out_neighbors(w)) {
                stack.push_back(u);
            }
        }
    }
    return std::nullopt;
}

template <typename VertexAttr, typename CostType>
Digraph<VertexAttr, CostType>
build_min_edge_subgraph(
    Digraph<VertexAttr, CostType> const& g,
    typename Digraph<VertexAttr, CostType>::Vertex const& root) {
    using DigraphT = Digraph<VertexAttr, CostType>;
    using EdgeT    = typename DigraphT::Edge;

    std::vector<EdgeT> edges;

    for (auto const& v : g.vertices()) {
        if (v == root) continue;
        auto const w = *std::ranges::min_element(
            g.in_neighbors(v),
            std::less{},
            [&](auto const& w) { return g[{w, v}]; });
        edges.emplace_back(w, v);
    }

    auto mst = DigraphT{};
    for (auto const& v : g.vertices()) {
        mst.add_vertex_with_id(v);
    }
    for (auto const& e : edges) {
        mst.add_edge(e, g[e]);
    }
    return mst;
}

}  // namespace detail

template <typename VertexAttr, typename CostType>
requires std::signed_integral<CostType> || std::floating_point<CostType>
Digraph<VertexAttr, CostType>
minimum_spanning_arborescence(
    Digraph<VertexAttr, CostType> const& g,
    typename Digraph<VertexAttr, CostType>::Vertex const& root) {
    using DigraphT = Digraph<VertexAttr, CostType>;
    using VertexT  = typename DigraphT::Vertex;

    auto min_edges = detail::build_min_edge_subgraph(g, root);

    auto const cycle = detail::find_cycle(min_edges);
    if (!cycle.has_value()) {
        // already a mst
        return min_edges;
    }

    // build a graph with the cycle replaced by a single vertex
    auto g_prime = g;

    for (auto const& v : *cycle) {
        g_prime.remove_vertex(v);
    }

    auto const v_cycle = g_prime.add_vertex();

    auto const is_in_cycle = [&](auto const& v) {
        return !g_prime.has_vertex(v);
    };

    std::unordered_map<VertexT, VertexT>
        v_cycle_in_idx;  // records the original successors for edge
                         // sources that point into the cycle
    std::unordered_map<VertexT, VertexT>
        v_cycle_out_idx;  // records the original predecessors for edge
                          // destinations that point out of the cycle

    for (auto const& u : g.vertices()) {
        for (auto const& v : g.out_neighbors(u)) {
            // case 1: (u, v) points into the cycle
            if (!is_in_cycle(u) && is_in_cycle(v)) {
                auto const pred_in_cycle = *min_edges.in_neighbors(v).begin();
                auto const new_weight    = g[{u, v}] + min_edges[{pred_in_cycle, v}];
                if (!g_prime.has_edge(u, v_cycle)) {
                    g_prime.add_edge(u, v_cycle, new_weight);
                    v_cycle_in_idx[u] = v;
                } else {
                    if (new_weight < g_prime[{u, v_cycle}]) {
                        g_prime[{u, v_cycle}] = new_weight;
                        v_cycle_in_idx[u]     = v;
                    }
                }
            }
            // case 2: (u, v) points out of the cycle
            else if (is_in_cycle(u) && !is_in_cycle(v)) {
                auto const weight = g[{u, v}];
                if (!g_prime.has_edge(v_cycle, v)) {
                    g_prime.add_edge(v_cycle, v, weight);
                    v_cycle_out_idx[v] = u;
                } else {
                    if (weight < g_prime[{v_cycle, v}]) {
                        g_prime[{v_cycle, v}] = weight;
                        v_cycle_out_idx[v]    = u;
                    }
                }
            }
            // Case 3: If (u, v) is a part of the loop, it is removed in
            // g_prime, so we do nothing. If neither u nor v is in the cycle,
            // the edge is unaffected.
        }
    }

    // find mst of g_prime
    auto mst = minimum_spanning_arborescence(g_prime, root);

    for (auto const& v : *cycle) {
        mst.add_vertex_with_id(v);
    }

    // restore the cycle edges
    for (auto const& node_in : mst.out_neighbors(v_cycle)) {
        auto const orig_out = v_cycle_out_idx.at(node_in);
        assert(g.has_vertex(orig_out));
        mst.add_edge(orig_out, node_in, g[{orig_out, node_in}]);
    }

    DVLAB_ASSERT(mst.in_degree(v_cycle) == 1,
                 "In-degree to the cycle vertex must be 1");

    auto const src = *mst.in_neighbors(v_cycle).begin();

    auto const orig_in = v_cycle_in_idx.at(src);
    mst.add_edge(src, orig_in, g[{src, orig_in}]);
    auto n = mst.remove_vertex(v_cycle);
    DVLAB_ASSERT(n == 1, "Must remove exactly one vertex");

    for (auto const& v : *cycle) {
        if (v == orig_in) continue;
        auto const src = *min_edges.in_neighbors(v).begin();
        mst.add_edge(src, v, min_edges[{src, v}]);
    }

    DVLAB_ASSERT(mst.num_edges() == g.num_vertices() - 1,
                 "MST must have n - 1 edges");

    return mst;
}

template <typename VertexAttr, typename CostType>
requires std::signed_integral<CostType> || std::floating_point<CostType>
std::pair<Digraph<VertexAttr, CostType>,
          typename Digraph<VertexAttr, CostType>::Vertex>
minimum_spanning_arborescence(
    Digraph<VertexAttr, CostType> const& g) {
    using DigraphT          = Digraph<VertexAttr, CostType>;
    using VertexT           = typename DigraphT::Vertex;
    auto const total_weight = [&](DigraphT const& g) {
        auto sum = CostType{0};
        // circumvents compilation error in clang for g.edges()
        for (auto const& v : g.vertices()) {
            for (auto const& e : g.out_edges(v)) {
                sum += g[e];
            }
        }
        return sum;
    };

    auto cost = std::numeric_limits<CostType>::max();
    auto mst  = DigraphT{};
    auto root = VertexT{};
    for (auto const& v : g.vertices()) {
        auto const mst_prime = minimum_spanning_arborescence(g, v);
        auto const new_cost  = total_weight(mst_prime);
        if (new_cost < cost) {
            cost = new_cost;
            mst  = mst_prime;
            root = v;
        }
    }

    return {mst, root};
}

}  // namespace dvlab
