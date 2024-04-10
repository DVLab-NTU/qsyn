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
    explicit DepGraphNode(DepGraphNodeID const& id, XAGNodeID const& xag_id = XAGNodeID(0)) : id{id}, xag_id{xag_id} {}
    DepGraphNode(DepGraphNodeID const& id, XAGNodeID const& xag_id, std::vector<DepGraphNodeID> deps)
        : id(id), xag_id{xag_id}, dependencies(std::move(deps)) {}
    std::string to_string() const {
        return fmt::format("DepGraphNode(id: {}, xag_id: {}, deps: [{}])", id.get(), xag_id.get(),
                           fmt::join(dependencies | std::views::transform([](DepGraphNodeID const& id) { return id.get(); }), ", "));
    }

    DepGraphNodeID id;
    XAGNodeID xag_id;
    std::vector<DepGraphNodeID> dependencies;
};

class DepGraph {
public:
    DepGraph() = default;
    void add_node(DepGraphNode node) { _graph.emplace(node.id, std::move(node)); }
    void add_output(DepGraphNodeID const output_id) { _output_ids.insert(output_id); }
    DepGraphNode& get_node(DepGraphNodeID const& id) { return _graph.at(id); }
    DepGraphNode const& get_node(DepGraphNodeID const& id) const { return _graph.at(id); }
    bool is_output(DepGraphNodeID const& id) const { return _output_ids.contains(id); }
    size_t size() const { return _graph.size(); }
    size_t output_size() const { return _output_ids.size(); }
    std::map<DepGraphNodeID, DepGraphNode> const& get_graph() const { return _graph; }
    std::string to_string() const {
        return fmt::format("DepGraph(size: {}, output: [{}],\ngraph:\n{}\n)", size(),
                           fmt::join(_output_ids | std::views::transform([](auto const& id) { return id.get(); }), ", "),
                           fmt::join(_graph | std::views::transform([](auto const& p) { return p.second.to_string(); }), ",\n"));
    }

private:
    std::map<DepGraphNodeID, DepGraphNode> _graph;
    std::set<DepGraphNodeID> _output_ids;
};

DepGraph from_deps_file(std::istream& ifs);

std::optional<DepGraph> from_xag_cuts(XAG const& xag, std::map<XAGNodeID, XAGCut> const& optimal_cut);

std::optional<std::vector<std::vector<bool>>> pebble(dvlab::sat::SatSolver& solver, size_t num_pebbles, DepGraph graph);

/**
 * @brief test ancilla qubit scheduling with SAT based reversible pebbling game
 *
 * @param P number of ancilla qubits
 * @param input
 */
void test_pebble(size_t num_pebbles, std::istream& input);

}  // namespace qsyn::qcir
