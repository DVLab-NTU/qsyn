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

#include "qcir/oracle/xag.hpp"
#include "util/sat/sat_solver.hpp"

namespace qsyn::qcir {

using DepGraphNodeID = fluent::NamedType<size_t, struct DepGrapgNodeIDParameter, fluent::Comparable, fluent::Hashable>;

// dependency graph node
class DepGraphNode {
public:
    DepGraphNode(DepGraphNodeID const& id, XAGNodeID const& xag_id = XAGNodeID(0)) : id{id}, xag_id{xag_id} {}
    DepGraphNode(DepGraphNodeID const& id, XAGNodeID const& xag_id, std::vector<DepGraphNodeID> deps)
        : id(id), xag_id{xag_id}, dependencies(deps) {}

    DepGraphNodeID id;
    XAGNodeID xag_id;
    std::vector<DepGraphNodeID> dependencies;
};

class DepGraph {
public:
    DepGraph() = default;
    void add_node(DepGraphNode const node) { graph.emplace(node.id, node); }
    void add_output(DepGraphNode const node) {
        add_node(node);
        output_ids.insert(node.id);
    }
    DepGraphNode& get_node(DepGraphNodeID const& id) { return graph.at(id); }
    DepGraphNode const& get_node(DepGraphNodeID const& id) const { return graph.at(id); }
    bool is_output(DepGraphNodeID const& id) const { return output_ids.contains(id); }
    size_t size() const { return graph.size(); }
    size_t output_size() const { return output_ids.size(); }
    std::map<DepGraphNodeID, DepGraphNode> const& get_graph() const { return graph; }

private:
    std::map<DepGraphNodeID, DepGraphNode> graph;
    std::set<DepGraphNodeID> output_ids;
};

size_t sanitize_n_ancilla(size_t const P, size_t const N, size_t const max_deps);

DepGraph from_deps_file(std::istream& ifs);

DepGraph from_xag_cuts(std::map<XAGNodeID, XAGCut> const& optimal_cut);

std::optional<std::vector<std::vector<bool>>> pebble(dvlab::sat::SatSolver& solver, size_t const P, DepGraph graph);

/**
 * @brief test ancilla qubit scheduling with SAT based reversible pebbling game
 *
 * @param P number of ancilla qubits
 * @param input
 */
void test_pebble(const size_t P, std::istream& input);

}  // namespace qsyn::qcir