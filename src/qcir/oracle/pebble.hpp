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
#include <optional>
#include <set>

#include "util/sat/sat_solver.hpp"
namespace qsyn::qcir {

using DepGrapgNodeID = fluent::NamedType<size_t, struct DepGrapgNodeIDParameter, fluent::Comparable, fluent::Hashable>;

// dependency graph node
using DepGraphNode = struct DepGraphNode {
    DepGraphNode(DepGrapgNodeID const& id = DepGrapgNodeID(0)) : id(id) {}
    DepGrapgNodeID id{};
    std::vector<DepGraphNode*> dependencies;
};

size_t sanitize_P(size_t const P, size_t const N, size_t const max_deps);

void create_dependency_graph(std::istream& ifs, std::vector<DepGraphNode>& graph, std::set<size_t>& output_ids);

std::optional<std::vector<std::vector<bool>>> pebble(dvlab::sat::SatSolver& solver, size_t const P, std::vector<DepGraphNode> graph, std::set<size_t> output_ids);

/**
 * @brief test ancilla qubit scheduling with SAT based reversible pebbling game
 *
 * @param P number of ancilla qubits
 * @param input
 */
void test_pebble(const size_t P, std::istream& input);

}  // namespace qsyn::qcir
