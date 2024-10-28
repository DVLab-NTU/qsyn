#include <cstddef>
#include <optional>
#include <unordered_set>

#include "./heuristics.hpp"
#include "./simplify.hpp"
#include "zx/gflow/causal_flow.hpp"
#include "zx/simplifier/rules/rule-matchers.hpp"
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
    auto matches = std::vector<MatchWithScore>{};

    auto ifu_matcher = IdentityFusionMatcher{};
    auto lcu_matcher = LCompUnfusionMatcher{max_lcomp_unfusions};
    auto pvu_matcher = PivotUnfusionMatcher{max_pivot_unfusions};

    for (auto&& m : ifu_matcher.find_matches(g, candidates)) {
        auto const score = calculate_2q_decrease(m, g);
        if (score > 0) {
            matches.emplace_back(std::move(m), score);
        }
    }

    for (auto&& m : lcu_matcher.find_matches(g, candidates)) {
        auto const score = calculate_2q_decrease(m, g);
        if (score > 0) {
            matches.emplace_back(std::move(m), score);
        }
    }

    for (auto&& m : pvu_matcher.find_matches(g, candidates)) {
        auto const score = calculate_2q_decrease(m, g);
        if (score > 0) {
            matches.emplace_back(std::move(m), score);
        }
    }

    std::ranges::sort(matches, std::less{}, &MatchWithScore::second);

    return matches;
}

void update_affected_matches(
    ZXGraph& g, std::vector<MatchWithScore>& matches,
    MatchType const& applied_match, size_t max_lcomp_unfusions,
    size_t max_pivot_unfusions) {
    constexpr auto max_radius = std::max({
        IdentityFusion::radius(),
        LCompUnfusion::radius(),
        PivotUnfusion::radius(),
    });

    auto const affected_vertices = std::visit(
        [&](auto&& m) { return m.get_affected_vertices(g); }, applied_match);

    auto const search_space_vec = closed_neighborhood(
        g, affected_vertices, max_radius);

    // build the search space as a ZXVertexList
    auto search_space = std::unordered_set<size_t>{
        affected_vertices.begin(), affected_vertices.end()};

    search_space.insert(search_space_vec.begin(), search_space_vec.end());

    // remove the matches that becomes invalid or those with changed scores
    auto const num_removed = std::erase_if(matches, [&](auto const& mws) {
        auto const& [match, score] = mws;
        auto const core_vertices   = std::visit(
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
    auto const new_matches = get_matches_with_scores(
        g, candidates, max_lcomp_unfusions, max_pivot_unfusions);

    matches.insert(matches.end(), new_matches.begin(), new_matches.end());

    // sort the matches in the ascending order of the score
    // so that we can apply the first match and discard it in O(1) time
    std::ranges::sort(matches, std::less{}, &MatchWithScore::second);
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
                     size_t max_pivot_unfusions) {
    using namespace std::chrono;
    constexpr auto const to_us = [](auto const dur) {
        return duration_cast<microseconds>(dur).count();
    };

    to_graph_like(g);

    if (!has_causal_flow(g)) {
        spdlog::error("The ZXGraph is not causal to begin with!!");
        return;
    }

    auto const start_time = high_resolution_clock::now();

    auto matches = get_matches_with_scores(
        g, std::nullopt, max_lcomp_unfusions, max_pivot_unfusions);

    // sort the matches in the ascending order of the score
    // so that we can apply the first match and discard it in O(1) time

    size_t num_matches_tried   = 0;
    size_t num_matches_applied = 0;
    size_t num_ifus_tried      = 0;
    size_t num_ifus_applied    = 0;
    size_t num_lcus_tried      = 0;
    size_t num_lcus_applied    = 0;
    size_t num_pvus_tried      = 0;
    size_t num_pvus_applied    = 0;

    auto const loop_start_time = high_resolution_clock::now();
    auto causal_flow_duration  = 0ll;
    auto update_duration       = 0ll;

    while (!matches.empty()) {
        // step 2: identify a best match to apply
        auto [match, score] = std::move(matches.back());
        matches.pop_back();

        // fmt::println("applying match: {}", std::visit(
        //    [&](auto&& m) { return m.to_string(); }, match));

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
            g.remove_isolated_vertices();

            auto const update_start = high_resolution_clock::now();

            // accept the match and update the affected matches
            update_affected_matches(g, matches, match,
                                    max_lcomp_unfusions,
                                    max_pivot_unfusions);
            update_duration +=
                to_us(high_resolution_clock::now() - update_start);
        } else {
            // undo the change and discard the match
            std::visit([&](auto&& m) { m.undo_unchecked(g); }, match);
        }
    }

    auto const total_duration =
        to_us(high_resolution_clock::now() - loop_start_time);

    fmt::println(
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

    fmt::println("ALL:   Applied {:>8} out of {:>8}. ({:3.2f}%)",
                 num_matches_applied, num_matches_tried,
                 applied_ratio(num_matches_applied, num_matches_tried));

    fmt::println("- IFU: Applied {:>8} out of {:>8}. ({:3.2f}%)",
                 num_ifus_applied, num_ifus_tried,
                 applied_ratio(num_ifus_applied, num_ifus_tried));

    fmt::println("- LCU: Applied {:>8} out of {:>8}. ({:3.2f}%)",
                 num_lcus_applied, num_lcus_tried,
                 applied_ratio(num_lcus_applied, num_lcus_tried));

    fmt::println("- PVU: Applied {:>8} out of {:>8}. ({:3.2f}%)",
                 num_pvus_applied, num_pvus_tried,
                 applied_ratio(num_pvus_applied, num_pvus_tried));
}

}  // namespace qsyn::zx::simplify
