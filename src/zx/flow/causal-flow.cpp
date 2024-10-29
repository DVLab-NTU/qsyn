/****************************************************************************
  PackageName  [ gflow ]
  Synopsis     [ Define causal flow-finding functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./causal-flow.hpp"

#include <ranges>
#include <set>
#include <tl/enumerate.hpp>
#include <unordered_set>
#include <vector>

namespace qsyn::zx {

namespace {

auto get_neighbor_vector(
    ZXGraph const& g,
    ZXVertex* v,
    std::unordered_set<ZXVertex*> const& processed) {
    return g.get_neighbors(v) |
           std::views::keys |
           std::views::filter([&](auto const& p) {
               return !processed.contains(p);
           }) |
           tl::to<std::vector>();
};

/**
 * @brief Loop through the correctors of a ZXGraph. This function is the core
 *        loop of the causal flow calculation. Different lambda functions can be
 *        passed in to tailor the behavior of the loop.
 *
 * @param g
 * @param on_last_neighbor
 * @param on_level_end
 * @return true
 * @return false
 */
bool loop_through_correctors(
    ZXGraph const& g, auto on_last_neighbor, auto on_level_end) {
    auto processed = g.get_outputs() | tl::to<std::unordered_set>();

    auto correctors =
        std::vector<std::pair<ZXVertex*, std::vector<ZXVertex*>>>{};

    for (auto const& v : g.get_outputs()) {
        if (!g.get_inputs().contains(v)) {
            correctors.emplace_back(v, get_neighbor_vector(g, v, processed));
        }
    }

    auto new_correctors = std::vector<ZXVertex*>{};

    while (true) {
        new_correctors.clear();

        for (auto& [v, neighbors] : correctors) {
            std::erase_if(neighbors, [&](auto const& p) {
                return processed.contains(p);
            });

            if (neighbors.size() > 1) continue;

            auto const pred = *std::begin(neighbors);

            on_last_neighbor(v->get_id(), pred->get_id());

            new_correctors.emplace_back(pred);
        }

        if (new_correctors.empty()) {
            return processed.size() == g.num_vertices();
        }

        processed.insert(new_correctors.begin(), new_correctors.end());
        std::erase_if(correctors, [](auto const& pair) {
            return pair.second.size() == 1;
        });

        for (auto const& v : new_correctors) {
            if (!g.get_inputs().contains(v)) {
                correctors.emplace_back(
                    v, get_neighbor_vector(g, v, processed));
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
 * @brief Check if a ZXGraph has a causal flow. This function does not record
 *        the causal flow and should be a little bit faster than
 *        `calculate_causal_flow`.
 *
 * @param g
 * @return true
 * @return false
 */
bool has_causal_flow(ZXGraph const& g) {
    return loop_through_correctors(g, [](auto, auto) {}, []() {});
}

}  // namespace qsyn::zx
