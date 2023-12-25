/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define data structures for XAG ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./xag.hpp"

#include <unordered_map>
#include <unordered_set>

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
    auto id_to_node = std::unordered_map<XAGNodeID, XAGNode*>();
    for (auto& node : _nodes) {
        id_to_node[node.get_id()] = &node;
    }
    std::vector<XAGNodeID> order;
    std::vector<XAGNodeID> stack;
    std::unordered_set<XAGNodeID> visited;

    for (auto& input : _inputs) {
        stack.emplace_back(input);
    }

    while (!stack.empty()) {
        auto node_id = stack.back();
        stack.pop_back();
        if (visited.contains(node_id)) {
            continue;
        }
        visited.emplace(node_id);

        order.emplace_back(node_id);
        for (auto& output : id_to_node[node_id]->fanouts) {
            if (!visited.contains(output)) {
                stack.emplace_back(output);
            }
        }
    }

    return order;
}

}  // namespace qsyn::qcir
