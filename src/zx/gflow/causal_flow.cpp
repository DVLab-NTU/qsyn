/****************************************************************************
  PackageName  [ gflow ]
  Synopsis     [ Define causal flow-finding functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./causal_flow.hpp"

#include <ranges>
#include <unordered_set>

namespace qsyn::zx {

namespace {

/**
 * @brief return a set containing all elements in a that are not in b
 *
 * @tparam T
 * @param a
 * @param b
 * @return std::unordered_set<T>
 */
template <typename T>
std::unordered_set<T>
set_difference(std::unordered_set<T> const& a, std::unordered_set<T> const& b) {
    auto result = std::unordered_set<T>{};
    std::ranges::copy_if(a, std::inserter(result, std::end(result)),
                         [&b](T const& x) { return b.find(x) == std::end(b); });

    return result;
}

/**
 * @brief remove all elements in b from a
 *
 * @tparam T
 * @param a
 * @param b
 */
template <typename T>
void set_difference_inplace(std::unordered_set<T>& a, std::unordered_set<T> const& b) {
    for (auto const& x : b) {
        a.erase(x);
    }
}

template <typename T>
std::unordered_set<T> set_intersection(std::unordered_set<T> const& a, std::unordered_set<T> const& b) {
    auto result = std::unordered_set<T>{};
    std::ranges::copy_if(a, std::inserter(result, std::end(result)),
                         [&b](T const& x) { return b.find(x) != std::end(b); });

    return result;
}

template <typename T>
void set_union_inplace(std::unordered_set<T>& a, std::unordered_set<T> const& b) {
    for (auto const& x : b) {
        a.insert(x);
    }
}

auto get_neighbor_sets(ZXGraph const& g) {
    auto neighbor_sets = std::unordered_map<ZXVertex*, std::unordered_set<ZXVertex*>>{};

    for (auto const& v : g.get_vertices()) {
        auto const neighbors =
            g.get_neighbors(v) | std::views::keys | tl::to<std::unordered_set>();
        neighbor_sets.emplace(v, neighbors);
    }

    return neighbor_sets;
}

}  // namespace

/**
 * @brief calculate the causal flow of a ZXGraph. If the graph is not causal, return std::nullopt.
 *        The source code is basically a translation from https://github.com/calumholker/pyzx/blob/master/pyzx/flow.py
 *
 * @param g
 * @return std::optional<CausalFlow>
 * @ref Perdrix & Mhalla, "Finding Optimal Flows Efficiently." arXiv: https://arxiv.org/abs/0709.2670
 */
std::optional<CausalFlow> causal_flow(ZXGraph const& g) {
    CausalFlow flow{.order     = {},
                    .successor = {},
                    .depth     = 1};

    auto const inputs     = g.get_inputs() | tl::to<std::unordered_set>();
    auto processed        = g.get_outputs() | tl::to<std::unordered_set>();
    auto const vertices   = g.get_vertices() | tl::to<std::unordered_set>();
    auto const non_inputs = set_difference(vertices, inputs);
    auto correctors       = set_difference(processed, inputs);

    auto const neighbor_sets = get_neighbor_sets(g);

    while (true) {
        auto out_prime = std::unordered_set<ZXVertex*>{};
        auto c_prime   = std::unordered_set<ZXVertex*>{};

        for (auto const& v : correctors) {
            auto const ns = set_difference(neighbor_sets.at(v), processed);

            if (ns.size() > 1) continue;

            auto const u = *std::begin(ns);

            if (v == u) continue;

            flow.order.emplace(v->get_id(), flow.depth);
            flow.successor.emplace(u->get_id(), v->get_id());
            out_prime.insert(u);
            c_prime.insert(v);
        }

        if (out_prime.empty()) {
            if (processed.size() == vertices.size()) {
                return flow;
            } else {
                return std::nullopt;
            }
        }

        set_union_inplace(processed, out_prime);
        set_difference_inplace(correctors, c_prime);
        set_union_inplace(correctors, set_intersection(non_inputs, out_prime));
        flow.depth++;
    }

    return std::nullopt;
}

}  // namespace qsyn::zx
