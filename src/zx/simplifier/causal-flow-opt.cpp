#include <chrono>
#include <cstddef>
#include <optional>
#include <random>
#include <unordered_set>

#include "./heuristics.hpp"
#include "./simplify.hpp"
#include "zx/flow/causal-flow.hpp"
#include "zx/simplifier/rules/rule-matchers.hpp"
#include "zx/simplifier/rules/zx_rules_template.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

namespace qsyn::zx::simplify {

using MatchType = std::variant<
    IdentityFusion,
    LCompUnfusion,
    PivotUnfusion>;

using MatchWithScore = std::pair<MatchType, size_t>;

std::vector<MatchWithScore> get_matches_with_scores(
    ZXGraph const& g, std::optional<ZXVertexList> const& candidates,
    size_t max_lcomp_unfusions, size_t max_pivot_unfusions) {
    using namespace std::chrono;
    auto matches = std::vector<MatchWithScore>{};

    auto const ifu_matcher = IdentityFusionMatcher{};
    auto const lcu_matcher = LCompUnfusionMatcher{max_lcomp_unfusions};
    auto const pvu_matcher = PivotUnfusionMatcher{max_pivot_unfusions};

    auto const ifu_start = high_resolution_clock::now();

    for (auto&& m : ifu_matcher.find_matches(g, candidates)) {
        auto const score = calculate_2q_decrease(m, g);
        if (score > 0) {
            matches.emplace_back(std::move(m), score);
        }
    }

    auto const lcu_start = high_resolution_clock::now();

    for (auto&& m : lcu_matcher.find_matches(g, candidates)) {
        auto const score = calculate_2q_decrease(m, g);
        if (score > 0) {
            matches.emplace_back(std::move(m), score);
        }
    }

    auto const pvu_start = high_resolution_clock::now();

    for (auto&& m : pvu_matcher.find_matches(g, candidates)) {
        auto const score = calculate_2q_decrease(m, g);
        if (score > 0) {
            matches.emplace_back(std::move(m), score);
        }
    }

    auto const pvu_end = high_resolution_clock::now();

    auto const ifu_duration =
        duration_cast<microseconds>(lcu_start - ifu_start).count();
    auto const lcu_duration =
        duration_cast<microseconds>(pvu_start - lcu_start).count();
    auto const pvu_duration =
        duration_cast<microseconds>(pvu_end - pvu_start).count();

    spdlog::debug(
        "{:>5} matches; IFU: {:>5.4f} ms, LCU: {:>5.4f} ms, PVU: {:>5.4f} ms",
        matches.size(),
        static_cast<double>(ifu_duration) / 1'000.0,
        static_cast<double>(lcu_duration) / 1'000.0,
        static_cast<double>(pvu_duration) / 1'000.0);

    return matches;
}

std::unordered_set<size_t> get_search_space(
    ZXGraph const& g, std::vector<size_t> const& affected_vertices) {
    constexpr auto max_radius = std::max({
        IdentityFusion::radius(),
        LCompUnfusion::radius(),
        PivotUnfusion::radius(),
    });

    auto search_space_vec = closed_neighborhood(
        g, affected_vertices, max_radius);

    // build the search space as a ZXVertexList
    auto search_space = std::unordered_set<size_t>{
        affected_vertices.begin(), affected_vertices.end()};

    std::move(search_space_vec.begin(), search_space_vec.end(),
              std::inserter(search_space, search_space.end()));

    return search_space;
}

void update_affected_matches(
    ZXGraph& g, std::vector<MatchWithScore>& matches,
    std::vector<size_t> const& affected_vertices, size_t max_lcomp_unfusions,
    size_t max_pivot_unfusions) {
    auto search_space = get_search_space(g, affected_vertices);

    // remove the matches that becomes invalid or those with changed scores
    std::erase_if(matches, [&](auto const& mws) {
        auto const& [match, _]   = mws;
        auto const core_vertices = std::visit(
            [&](auto&& m) { return m.core_vertices(); }, match);
        return std::ranges::any_of(core_vertices, [&](auto v_id) {
            return search_space.contains(v_id);
        });
    });

    ZXVertexList candidates;

    for (auto const id : search_space) {
        if (g.is_v_id(id)) {
            candidates.emplace(g[id]);
        }
    }

    // update matches

    auto new_matches = get_matches_with_scores(
        g, candidates, max_lcomp_unfusions, max_pivot_unfusions);

    std::move(new_matches.begin(), new_matches.end(),
              std::back_inserter(matches));

    // sort the matches in the ascending order of the score
    // so that we can apply the first match and discard it in O(1) time
    auto const n_old_matches = matches.size();

    std::ranges::inplace_merge(
        matches.begin(),
        matches.begin() + gsl::narrow_cast<std::ptrdiff_t>(n_old_matches),
        matches.end(),
        std::less{}, &MatchWithScore::second);
}

/**
 * @brief Perform the core of the causal flow-preserving simplification as
 *        described in https://arxiv.org/pdf/2312.02793.
 *
 * @param g the graph to simplify
 * @param max_lcomp_unfusions the maximum number of LCompUnfusion to apply
 * @param max_pivot_unfusions the maximum number of PivotUnfusion to apply
 */
void causal_flow_opt(ZXGraph& g,
                     size_t max_lcomp_unfusions,
                     size_t max_pivot_unfusions,
                     size_t max_spider_arity) {
    using namespace std::chrono;
    constexpr auto const to_us = [](auto const dur) {
        return duration_cast<microseconds>(dur).count();
    };

    size_t num_matches_tried   = 0;
    size_t num_matches_applied = 0;
    size_t num_ifus_tried      = 0;
    size_t num_ifus_applied    = 0;
    size_t num_lcus_tried      = 0;
    size_t num_lcus_applied    = 0;
    size_t num_pvus_tried      = 0;
    size_t num_pvus_applied    = 0;

    auto causal_flow_duration = 0ll;
    auto update_duration      = 0ll;

    auto const loop_start_time = high_resolution_clock::now();
    // insert redundant vertices between Z-Z or X-X simple edges

    hadamard_rule_simp(g);
    to_z_graph(g);
    while (!stop_requested()) {
        auto const rule = SpiderFusionRule{};
        auto matches    = rule.find_matches(g);
        std::erase_if(matches, [&](auto const& m) {
            auto const& [v1, v2] = m;
            auto const expected_arity =
                g.num_neighbors(v1) + g.num_neighbors(v2) - 2;
            return expected_arity > max_spider_arity;
        });

        // check emptiness after erasing to avoid infinite loop
        if (matches.empty()) {
            break;
        }

        rule.apply(g, matches);
    }
    redundant_hadamard_insertion(g, 1.0);
    to_graph_like(g);

    if (!has_causal_flow(g)) {
        spdlog::error("The ZXGraph is not causal to begin with!!");
        return;
    }

    auto matches = get_matches_with_scores(
        g, std::nullopt, max_lcomp_unfusions, max_pivot_unfusions);

    while (!matches.empty()) {
        // step 2: identify a best match to apply
        auto [match, score] = std::move(matches.back());
        matches.pop_back();

        std::visit([&](auto&& m) { m.apply_unchecked(g); }, match);

        ++num_matches_tried;
        if (std::holds_alternative<IdentityFusion>(match)) {
            ++num_ifus_tried;
        } else if (std::holds_alternative<LCompUnfusion>(match)) {
            ++num_lcus_tried;
        } else {
            ++num_pvus_tried;
        }

        auto const causal_flow_start = high_resolution_clock::now();
        auto expected_result         = has_causal_flow(g);
        causal_flow_duration +=
            to_us(high_resolution_clock::now() - causal_flow_start);

        if (expected_result) {
            ++num_matches_applied;
            if (std::holds_alternative<IdentityFusion>(match)) {
                ++num_ifus_applied;
            } else if (std::holds_alternative<LCompUnfusion>(match)) {
                ++num_lcus_applied;
            } else {
                ++num_pvus_applied;
            }
            auto isolated_vertices = get_isolated_vertices(g);
            g.remove_vertices(isolated_vertices);

            auto const update_start = high_resolution_clock::now();
            auto affected_vertices  = std::visit(
                [&](auto&& m) { return m.get_affected_vertices(g); }, match);

            std::move(isolated_vertices.begin(), isolated_vertices.end(),
                      std::back_inserter(affected_vertices));

            // accept the match and update the affected matches
            update_affected_matches(g, matches, affected_vertices,
                                    max_lcomp_unfusions, max_pivot_unfusions);
            update_duration +=
                to_us(high_resolution_clock::now() - update_start);

        } else {
            // undo the change and discard the match
            std::visit([&](auto&& m) { m.undo_unchecked(g); }, match);
        }
    }

    auto const total_duration =
        to_us(high_resolution_clock::now() - loop_start_time);

    spdlog::info(
        "Total time: {:.3f} s, {:.3f}s cauculating causal flow ({:.2f}%), "
        "{:.3f}s updating ({:.2f}%)",
        static_cast<double>(total_duration) / 1'000'000.0,
        static_cast<double>(causal_flow_duration) / 1'000'000.0,
        (static_cast<double>(causal_flow_duration) * 100.0) /
            static_cast<double>(total_duration),
        static_cast<double>(update_duration) / 1'000'000.0,
        (static_cast<double>(update_duration) * 100.0) /
            static_cast<double>(total_duration));

    auto const applied_ratio = [](size_t applied, size_t tried) {
        return (static_cast<double>(applied) * 100.0) /
               static_cast<double>(tried);
    };

    spdlog::info("ALL:   Applied {:>8} out of {:>8}. ({:3.2f}%)",
                 num_matches_applied, num_matches_tried,
                 applied_ratio(num_matches_applied, num_matches_tried));

    spdlog::info("- IFU: Applied {:>8} out of {:>8}. ({:3.2f}%)",
                 num_ifus_applied, num_ifus_tried,
                 applied_ratio(num_ifus_applied, num_ifus_tried));

    spdlog::info("- LCU: Applied {:>8} out of {:>8}. ({:3.2f}%)",
                 num_lcus_applied, num_lcus_tried,
                 applied_ratio(num_lcus_applied, num_lcus_tried));

    spdlog::info("- PVU: Applied {:>8} out of {:>8}. ({:3.2f}%)",
                 num_pvus_applied, num_pvus_tried,
                 applied_ratio(num_pvus_applied, num_pvus_tried));
}

void redundant_hadamard_insertion(ZXGraph& g, double prob) {
    hadamard_rule_simp(g);
    to_z_graph(g);

    if (prob <= 0.0) return;

    auto vpairs = std::vector<std::pair<size_t, size_t>>{};

    g.for_each_edge([&](auto const& ep) {
        auto const& [v0, v1] = ep.first;
        if (v0->is_boundary() || v1->is_boundary()) return;
        if (ep.second == EdgeType::simple) {
            vpairs.emplace_back(v0->get_id(), v1->get_id());
        }
    });

    auto num_added = 0ul;
    for (auto const& [v0_id, v1_id] : vpairs) {
        // randomly add...
        static std::mt19937 gen(std::random_device{}());
        if (prob >= 1.0 || std::bernoulli_distribution{prob}(gen)) {
            IdentityAddition{
                v0_id, v1_id, VertexType::z, EdgeType::hadamard}
                .apply_unchecked(g);
            ++num_added;
        }
    }
    spdlog::info("Inserted {} redundant vertices", num_added);
}

}  // namespace qsyn::zx::simplify
