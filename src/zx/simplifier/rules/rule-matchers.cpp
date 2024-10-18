#include "./rule-matchers.hpp"

namespace qsyn::zx::simplify {

std::vector<IdentityFusionMatcher::MatchType>
IdentityFusionMatcher::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates) const {
    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    auto matches = std::vector<MatchType>{};

    for (auto const& v : *candidates) {
        if (!IdentityFusion::is_applicable(graph, v->get_id())) {
            continue;
        }
        matches.emplace_back(v->get_id());
    }
    return matches;
}

std::vector<LCompUnfusionMatcher::MatchType>
LCompUnfusionMatcher::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates) const {
    return {};
}

std::vector<PivotUnfusionMatcher::MatchType>
PivotUnfusionMatcher::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates) const {
    return {};
}

}  // namespace qsyn::zx::simplify
