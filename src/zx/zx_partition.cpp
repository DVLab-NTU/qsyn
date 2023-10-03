/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Implements the split function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_partition.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <stack>
#include <tl/enumerate.hpp>
#include <unordered_map>
#include <utility>

#include "./zx_def.hpp"
#include "./zxgraph.hpp"

bool stop_requested();

namespace qsyn {

namespace zx {

/*****************************************************/
/*   class ZXGraph partition functions.              */
/*****************************************************/

struct DirectionalZXCutHash {
    size_t operator()(ZXCut const& cut) const {
        auto [v1, v2, edgeType] = cut;
        return std::hash<ZXVertex*>()(v1) ^ std::hash<ZXVertex*>()(v2) ^ std::hash<EdgeType>()(edgeType);
    };
};

/**
 * @brief Creates a list of subgraphs from a list of partitions
 *
 * @param partitions The list of partitions to split the graph into
 *
 * @return A pair of the list of subgraphs and the list of cuts between the subgraphs
 */
std::pair<std::vector<ZXGraph*>, std::vector<ZXCut>> ZXGraph::create_subgraphs(std::vector<ZXVertexList> partitions) {
    std::vector<ZXGraph*> subgraphs;
    // stores the two sides of the cut and the edge type
    ZXCutSet inner_cuts;
    // stores the two boundary vertices and the edge type corresponding to the cut
    std::vector<ZXCut> outer_cuts;
    std::unordered_map<ZXCut, ZXVertex*, DirectionalZXCutHash> cut_to_boundary;

    ZXVertexList primary_inputs  = get_inputs();
    ZXVertexList primary_outputs = get_outputs();

    // by pass the output qubit id collision check in the copy constructor
    int next_boundary_qubit_id = INT_MIN;

    for (auto& partition : partitions) {
        ZXVertexList subgraph_inputs;
        ZXVertexList subgraph_outputs;
        std::vector<ZXVertex*> boundary_vertices;

        size_t next_vertex_id = 0;
        for (auto const& vertex : partition) {
            vertex->set_id(next_vertex_id);
            next_vertex_id++;
        }

        for (auto const& vertex : partition) {
            if (primary_inputs.contains(vertex)) subgraph_inputs.insert(vertex);
            if (primary_outputs.contains(vertex)) subgraph_outputs.insert(vertex);

            std::vector<NeighborPair> neighbors_to_remove;
            std::vector<NeighborPair> neighbors_to_add;
            for (auto const& [neighbor, edgeType] : vertex->get_neighbors()) {
                if (!partition.contains(neighbor)) {
                    ZXVertex* boundary = new ZXVertex(next_vertex_id++, next_boundary_qubit_id++, VertexType::boundary);
                    inner_cuts.insert({vertex, neighbor, edgeType});
                    cut_to_boundary[{vertex, neighbor, edgeType}] = boundary;

                    neighbors_to_remove.push_back({neighbor, edgeType});
                    neighbors_to_add.push_back({boundary, edgeType});

                    boundary->add_neighbor(vertex, edgeType);

                    boundary_vertices.push_back(boundary);
                    subgraph_outputs.insert(boundary);
                }
            }

            for (auto const& neighbor_pair : neighbors_to_add) {
                vertex->add_neighbor(neighbor_pair);
            }
            for (auto const& neighbor_pair : neighbors_to_remove) {
                vertex->remove_neighbor(neighbor_pair);
            }
        }

        for (auto const& vertex : boundary_vertices) {
            partition.insert(vertex);
        }
        subgraphs.push_back(new ZXGraph(partition, subgraph_inputs, subgraph_outputs));
    }

    for (auto&& [i, g] : tl::views::enumerate(subgraphs)) {
        spdlog::debug("subgraph {}", i);
        g->print_vertices(spdlog::level::debug);
    }

    for (auto [v1, v2, edge_type] : inner_cuts) {
        ZXVertex* b1 = cut_to_boundary.at({v1, v2, edge_type});
        ZXVertex* b2 = cut_to_boundary.at({v2, v1, edge_type});
        outer_cuts.push_back({b1, b2, edge_type});
    }

    // ownership of the vertices is transferred to the subgraphs
    release();

    return {subgraphs, outer_cuts};
}

/**
 * @brief Creates a new ZXGraph from a list of subgraphs and a list of cuts
 *        between the subgraphs. Deletes the subgraphs after merging.
 *
 * @param subgraphs The list of subgraphs to merge
 * @param cuts The list of cuts between the subgraph (boundary vertices)
 *
 * @return The merged ZXGraph
 *
 */
ZXGraph* ZXGraph::from_subgraphs(std::vector<ZXGraph*> const& subgraphs, std::vector<ZXCut> const& cuts) {
    ZXVertexList vertices;
    ZXVertexList inputs;
    ZXVertexList outputs;

    for (auto subgraph : subgraphs) {
        vertices.insert(subgraph->get_vertices().begin(), subgraph->get_vertices().end());
        inputs.insert(subgraph->get_inputs().begin(), subgraph->get_inputs().end());
        outputs.insert(subgraph->get_outputs().begin(), subgraph->get_outputs().end());
    }

    for (auto [b1, b2, edgeType] : cuts) {
        ZXVertex* v1 = b1->get_first_neighbor().first;
        ZXVertex* v2 = b2->get_first_neighbor().first;

        EdgeType e1            = v1->is_neighbor(b1, EdgeType::simple) ? EdgeType::simple : EdgeType::hadamard;
        EdgeType e2            = v2->is_neighbor(b2, EdgeType::simple) ? EdgeType::simple : EdgeType::hadamard;
        EdgeType new_edge_type = (e1 == EdgeType::hadamard) ^ (e2 == EdgeType::hadamard) ^ (edgeType == EdgeType::hadamard)
                                     ? EdgeType::hadamard
                                     : EdgeType::simple;

        v1->remove_neighbor(b1, e1);
        v2->remove_neighbor(b2, e2);
        vertices.erase(b1);
        vertices.erase(b2);
        inputs.erase(b1);
        inputs.erase(b2);
        outputs.erase(b1);
        outputs.erase(b2);
        v1->add_neighbor(v2, new_edge_type);
        v2->add_neighbor(v1, new_edge_type);
        delete b1;
        delete b2;
    }

    for (auto subgraph : subgraphs) {
        // ownership of the vertices is transferred to the merged graph
        subgraph->release();
        delete subgraph;
    }

    return new ZXGraph(vertices, inputs, outputs);
}

/*****************************************************/
/*  ZXGraph partition strategies.                    */
/*****************************************************/

namespace detail {

std::pair<ZXVertexList, ZXVertexList> kl_bipartition(ZXVertexList vertices);

}
/**
 * @brief Recursively partition the graph into numPartitions partitions using the Kernighan-Lin algorithm.
 *
 * @param graph The graph to partition.
 * @param numPartitions The number of partitions to split the graph into.
 *
 * @return A vector of vertex lists, each representing a partition.
 */
std::vector<ZXVertexList> kl_partition(ZXGraph const& graph, size_t n_partitions) {
    std::vector<ZXVertexList> partitions = {graph.get_vertices()};
    size_t count                         = 1;
    while (count < n_partitions) {
        std::vector<ZXVertexList> new_partitions;
        for (auto& partition : partitions) {
            auto [p1, p2] = detail::kl_bipartition(partition);
            partition     = p1;
            new_partitions.push_back(p2);
            if (++count == n_partitions) break;
        }
        partitions.insert(partitions.end(), new_partitions.begin(), new_partitions.end());
    }
    return partitions;
}

std::pair<ZXVertexList, ZXVertexList> detail::kl_bipartition(ZXVertexList vertices) {
    using SwapPair = std::pair<ZXVertex*, ZXVertex*>;

    ZXVertexList partition1 = ZXVertexList();
    ZXVertexList partition2 = ZXVertexList();

    bool toggle = false;
    for (auto v : vertices) {
        if (toggle) {
            partition1.insert(v);
        } else {
            partition2.insert(v);
        }
        toggle = !toggle;
    }

    std::unordered_map<ZXVertex*, int> d_values;
    int cumulative_gain;
    std::stack<SwapPair> swap_history;
    int best_cumulative_gain;
    size_t best_iteration;
    std::unordered_set<ZXVertex*> locked_vertices;

    auto compute_d = [&]() {
        for (auto& v : vertices) {
            int internal_cost = 0;
            int external_cost = 0;

            ZXVertexList& my_partition    = partition1.contains(v) ? partition1 : partition2;
            ZXVertexList& other_partition = partition1.contains(v) ? partition2 : partition1;

            for (auto& [neighbor, edge] : v->get_neighbors()) {
                if (my_partition.contains(neighbor)) {
                    internal_cost++;
                } else if (other_partition.contains(neighbor)) {
                    external_cost++;
                }
            }

            d_values[v] = external_cost - internal_cost;
        }
    };

    auto swap_once = [&]() {
        SwapPair best_swap = {nullptr, nullptr};
        int best_swap_gain = INT_MIN;

        for (auto& v1 : partition1) {
            if (locked_vertices.contains(v1)) continue;
            for (auto& v2 : partition2) {
                if (locked_vertices.contains(v2)) continue;
                int swap_gain = d_values[v1] + d_values[v2] - 2 * (v1->is_neighbor(v2) ? 1 : 0);
                if (swap_gain > best_swap_gain) {
                    best_swap_gain = swap_gain;
                    best_swap      = {v1, v2};
                }
            }
        }

        auto [swap1, swap2] = best_swap;
        partition1.erase(swap1);
        partition2.erase(swap2);
        partition1.insert(swap2);
        partition2.insert(swap1);
        locked_vertices.insert(swap1);
        locked_vertices.insert(swap2);

        for (auto& v1 : partition1) {
            if (locked_vertices.contains(v1)) continue;
            d_values[v1] += 2 * (v1->is_neighbor(swap1) ? 1 : 0) - 2 * (v1->is_neighbor(swap2) ? 1 : 0);
        }
        for (auto& v2 : partition2) {
            if (locked_vertices.contains(v2)) continue;
            d_values[v2] += 2 * (v2->is_neighbor(swap2) ? 1 : 0) - 2 * (v2->is_neighbor(swap1) ? 1 : 0);
        }

        cumulative_gain += best_swap_gain;
        swap_history.push(best_swap);
        if (cumulative_gain >= best_cumulative_gain) {
            best_cumulative_gain = cumulative_gain;
            best_iteration       = swap_history.size();
        }
    };

    size_t iteration = 0;
    while (!stop_requested()) {
        cumulative_gain      = 0;
        swap_history         = std::stack<SwapPair>();
        best_cumulative_gain = INT_MIN;
        best_iteration       = 0;
        locked_vertices.clear();
        compute_d();

        // OPTIMIZE: decide a better stopping condition
        for (size_t _ = 0; _ < partition1.size() - 1; _++) {
            swap_once();
        }

        // OPTIMIZE: decide a better stopping condition
        if (best_cumulative_gain <= 0) {
            break;
        }

        // undo until best iteration
        while (swap_history.size() > best_iteration) {
            auto [swap1, swap2] = swap_history.top();
            swap_history.pop();
            partition1.erase(swap2);
            partition2.erase(swap1);
            partition1.insert(swap1);
            partition2.insert(swap2);
        }
    }

    return std::make_pair(partition1, partition2);
}

}  // namespace zx

}  // namespace qsyn