/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define data structures for XAG ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <base/abc/abc.h>
#include <fmt/format.h>

#include <NamedType/named_type.hpp>
#include <NamedType/underlying_functionalities.hpp>
#include <set>
#include <vector>

namespace qsyn::qcir {

// TODO: use output node to represent the output of the circuit
enum class XAGNodeType {
    CONST_1,
    INPUT,
    OUTPUT,
    XOR,
    AND,
    VOID,
};

using XAGNodeID = fluent::NamedType<size_t, struct XAGNodeIDTag, fluent::Comparable, fluent::Hashable>;
using XAGCut    = std::set<XAGNodeID>;

class XAGNode {
public:
    XAGNode(const XAGNodeID id, const std::vector<XAGNodeID> fanins, const std::vector<bool> inverted, XAGNodeType type) : fanins(fanins), inverted(inverted), _id(id), _type(type) {}

    XAGNodeID get_id() const { return _id; }
    XAGNodeType get_type() const { return _type; }
    bool is_gate() const { return _type == XAGNodeType::AND || _type == XAGNodeType::XOR; }
    bool is_and() const { return _type == XAGNodeType::AND; }
    bool is_xor() const { return _type == XAGNodeType::XOR; }
    bool is_valid() const { return _type != XAGNodeType::VOID; }
    bool is_input() const { return _type == XAGNodeType::INPUT || _type == XAGNodeType::CONST_1; }
    bool is_output() const { return _type == XAGNodeType::OUTPUT; }
    std::string to_string() const;

    std::vector<XAGNodeID> fanins;
    // fan in inverted
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
    std::vector<XAGNodeID> get_cone_node_ids(XAGNodeID const& node_id, XAGCut const& cut);
    std::vector<XAGNodeID> calculate_topological_order();
    std::vector<XAGNodeID> inputs;
    std::vector<XAGNodeID> outputs;

private:
    void evaluate_fanouts();

    std::vector<XAGNode> _nodes;
};

XAG from_xaag(std::istream& input);

XAG from_abc_ntk(Abc_Ntk_t* pNtk);

}  // namespace qsyn::qcir
