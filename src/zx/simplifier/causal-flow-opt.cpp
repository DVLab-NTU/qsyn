#include <cstddef>

#include "./heuristics.hpp"
#include "./simplify.hpp"
#include "zx/gflow/causal_flow.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

namespace qsyn::zx::simplify {

using MatchType = std::variant<
    IdentityFusion,
    LCompUnfusion,
    PivotUnfusion>;

using MatchWithScore = std::pair<MatchType, size_t>;

std::vector<IdentityFusion> match_identity_fusion(ZXGraph const& g) {
    return {};
}

std::vector<LCompUnfusion> match_lcomp_unfusion(ZXGraph const& g) {
    return {};
}

std::vector<PivotUnfusion> match_pivot_unfusion(ZXGraph const& g) {
    return {};
}

void update_affected_matches(
    ZXGraph& g, std::vector<MatchWithScore>& matches, MatchType const& match) {
}

/**
 * @brief Perform the core of the causal flow-preserving simplification as
 *        described in https://arxiv.org/pdf/2312.02793.
 *
 * @param g
 */
void causal_flow_opt(ZXGraph& g) {
    to_graph_like(g);

    auto matches = std::vector<MatchWithScore>{};

    for (auto&& m : match_identity_fusion(g)) {
        auto const score = calculate_2q_decrease(m, g);
        if (score > 0) {
            matches.emplace_back(std::move(m), score);
        }
    }

    for (auto&& m : match_lcomp_unfusion(g)) {
        auto const score = calculate_2q_decrease(m, g);
        if (score > 0) {
            matches.emplace_back(std::move(m), score);
        }
    }

    for (auto&& m : match_pivot_unfusion(g)) {
        auto const score = calculate_2q_decrease(m, g);
        if (score > 0) {
            matches.emplace_back(std::move(m), score);
        }
    }

    // sort the matches in the ascending order of the score
    // so that we can apply the first match and discard it in O(1) time
    std::ranges::sort(matches, std::less{}, &MatchWithScore::second);

    while (!matches.empty()) {
        // step 2: identify a best match to apply
        auto const& [match, score] = matches.back();
        matches.pop_back();

        std::visit([&](auto&& m) { m.apply_unchecked(g); }, match);

        if (causal_flow(g).has_value()) {
            g.remove_isolated_vertices();
            // accept the match and update the affected matches
            update_affected_matches(g, matches, match);
        } else {
            // undo the change and discard the match
            std::visit([&](auto&& m) { m.undo_unchecked(g); }, match);
        }
    }
}

}  // namespace qsyn::zx::simplify
