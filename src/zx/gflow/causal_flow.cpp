/****************************************************************************
  PackageName  [ gflow ]
  Synopsis     [ Define causal flow-finding functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./causal_flow.hpp"

#include <chrono>
#include <ranges>
#include <unordered_set>
#include <vector>

namespace qsyn::zx {

namespace {

auto const get_neighbor_vector = [](ZXGraph const& g, ZXVertex* v) {
    return g.get_neighbors(v) | std::views::keys | tl::to<std::vector>();
};

void erase_last_occurrence(std::ranges::range auto& range, auto const& value) {
    if (auto it = std::ranges::find(
            range | std::views::reverse, value);
        it != range.rend()) {
        range.erase(std::prev(it.base()));
    }
}

bool loop_through_correctors(
    ZXGraph const& g, auto on_last_neighbor, auto on_level_end) {
    auto processed = g.get_outputs() | tl::to<std::vector>();

    auto correctors =
        std::vector<std::pair<ZXVertex*, std::vector<ZXVertex*>>>{};

    for (auto const& v : g.get_outputs()) {
        if (!g.get_inputs().contains(v)) {
            correctors.emplace_back(v, get_neighbor_vector(g, v));
        }
    }

    auto new_correctors = std::vector<ZXVertex*>{};
    auto old_correctors = std::vector<ZXVertex*>{};

    while (true) {
        new_correctors.clear();
        old_correctors.clear();

        for (auto& [v, neighbors] : correctors) {
            for (auto const& p : processed) {
                erase_last_occurrence(neighbors, p);
            }

            if (neighbors.size() > 1) continue;

            auto const pred = *std::begin(neighbors);

            on_last_neighbor(v->get_id(), pred->get_id());

            new_correctors.emplace_back(pred);
            old_correctors.emplace_back(v);
        }

        if (new_correctors.empty()) {
            return processed.size() == g.num_vertices();
        }

        processed.insert(
            processed.end(), new_correctors.begin(), new_correctors.end());

        for (auto const& v : old_correctors) {
            std::erase_if(correctors, [&](auto const& p) {
                return p.first == v;
            });
        }

        for (auto const& v : new_correctors) {
            if (!g.get_inputs().contains(v)) {
                correctors.emplace_back(v, get_neighbor_vector(g, v));
            }
        }

        on_level_end();
    }
}

}  // namespace

/**
 * @brief calculate the causal flow of a ZXGraph. If the graph is not causal,
 *        return std::nullopt. The source code is an optimized version of
 *        https://github.com/calumholker/pyzx/blob/master/pyzx/flow.py
 *
 * @param g
 * @return std::optional<CausalFlow>
 * @ref Perdrix & Mhalla, "Finding Optimal Flows Efficiently."
 *      arXiv: https://arxiv.org/abs/0709.2670
 */
std::optional<CausalFlow> calculate_causal_flow(ZXGraph const& g) {
    CausalFlow flow{.order     = {},
                    .successor = {},
                    .depth     = 1};

    flow.order.reserve(g.num_vertices());
    flow.successor.reserve(g.num_vertices());

    auto const success = loop_through_correctors(
        g,
        [&](size_t v, size_t pred) {
            flow.order.emplace(v, flow.depth);
            flow.successor.emplace(pred, v);
        },
        [&]() { ++flow.depth; });

    return success ? std::make_optional(flow) : std::nullopt;
}

/**
 * @brief Specialized version of calculate_causal_flow that only returns the
 *        successor map.
 *
 * @param g
 * @return std::optional<CausalFlow::SuccessorMap>
 */
std::optional<CausalFlow::VertexRelation>
calculate_causal_flow_predecessor_map(ZXGraph const& g) {
    CausalFlow::VertexRelation predecessor;

    predecessor.reserve(g.num_vertices());

    auto const success = loop_through_correctors(
        g,
        [&](size_t v, size_t pred) {
            predecessor.emplace(v, pred);
        },
        [&]() {});

    return success ? std::make_optional(predecessor) : std::nullopt;
}

/**
 * @brief Remove the parts of the flow that are affected by the given vertices
 *
 * @param flow
 * @param affected_vertices
 */
void cut_predecessor_map(CausalFlow::VertexRelation& predecessor_map,
                         std::vector<size_t> const& affected_vertices) {
    for (auto const v_id : affected_vertices) {
        predecessor_map.erase(v_id);
    }

    auto to_erase = std::vector<size_t>{};
    for (auto const& [v, pred] : predecessor_map) {
        if (dvlab::contains(affected_vertices, pred)) {
            to_erase.emplace_back(v);
        }
    }

    for (auto const v_id : to_erase) {
        predecessor_map.erase(v_id);
    }

    // REVIEW - check that the affected vertices are not in the predecessor map
    // remove this after debugging
    for (auto const& [v, pred] : predecessor_map) {
        assert(!dvlab::contains(affected_vertices, v));
        assert(!dvlab::contains(affected_vertices, pred));
    }
}

}  // namespace qsyn::zx
