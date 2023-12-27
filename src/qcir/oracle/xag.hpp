/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define data structures for XAG ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/format.h>

#include <NamedType/named_type.hpp>
#include <NamedType/underlying_functionalities.hpp>
#include <set>
#include <vector>

namespace qsyn::qcir {

enum class XAGNodeType {
    INPUT,
    XOR,
    AND,
};

using XAGNodeID = fluent::NamedType<size_t, struct XAGNodeIDTag, fluent::Comparable, fluent::Hashable>;
using XAGCut    = std::set<XAGNodeID>;

class XAGNode {
public:
    XAGNode(const XAGNodeID id, const std::vector<XAGNodeID> fanins, const std::vector<bool> inverted, XAGNodeType type) : fanins(fanins), inverted(inverted), _id(id), _type(type) {}

    XAGNodeID get_id() const { return _id; }
    XAGNodeType get_type() const { return _type; }
    std::string to_string() const;

    std::vector<XAGNodeID> fanins;
    std::vector<bool> inverted;
    std::vector<XAGNodeID> fanouts;

private:
    XAGNodeID _id;
    XAGNodeType _type;
};

class XAG {
public:
    XAG(const std::vector<XAGNode> nodes, const std::vector<XAGNodeID> inputs, const std::vector<XAGNodeID> outputs) : inputs(inputs), outputs(outputs), _nodes(nodes) {
        evaluate_fanouts();
    }
    size_t size() const { return _nodes.size(); }
    XAGNode* get_node(XAGNodeID id) { return &_nodes[id.get()]; }
    void set_node(size_t id, XAGNode node) { _nodes[id] = node; }

    std::vector<XAGNodeID> calculate_topological_order();
    std::vector<XAGNodeID> inputs;
    std::vector<XAGNodeID> outputs;

private:
    void evaluate_fanouts();

    std::vector<XAGNode> _nodes;
};

XAG from_xaag(std::istream& input);

}  // namespace qsyn::qcir
