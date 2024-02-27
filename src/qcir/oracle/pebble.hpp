/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <NamedType/named_type_impl.hpp>
#include <NamedType/underlying_functionalities.hpp>
#include <cstddef>
#include <istream>
#include <map>
#include <optional>
#include <set>
#include <tl/to.hpp>

#include "util/sat/sat_solver.hpp"

namespace qsyn::qcir {

using DepGraphNodeID = fluent::NamedType<size_t, struct DepGrapgNodeIDParameter, fluent::Comparable, fluent::Hashable>;

// dependency graph node
class DepGraphNode {
public:
    DepGraphNode() : id{} {}
    DepGraphNode(DepGraphNodeID const& id) : id(id) {}

    DepGraphNodeID id;
    std::vector<DepGraphNodeID> dependencies;
};

class DepGraph {
public:
    DepGraph() = default;
    void add_node(DepGraphNode const node) { graph[node.id] = node; }
    void add_output(DepGraphNodeID const& id) {
        add_node(DepGraphNode(id));
        output_ids.insert(id);
    }
    DepGraphNode& get_node(DepGraphNodeID const& id) { return graph.at(id); }
    bool is_output(DepGraphNodeID const& id) const { return output_ids.contains(id); }
    size_t size() const { return graph.size(); }
    size_t output_size() const { return output_ids.size(); }
    std::map<DepGraphNodeID, DepGraphNode> const& get_graph() const { return graph; }

private:
    std::map<DepGraphNodeID, DepGraphNode> graph;
    std::set<DepGraphNodeID> output_ids;
};

size_t sanitize_P(size_t const P, size_t const N, size_t const max_deps);

DepGraph create_dependency_graph(std::istream& ifs);

std::optional<std::vector<std::vector<bool>>> pebble(dvlab::sat::SatSolver& solver, size_t const P, DepGraph graph);

/**
 * @brief test ancilla qubit scheduling with SAT based reversible pebbling game
 *
 * @param P number of ancilla qubits
 * @param input
 */
void test_pebble(const size_t P, std::istream& input);

}  // namespace qsyn::qcir
