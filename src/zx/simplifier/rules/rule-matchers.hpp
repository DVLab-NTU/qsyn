#pragma once

#include "./zx_rules_template.hpp"
#include "zx/zxgraph_action.hpp"

namespace qsyn::zx::simplify {

/**
 * @brief Calculate the 2Q-cost decrease of a rule. This function assumes that
 *        the rule is applied to a graph-like ZXGraph admitting a causal flow
 *        and the causal flow is preserved after the rule is applied.
 *
 * @tparam Rule the type of the rule to apply. Each rule should implement its
 *         own `calculate_2q_decrease` specialization.
 * @param rule
 * @param g
 * @return the 2Q-cost decrease. The higher the better.
 */
template <typename Rule>
requires std::is_base_of_v<ZXRule, Rule>
std::vector<Rule> match_all(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates = std::nullopt);

struct IdentityFusionMatcher : public ZXRuleMatcher<IdentityFusion> {
    std::string get_name() const override { return "Identity Fusion"; }

    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt) const override;
};

struct LCompUnfusionMatcher : public ZXRuleMatcher<LCompUnfusion> {
    LCompUnfusionMatcher(size_t num_max_unfusions)
        : _num_max_unfusions(num_max_unfusions) {}

    std::string get_name() const override { return "LComp Unfusion"; }

    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt) const override;

    auto const& num_max_unfusions() const { return _num_max_unfusions; }
    auto& num_max_unfusions() { return _num_max_unfusions; }

private:
    size_t _num_max_unfusions;
};

struct PivotUnfusionMatcher : public ZXRuleMatcher<PivotUnfusion> {
    PivotUnfusionMatcher(size_t num_max_unfusions)
        : _num_max_unfusions(num_max_unfusions) {}

    std::string get_name() const override { return "Pivot Unfusion"; }

    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt) const override;

    auto const& num_max_unfusions() const { return _num_max_unfusions; }
    auto& num_max_unfusions() { return _num_max_unfusions; }

private:
    size_t _num_max_unfusions;
};

}  // namespace qsyn::zx::simplify
