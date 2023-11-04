/************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <vector>

#include "zx/zxgraph.hpp"

namespace qsyn::zx {

struct ZXOperation {
    std::vector<ZXVertex*> vertices_to_add;
    std::vector<EdgePair> edges_to_add;
    std::vector<EdgePair> edges_to_remove;
    std::vector<ZXVertex*> vertices_to_remove;
};

class ZXRuleBase {
public:
    ZXRuleBase(std::string const& n) : _name(n) {}

    std::string get_name() const { return _name; }

protected:
    std::string _name;
    void _update(ZXGraph& graph, ZXOperation const& op) const {
        // TODO: add vertices is not implemented yet
        assert(op.vertices_to_add.empty());

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

    ZXRuleTemplate(std::string const& name) : ZXRuleBase(name) {}
    virtual ~ZXRuleTemplate() = default;

    virtual std::vector<MatchType> find_matches(ZXGraph const& graph) const         = 0;
    virtual void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const = 0;
    virtual std::vector<ZXVertex*> flatten_vertices(MatchType match) const          = 0;
};

// H Box related rules have simliar interface but is used differentlu in simplifier
template <typename T>
class HZXRuleTemplate : public ZXRuleBase {
public:
    using MatchType = T;

    HZXRuleTemplate(std::string const& name) : ZXRuleBase(name) {}
    virtual ~HZXRuleTemplate() = default;

    virtual std::vector<MatchType> find_matches(ZXGraph const& graph) const         = 0;
    virtual void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const = 0;
    virtual std::vector<ZXVertex*> flatten_vertices(MatchType match) const          = 0;
};

class BialgebraRule : public ZXRuleTemplate<EdgePair> {
public:
    BialgebraRule() : ZXRuleTemplate("Bialgebra Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
    std::vector<ZXVertex*> flatten_vertices(MatchType match) const override {
        auto [v0, v1] = match.first;
        return {v0, v1};
    }
};

class StateCopyRule : public ZXRuleTemplate<std::tuple<ZXVertex*, ZXVertex*, std::vector<ZXVertex*>>> {
public:
    StateCopyRule() : ZXRuleTemplate("State Copy Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
    std::vector<ZXVertex*> flatten_vertices(MatchType match) const override {
        auto [v0, v1, vertices] = match;
        vertices.push_back(v0);
        vertices.push_back(v1);
        return vertices;
    }
};

class HadamardFusionRule : public ZXRuleTemplate<ZXVertex*> {
public:
    HadamardFusionRule() : ZXRuleTemplate("Hadmard Fusion Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
    std::vector<ZXVertex*> flatten_vertices(MatchType match) const override { return {match}; }
};

class IdentityRemovalRule : public ZXRuleTemplate<std::tuple<ZXVertex*, ZXVertex*, ZXVertex*, EdgeType>> {
public:
    IdentityRemovalRule() : ZXRuleTemplate("Identity Removal Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
    std::vector<ZXVertex*> flatten_vertices(MatchType match) const override { return {std::get<0>(match), std::get<1>(match), std::get<2>(match)}; }
};

class LocalComplementRule : public ZXRuleTemplate<std::pair<ZXVertex*, std::vector<ZXVertex*>>> {
public:
    LocalComplementRule() : ZXRuleTemplate("Local Complementation Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
    std::vector<ZXVertex*> flatten_vertices(MatchType match) const override {
        auto [v0, vertices] = match;
        vertices.push_back(v0);
        return vertices;
    }
};

class PhaseGadgetRule : public ZXRuleTemplate<std::tuple<Phase, std::vector<ZXVertex*>, std::vector<ZXVertex*>>> {
public:
    PhaseGadgetRule() : ZXRuleTemplate("Phase Gadget Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
    std::vector<ZXVertex*> flatten_vertices(MatchType match) const override {
        auto [_, vertices0, vertices1] = match;
        vertices0.insert(std::end(vertices0), std::begin(vertices1), std::end(vertices1));
        return vertices0;
    }
};

class PivotRuleInterface : public ZXRuleTemplate<std::pair<ZXVertex*, ZXVertex*>> {
public:
    PivotRuleInterface(std::string const& name) : ZXRuleTemplate(name) {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override = 0;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
    std::vector<ZXVertex*> flatten_vertices(MatchType match) const override { return {match.first, match.second}; }
};

class PivotRule : public PivotRuleInterface {
public:
    PivotRule() : PivotRuleInterface("Pivot Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

class PivotGadgetRule : public PivotRuleInterface {
public:
    PivotGadgetRule() : PivotRuleInterface("Pivot Gadget Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

class PivotBoundaryRule : public PivotRuleInterface {
public:
    PivotBoundaryRule() : PivotRuleInterface("Pivot Boundary Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
};

class SpiderFusionRule : public ZXRuleTemplate<std::pair<ZXVertex*, ZXVertex*>> {
public:
    SpiderFusionRule() : ZXRuleTemplate("Spider Fusion Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
    std::vector<ZXVertex*> flatten_vertices(MatchType match) const override { return {match.first, match.second}; }
};

class HadamardRule : public HZXRuleTemplate<ZXVertex*> {
public:
    HadamardRule() : HZXRuleTemplate("Hadamard Rule") {}

    std::vector<MatchType> find_matches(ZXGraph const& graph) const override;
    void apply(ZXGraph& graph, std::vector<MatchType> const& matches) const override;
    std::vector<ZXVertex*> flatten_vertices(MatchType match) const override { return {match}; }
};

}  // namespace qsyn::zx
