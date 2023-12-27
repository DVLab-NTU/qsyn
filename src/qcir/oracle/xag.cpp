/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define data structures for XAG ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./xag.hpp"

#include <spdlog/spdlog.h>

#include <map>
#include <ranges>
#include <set>

namespace qsyn::qcir {

void XAG::evaluate_fanouts() {
    for (auto& node : _nodes) {
        for (auto& fanin : node.fanins) {
            _nodes[fanin.get()].fanouts.emplace_back(node.get_id());
        }
    }
    for (auto& node : _nodes) {
        std::sort(node.fanouts.begin(), node.fanouts.end());
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

std::string XAGNode::to_string() const {
    if (_type == XAGNodeType::INPUT) {
        return fmt::format("XAGNode({} = INPUT)", _id.get());
    }
    return fmt::format("XAGNode({} = {}{} {} {}{})",
                       _id.get(), inverted[0] ? "~" : "", fanins[0].get(),
                       _type == XAGNodeType::XOR ? "^" : "&",
                       inverted[1] ? "~" : "",
                       fanins[1].get());
}

XAG from_xaag(std::istream& input) {
    std::string header;
    size_t max_id{};
    size_t num_inputs{};
    size_t num_latches{};
    size_t num_outputs{};
    size_t num_ands{};
    size_t num_xors{};

    input >> header >> max_id >> num_inputs >> num_latches >> num_outputs >> num_ands >> num_xors;
    if (header != "xaag") {
        spdlog::error("from_xaag: expected header \"xaag\", but got \"{}\"", header);
        throw std::runtime_error("from_xaag: expected header \"xaag\", but got \"" + header + "\"");
    }
    if (num_latches != 0) {
        spdlog::error("from_xaag: expected 0 latches, but got {}", num_latches);
        throw std::runtime_error("from_xaag: expected 0 latches, but got " + std::to_string(num_latches));
    }

    std::vector<XAGNode> nodes = std::vector<XAGNode>(max_id, XAGNode(XAGNodeID(0), {}, {}, XAGNodeType::INPUT));
    std::vector<XAGNodeID> inputs_ids;
    std::vector<XAGNodeID> output_ids;

    for ([[maybe_unused]] auto const& _ : std::views::iota(0ul, num_inputs)) {
        size_t in{};
        input >> in;
        size_t const id = (in >> 1) - 1;
        nodes[id]       = XAGNode(XAGNodeID(id), {}, {}, XAGNodeType::INPUT);
        inputs_ids.emplace_back(id);
    }
    for ([[maybe_unused]] auto const& _ : std::views::iota(0ul, num_outputs)) {
        size_t out{};
        input >> out;
        if (out & 1) {
            spdlog::error("from_xaag: only supports positive gate result as output, but got {}", out);
            throw std::runtime_error("from_xaag: only supports positive gate result as output, but got " +
                                     std::to_string(out));
        }
        size_t const id = (out >> 1) - 1;
        output_ids.emplace_back(id);
    }
    for ([[maybe_unused]] auto const& _ : std::views::iota(0ul, num_ands)) {
        size_t and_gate{};
        size_t fanin1{};
        size_t fanin2{};
        input >> and_gate >> fanin1 >> fanin2;
        auto and_gate_id         = XAGNodeID((and_gate >> 1) - 1);
        auto fanin1_id           = XAGNodeID((fanin1 >> 1) - 1);
        auto fanin2_id           = XAGNodeID((fanin2 >> 1) - 1);
        nodes[and_gate_id.get()] = XAGNode(and_gate_id, {fanin1_id, fanin2_id}, {false, false}, XAGNodeType::AND);
    }
    for ([[maybe_unused]] auto const& _ : std::views::iota(0ul, num_xors)) {
        size_t xor_gate{};
        size_t fanin1{};
        size_t fanin2{};
        input >> xor_gate >> fanin1 >> fanin2;
        auto xor_gate_id         = XAGNodeID((xor_gate >> 1) - 1);
        auto fanin1_id           = XAGNodeID((fanin1 >> 1) - 1);
        auto fanin2_id           = XAGNodeID((fanin2 >> 1) - 1);
        nodes[xor_gate_id.get()] = XAGNode(xor_gate_id, {fanin1_id, fanin2_id}, {false, false}, XAGNodeType::XOR);
    }

    return XAG(nodes, inputs_ids, output_ids);
}

}  // namespace qsyn::qcir
