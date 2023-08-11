/************************************************************
  FileName     [ zxRulesTemplate.hpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <vector>

#include "zx/zxDef.hpp"
#include "zx/zxGraph.hpp"

struct ZXOperation {
    std::vector<ZXVertex*> verticesToAdd;
    std::vector<EdgePair> edgesToAdd;
    std::vector<EdgePair> edgesToRemove;
    std::vector<ZXVertex*> verticesToRemove;
};

class ZXRuleBase {
public:
    ZXRuleBase(const std::string& _name) : name(_name) {}

    const std::string name;

protected:
    void update(ZXGraph& graph, const ZXOperation& op) const {
        // TODO: add vertices first
        for (auto& edge : op.edgesToAdd) {
            auto [v0, v1] = std::get<0>(edge);
            EdgeType edgeType = std::get<1>(edge);
            graph.addEdge(v0, v1, edgeType);
        }
        graph.removeEdges(op.edgesToRemove);
        graph.removeVertices(op.verticesToRemove);

        graph.removeIsolatedVertices();
    }
};

template <typename T>
class ZXRuleTemplate : public ZXRuleBase {
public:
    using MatchType = T;

    ZXRuleTemplate(const std::string& _name) : ZXRuleBase(_name) {}

    virtual std::vector<MatchType> findMatches(const ZXGraph& graph) const = 0;
    virtual void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const = 0;
};

// H Box related rules have simliar interface but is used differentlu in simplifier
template <typename T>
class HZXRuleTemplate : public ZXRuleBase {
public:
    using MatchType = T;

    HZXRuleTemplate(const std::string& _name) : ZXRuleBase(_name) {}

    virtual std::vector<MatchType> findMatches(const ZXGraph& graph) const = 0;
    virtual void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const = 0;
};

class BialgebraRule : public ZXRuleTemplate<EdgePair> {
public:
    BialgebraRule() : ZXRuleTemplate("Bialgebra Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;

private:
    bool has_dupicate(std::vector<ZXVertex*> vec) const;
};

class StateCopyRule : public ZXRuleTemplate<std::tuple<ZXVertex*, ZXVertex*, std::vector<ZXVertex*>>> {
public:
    StateCopyRule() : ZXRuleTemplate("State Copy Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class HBoxFusionRule : public ZXRuleTemplate<ZXVertex*> {
public:
    HBoxFusionRule() : ZXRuleTemplate("Hadmard Fusion Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class IdRemovalRule : public ZXRuleTemplate<std::tuple<ZXVertex*, ZXVertex*, ZXVertex*, EdgeType>> {
public:
    IdRemovalRule() : ZXRuleTemplate("Identity Removal Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class LocalComplementRule : public ZXRuleTemplate<std::pair<ZXVertex*, std::vector<ZXVertex*>>> {
public:
    LocalComplementRule() : ZXRuleTemplate("Local Complementation Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class PhaseGadgetRule : public ZXRuleTemplate<std::tuple<Phase, std::vector<ZXVertex*>, std::vector<ZXVertex*>>> {
public:
    PhaseGadgetRule() : ZXRuleTemplate("Phase Gadget Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class PivotRuleInterface : public ZXRuleTemplate<std::pair<ZXVertex*, ZXVertex*>> {
public:
    PivotRuleInterface(const std::string& _name) : ZXRuleTemplate(_name) {}

    virtual std::vector<MatchType> findMatches(const ZXGraph& graph) const override = 0;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class PivotRule : public PivotRuleInterface {
public:
    PivotRule() : PivotRuleInterface("Pivot Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class PivotGadgetRule : public PivotRuleInterface {
public:
    PivotGadgetRule() : PivotRuleInterface("Pivot Gadget Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class PivotBoundaryRule : public PivotRuleInterface {
public:
    PivotBoundaryRule() : PivotRuleInterface("Pivot Boundary Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class SpiderFusionRule : public ZXRuleTemplate<std::pair<ZXVertex*, ZXVertex*>> {
public:
    SpiderFusionRule() : ZXRuleTemplate("Spider Fusion Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};

class HadamardRule : public HZXRuleTemplate<ZXVertex*> {
public:
    HadamardRule() : HZXRuleTemplate("Hadamard Rule") {}

    std::vector<MatchType> findMatches(const ZXGraph& graph) const override;
    void apply(ZXGraph& graph, const std::vector<MatchType>& matches) const override;
};
