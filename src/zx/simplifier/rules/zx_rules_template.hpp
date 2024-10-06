/************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <string>
#include <vector>

#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

namespace qsyn::zx {

struct ZXOperation {
    // std::vector<ZXVertex*> vertices_to_add;
    std::vector<EdgePair> edges_to_add;
    std::vector<EdgePair> edges_to_remove;
    std::vector<ZXVertex*> vertices_to_remove;
};

class ZXRuleBase {
public:
    virtual ~ZXRuleBase()                = default;
    virtual std::string get_name() const = 0;

protected:
    void _update(ZXGraph& graph, ZXOperation const& op) const {
        // TODO: add vertices is not implemented yet
        for (auto& edge : op.edges_to_add) {
            auto const [v0, v1]      = std::get<0>(edge);
            EdgeType const edge_type = std::get<1>(edge);
            graph.add_edge(v0, v1, edge_type);
        }
        graph.remove_edges(op.edges_to_remove);
        graph.remove_vertices(op.vertices_to_remove);

        graph.remove_isolated_vertices();
    }
};

template <typename T>
class ZXRuleTemplate : public ZXRuleBase {
public:
    using MatchType = T;

    ~ZXRuleTemplate() override = default;

    std::string get_name() const override = 0;

    virtual std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const = 0;

    virtual void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const = 0;
};

/**
 * @brief Template specialization when the match type is a ZXRule class.
 *
 * @tparam T
 */
template <typename T>
requires std::is_base_of<ZXRule, T>::value
class ZXRuleMatcher : public ZXRuleTemplate<T> {
public:
    using MatchType = T;

    ~ZXRuleMatcher() override = default;

    std::string get_name() const override = 0;

    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override = 0;

    void apply(
        ZXGraph& graph,
        std::vector<MatchType> const& matches) const override {
        std::ranges::for_each(
            matches,
            [&graph](auto const& match) {
                match.apply_unchecked(graph);
            });
        graph.remove_isolated_vertices();
    }
};

class BialgebraRule : public ZXRuleTemplate<EdgePair> {
public:
    std::string get_name() const override { return "Bialgebra Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

class StateCopyRule : public ZXRuleTemplate<std::tuple<ZXVertex*, ZXVertex*, std::vector<ZXVertex*>>> {
public:
    std::string get_name() const override { return "State Copy Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

class HadamardFusionRule : public ZXRuleTemplate<ZXVertex*> {
public:
    std::string get_name() const override { return "Hadamard Fusion Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

class IdentityRemovalRule : public ZXRuleMatcher<IdentityRemoval> {
public:
    std::string get_name() const override { return "Identity Removal Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
};

class LocalComplementRule : public ZXRuleMatcher<LComp> {
public:
    std::string get_name() const override { return "Local Complementation Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
};

class PhaseGadgetRule : public ZXRuleTemplate<std::tuple<Phase, std::vector<ZXVertex*>, std::vector<ZXVertex*>>> {
public:
    std::string get_name() const override { return "Phase Gadget Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

class PivotRuleInterface : public ZXRuleTemplate<std::pair<ZXVertex*, ZXVertex*>> {
public:
    std::string get_name() const override = 0;
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override = 0;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

class PivotRule : public ZXRuleMatcher<Pivot> {
public:
    std::string get_name() const override { return "Pivot Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
};

class PivotGadgetRule : public ZXRuleMatcher<PivotUnfusion> {
public:
    std::string get_name() const override { return "Pivot Gadget Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
};

class PivotBoundaryRule : public ZXRuleMatcher<PivotUnfusion> {
public:
    std::string get_name() const override { return "Pivot Boundary Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
    bool is_candidate(ZXGraph& graph, ZXVertex* v0, ZXVertex* v1);
};

class SpiderFusionRule : public ZXRuleTemplate<std::pair<ZXVertex*, ZXVertex*>> {
public:
    std::string get_name() const override { return "Spider Fusion Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

class HadamardRule : public ZXRuleTemplate<ZXVertex*> {
public:
    std::string get_name() const override { return "Hadamard Rule"; }
    std::vector<MatchType> find_matches(
        ZXGraph const& graph,
        std::optional<ZXVertexList> candidates = std::nullopt,
        bool allow_overlapping_candidates      = false) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

}  // namespace qsyn::zx
