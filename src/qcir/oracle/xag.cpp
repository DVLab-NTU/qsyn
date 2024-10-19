/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define data structures for XAG ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./xag.hpp"

#include <aig/aig/aig.h>
#include <base/abc/abc.h>
#include <spdlog/spdlog.h>

#include <kitty/bit_operations.hpp>
#include <map>
#include <queue>
#include <ranges>
#include <set>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>
#include <tl/zip.hpp>

// TODO: move abc related global variables and functions to a separate file
extern "C" {

Aig_Man_t* Abc_NtkToDar(Abc_Ntk_t* pNtk, int fExors, int fRegisters);  // NOLINT(readability-identifier-naming)  // identifier naming is from ABC
}

using std::views::iota;
using tl::views::enumerate;
using tl::views::zip;

namespace qsyn::qcir {

void XAG::_evaluate_fanouts() {
    for (auto& node : _nodes) {
        for (auto& fanin : node.fanins) {
            _nodes[fanin.get()].fanouts.emplace_back(node.get_id());
        }
    }
    for (auto& node : _nodes) {
        std::ranges::sort(node.fanouts);
    }
}

std::vector<XAGNodeID> XAG::calculate_topological_order() {
    auto id_to_node = std::map<XAGNodeID, XAGNode*>();
    for (auto& node : _nodes) {
        id_to_node[node.get_id()] = &node;
    }
    std::vector<XAGNodeID> order;
    std::set<XAGNodeID> waiting;
    std::set<XAGNodeID> visited;

    for (auto& input : inputs) {
        waiting.emplace(input);
    }

    while (!waiting.empty()) {
        auto node_id = *waiting.begin();
        waiting.erase(waiting.begin());

        visited.emplace(node_id);

        order.emplace_back(node_id);
        for (auto& output : id_to_node[node_id]->fanouts) {
            if (!visited.contains(output)) {
                waiting.emplace(output);
            }
        }
    }

    return order;
}

/*
 * @brief returns the node ids in the cone of the in topological order (top-down)
 */
std::vector<XAGNodeID> XAG::get_cone_node_ids(XAGNodeID const& node_id, XAGCut const& cut) const {
    std::set<XAGNodeID> cone_node_ids_set;
    std::vector<XAGNodeID> cone_node_ids;
    std::queue<XAGNodeID> queue;
    queue.push(node_id);
    while (!queue.empty()) {
        auto const node_id = queue.front();
        queue.pop();
        for (auto const& fanin_id : get_node(node_id).fanins) {
            if (cut.contains(node_id) && !cut.contains(fanin_id)) {
                continue;
            }
            if (!cone_node_ids_set.contains(fanin_id)) {
                queue.push(fanin_id);
            }
        }
        cone_node_ids_set.insert(node_id);
        cone_node_ids.emplace_back(node_id);
    }
    return cone_node_ids;
}

kitty::dynamic_truth_table XAG::calculate_truth_table(XAGNodeID const& output_id, XAGCut const& cut) const {
    auto const& xag       = *this;
    auto const tmp        = xag.get_cone_node_ids(output_id, cut);  // circumvents g++ 11.4 compilation bug
    auto node_ids_in_cone = tmp | std::views::reverse | tl::to<std::vector>();
    auto truth_table      = kitty::dynamic_truth_table(cut.size());

    for (auto const& minterm : iota(0UL, 1UL << cut.size())) {
        std::map<size_t, bool> intermediate_results;
        for (auto const& [i, id] : enumerate(cut)) {
            intermediate_results.insert({id.get(), (minterm >> i) & 1});
        }

        for (auto const& id : node_ids_in_cone) {
            if (cut.contains(id)) {
                continue;
            }

            auto const& node = xag.get_node(id);
            if (!node.is_gate()) {
                continue;
            }

            std::vector<bool> inputs;
            for (auto const& [fanin_id, inverted] : zip(node.fanins, node.fanin_inverted)) {
                inputs.push_back(inverted ^ intermediate_results[fanin_id.get()]);
            }

            bool result{};
            if (node.is_xor()) {
                result = false;
                for (auto const& input : inputs) {
                    result ^= input;
                }
            } else if (node.is_and()) {
                result = true;
                for (auto const& input : inputs) {
                    result &= input;
                }
            }
            intermediate_results.insert({id.get(), result});
        }

        if (intermediate_results[output_id.get()]) {
            kitty::set_bit(truth_table, minterm);
        }
    }

    return truth_table;
}

std::string XAGNode::to_string() const {
    if (_type == XAGNodeType::VOID) {
        return fmt::format("XAGNode({} = VOID)", _id.get());
    }
    if (_type == XAGNodeType::INPUT) {
        return fmt::format("XAGNode({} = INPUT)", _id.get());
    }
    if (_type == XAGNodeType::CONST_1) {
        return fmt::format("XAGNode({} = CONST_1)", _id.get());
    }
    return fmt::format("XAGNode({} = {}{} {} {}{})",
                       _id.get(),
                       fanin_inverted[0] ? "~" : "",
                       fanins[0].get(),
                       _type == XAGNodeType::XOR ? "^" : "&",
                       fanin_inverted[1] ? "~" : "",
                       fanins[1].get());
}

XAG from_xaag(std::istream& input) {
    std::string header;
    size_t num_nodes{};
    size_t num_inputs{};
    size_t num_latches{};
    size_t num_outputs{};
    size_t num_ands{};
    size_t num_xors{};

    input >> header >> num_nodes >> num_inputs >> num_latches >> num_outputs >> num_ands >> num_xors;
    if (header != "xaag") {
        spdlog::error("from_xaag: expected header \"xaag\", but got \"{}\"", header);
        throw std::runtime_error("from_xaag: expected header \"xaag\", but got \"" + header + "\"");
    }
    if (num_latches != 0) {
        spdlog::error("from_xaag: expected 0 latches, but got {}", num_latches);
        throw std::runtime_error("from_xaag: expected 0 latches, but got " + std::to_string(num_latches));
    }

    std::vector<XAGNode> nodes = std::vector<XAGNode>(num_nodes + 1, XAGNode(XAGNodeID(0), {}, {}, XAGNodeType::VOID));
    std::vector<XAGNodeID> input_ids;
    std::vector<XAGNodeID> output_ids;
    std::vector<bool> output_inverted;

    for ([[maybe_unused]] auto const& _ : std::views::iota(0ul, num_inputs)) {
        size_t in{};
        input >> in;
        size_t const id = in >> 1;
        if (id == 0) {
            throw std::runtime_error("from_xaag: input id 0 is reserved for constant 1");
        }
        nodes[id] = XAGNode(XAGNodeID(id), {}, {}, XAGNodeType::INPUT);
        input_ids.emplace_back(id);
    }
    for ([[maybe_unused]] auto const& _ : std::views::iota(0ul, num_outputs)) {
        size_t out{};
        input >> out;
        auto const id          = XAGNodeID(out >> 1);
        bool const is_inverted = out & 1;
        output_ids.emplace_back(id);
        output_inverted.emplace_back(is_inverted);
    }
    for ([[maybe_unused]] auto const& _ : std::views::iota(0ul, num_ands)) {
        size_t and_gate{};
        size_t fanin1{};
        size_t fanin2{};
        input >> and_gate >> fanin1 >> fanin2;
        auto and_gate_id     = XAGNodeID(and_gate >> 1);
        auto fanin1_id       = XAGNodeID(fanin1 >> 1);
        auto fanin2_id       = XAGNodeID(fanin2 >> 1);
        bool fanin1_inverted = fanin1 & 1;
        bool fanin2_inverted = fanin2 & 1;

        if (fanin1_id.get() == 0 || fanin2_id.get() == 0) {
            if (nodes[0].get_type() != XAGNodeType::VOID) {
                nodes[0] = XAGNode(XAGNodeID(0), {}, {}, XAGNodeType::CONST_1);
                input_ids.emplace_back(0);
            }
            fanin1_inverted = !fanin1_inverted;
            fanin2_inverted = !fanin2_inverted;
        }

        nodes[and_gate_id.get()] = XAGNode(and_gate_id,
                                           {fanin1_id, fanin2_id},
                                           {fanin1_inverted, fanin2_inverted},
                                           XAGNodeType::AND);
    }
    for ([[maybe_unused]] auto const& _ : std::views::iota(0ul, num_xors)) {
        size_t xor_gate{};
        size_t fanin1{};
        size_t fanin2{};
        input >> xor_gate >> fanin1 >> fanin2;
        auto xor_gate_id     = XAGNodeID(xor_gate >> 1);
        auto fanin1_id       = XAGNodeID(fanin1 >> 1);
        auto fanin2_id       = XAGNodeID(fanin2 >> 1);
        bool fanin1_inverted = fanin1 & 1;
        bool fanin2_inverted = fanin2 & 1;

        if (fanin1_id.get() == 0 || fanin2_id.get() == 0) {
            if (nodes[0].get_type() != XAGNodeType::VOID) {
                nodes[0] = XAGNode(XAGNodeID(0), {}, {}, XAGNodeType::CONST_1);
                input_ids.emplace_back(0);
            }
            fanin1_inverted = !fanin1_inverted;
            fanin2_inverted = !fanin2_inverted;
        }

        nodes[xor_gate_id.get()] = XAGNode(xor_gate_id,
                                           {fanin1_id, fanin2_id},
                                           {fanin1_inverted, fanin2_inverted},
                                           XAGNodeType::XOR);
    }

    return XAG(nodes, input_ids, output_ids, output_inverted);
}

XAG from_abc_ntk(Abc_Ntk_t* p_ntk) {
    int const f_exors = 1;
    auto p_aig        = Abc_NtkToDar(p_ntk, f_exors, 0);

    size_t const num_nodes     = Aig_ManObjNum(p_aig);
    std::vector<XAGNode> nodes = std::vector<XAGNode>(num_nodes, XAGNode(XAGNodeID(0), {}, {}, XAGNodeType::VOID));
    std::vector<XAGNodeID> input_ids;
    std::vector<XAGNodeID> output_ids;
    std::vector<bool> output_inverted;

    size_t const max_id = Aig_ManObjNumMax(p_aig);

    std::vector<XAGNodeID> obj_id_to_node_id(max_id, XAGNodeID(-1));

    int abc_const_1_id{};
    {
        size_t node_id_counter = 0;
        int _{};
        Aig_Obj_t* p_obj{};
        Aig_ManForEachObj(p_aig, p_obj, _) {
            if (Aig_ObjIsCo(p_obj)) {
                continue;
            } else if (Aig_ObjIsConst1(p_obj)) {
                abc_const_1_id = Aig_ObjId(p_obj);
            }
            obj_id_to_node_id[Aig_ObjId(p_obj)] = XAGNodeID(node_id_counter++);
        }
    }

    bool need_constant_1 = false;
    {
        int _{};
        Aig_Obj_t* p_obj{};
        Aig_ManForEachObj(p_aig, p_obj, _) {
            auto node_id = obj_id_to_node_id[Aig_ObjId(p_obj)];
            if (Aig_ObjIsNode(p_obj)) {
                auto fanin0_id = Aig_ObjFaninId0(p_obj);
                auto fanin1_id = Aig_ObjFaninId1(p_obj);
                if (fanin0_id == abc_const_1_id || fanin1_id == abc_const_1_id) {
                    need_constant_1 = true;
                }
                nodes[node_id.get()] = XAGNode(node_id,
                                               {obj_id_to_node_id[fanin0_id],
                                                obj_id_to_node_id[fanin1_id]},
                                               {(bool)Aig_ObjFaninC0(p_obj),
                                                (bool)Aig_ObjFaninC1(p_obj)},
                                               Aig_ObjIsAnd(p_obj) ? XAGNodeType::AND : XAGNodeType::XOR);
            } else if (Aig_ObjIsCi(p_obj)) {
                nodes[node_id.get()] = XAGNode(node_id, {}, {}, XAGNodeType::INPUT);
                input_ids.emplace_back(node_id);
            } else if (Aig_ObjIsConst1(p_obj)) {
                continue;
            } else if (Aig_ObjIsCo(p_obj)) {
                if (Aig_ObjFaninId0(p_obj) == abc_const_1_id) {
                    need_constant_1 = true;
                }
                auto fanin_id = obj_id_to_node_id[Aig_ObjFaninId0(p_obj)];
                output_ids.emplace_back(fanin_id);
                if (Aig_ObjFaninC0(p_obj)) {
                    output_inverted.emplace_back(true);
                } else {
                    output_inverted.emplace_back(false);
                }
            }
        }
    }

    if (need_constant_1) {
        auto node_id         = obj_id_to_node_id[abc_const_1_id];
        nodes[node_id.get()] = XAGNode(XAGNodeID(abc_const_1_id), {}, {}, XAGNodeType::CONST_1);
        input_ids.emplace_back(0);
    }

    XAG xag = XAG(nodes, input_ids, output_ids, output_inverted);
    return xag;
}

}  // namespace qsyn::qcir
