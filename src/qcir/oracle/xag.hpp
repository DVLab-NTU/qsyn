/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define data structures for XAG ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <NamedType/named_type.hpp>
#include <NamedType/underlying_functionalities.hpp>
#include <vector>

namespace qsyn::qcir {

enum class XAGNodeType {
    INPUT,
    XOR,
    AND,
};

using XAGNodeID = fluent::NamedType<size_t, struct XAGNodeIDParameter, fluent::Comparable, fluent::Hashable>;

class XAGNode {
public:
    XAGNode(const XAGNodeID id, const std::vector<XAGNodeID> fanins, const std::vector<XAGNodeID> fanouts, XAGNodeType type) : fanins(fanins), fanouts(fanouts), _id(id), _type(type) {}

    XAGNodeID get_id() const { return _id; }
    XAGNodeType get_type() const { return _type; }

    std::vector<XAGNodeID> fanins;
    std::vector<XAGNodeID> fanouts;

private:
    XAGNodeID _id;
    XAGNodeType _type;
};

class XAG {
public:
    XAG(const std::vector<XAGNode> nodes, const std::vector<XAGNodeID> inputs, const std::vector<XAGNodeID> outputs) : _nodes(nodes), _inputs(inputs), _outputs(outputs) {}
    size_t size() const { return _nodes.size(); }
    XAGNode* get_node(XAGNodeID id) { return &_nodes[id.get()]; }
    void set_node(size_t id, XAGNode node) { _nodes[id] = node; }
    std::vector<XAGNodeID> calculate_topological_order();

private:
    std::vector<XAGNode> _nodes;
    std::vector<XAGNodeID> _inputs;
    std::vector<XAGNodeID> _outputs;

    void evaluate_fanouts();
};

}  // namespace qsyn::qcir
