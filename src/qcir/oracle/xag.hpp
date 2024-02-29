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
#include <kitty/dynamic_truth_table.hpp>
#include <set>
#include <vector>

namespace qsyn::qcir {

// TODO: use output node to represent the output of the circuit
enum class XAGNodeType {
    CONST_1,
    INPUT,
    XOR,
    AND,
    VOID,
};

using XAGNodeID = fluent::NamedType<size_t, struct XAGNodeIDTag, fluent::Comparable, fluent::Hashable>;
using XAGCut    = std::set<XAGNodeID>;

class XAGNode {
public:
    XAGNode() : _id(0), _type(XAGNodeType::VOID) {}
    XAGNode(const XAGNodeID id, const std::vector<XAGNodeID> fanins, const std::vector<bool> inverted, XAGNodeType type)
        : fanins(fanins), fanin_inverted(inverted), _id(id), _type(type) {}

    XAGNodeID get_id() const { return _id; }
    XAGNodeType get_type() const { return _type; }
    bool is_gate() const { return _type == XAGNodeType::AND || _type == XAGNodeType::XOR; }
    bool is_and() const { return _type == XAGNodeType::AND; }
    bool is_xor() const { return _type == XAGNodeType::XOR; }
    bool is_valid() const { return _type != XAGNodeType::VOID; }
    bool is_input() const { return _type == XAGNodeType::INPUT || _type == XAGNodeType::CONST_1; }
    std::string to_string() const;

    std::vector<XAGNodeID> fanins;
    std::vector<bool> fanin_inverted;
    std::vector<XAGNodeID> fanouts;

private:
    XAGNodeID _id;
    XAGNodeType _type;
};

class XAG {
public:
    XAG() = default;
    XAG(const std::vector<XAGNode> nodes,
        const std::vector<XAGNodeID> inputs,
        const std::vector<XAGNodeID> outputs,
        const std::vector<bool> outputs_inverted)
        : inputs(inputs), outputs(outputs), outputs_inverted(outputs_inverted), _nodes(nodes) {
        evaluate_fanouts();
        assert(outputs.size() == outputs_inverted.size());
    }
    size_t size() const { return _nodes.size(); }
    XAGNode const& get_node(XAGNodeID id) const { return _nodes[id.get()]; }
    void set_node(size_t id, XAGNode node) { _nodes[id] = node; }
    std::vector<XAGNode> const& get_nodes() const { return _nodes; }
    bool is_output(XAGNodeID const& id) const { return std::find(outputs.begin(), outputs.end(), id) != outputs.end(); }
    bool is_input(XAGNodeID const& id) const { return std::find(inputs.begin(), inputs.end(), id) != inputs.end(); }

    std::vector<XAGNodeID> get_cone_node_ids(XAGNodeID const& node_id, XAGCut const& cut) const;
    std::vector<XAGNodeID> calculate_topological_order();
    kitty::dynamic_truth_table calculate_truth_table(XAGNodeID const& output_id, XAGCut const& cut);

    std::vector<XAGNodeID> inputs;
    std::vector<XAGNodeID> outputs;
    std::vector<bool> outputs_inverted;

private:
    void evaluate_fanouts();

    std::vector<XAGNode> _nodes;
};

XAG from_xaag(std::istream& input);

XAG from_abc_ntk(Abc_Ntk_t* pNtk);

}  // namespace qsyn::qcir
