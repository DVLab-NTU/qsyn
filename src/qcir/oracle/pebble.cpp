/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/oracle/pebble.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <optional>
#include <ranges>
#include <sstream>
#include <tl/enumerate.hpp>
#include <vector>

#include "qcir/oracle/xag.hpp"
#include "util/sat/sat_solver.hpp"

using namespace dvlab::sat;
using std::vector, std::views::iota;
using namespace qsyn::qcir;
using Node   = qsyn::qcir::DepGraphNode;
using NodeID = qsyn::qcir::DepGraphNodeID;

namespace views = std::views;

namespace qsyn::qcir {

DepGraph from_deps_file(std::istream& ifs) {
    auto tmp = vector<vector<size_t>>();

    DepGraph graph{};
    std::string line;
    size_t id{}, dep{};

    getline(ifs, line);

    std::stringstream ss(line);
    while (ss >> id) {
        graph.add_output(NodeID(id));
    }

    while (getline(ifs, line)) {
        std::stringstream ss(line);
        if (!(ss >> id)) {
            break;
        }
        tmp.emplace_back();
        while (ss >> dep) {
            tmp.back().emplace_back(dep);
        }
    }

    for (const auto& i : iota(0UL, tmp.size())) {
        auto node = Node(NodeID(i));
        for (const auto& dep : tmp[i]) {
            node.dependencies.emplace_back(NodeID(dep));
        }
        node.dependencies.shrink_to_fit();
        graph.add_node(node);
    }

    return graph;
}

std::optional<DepGraph> from_xag_cuts(XAG const& xag, std::map<XAGNodeID, XAGCut> const& optimal_cut) {
    for (auto const& node : xag.get_nodes()) {
        if (node.is_const_1()) {
            return std::nullopt;
        }
    }

    auto is_input = [&optimal_cut](XAGNodeID const& id) {
        return optimal_cut.contains(id) && optimal_cut.at(id).contains(id);
    };

    std::vector<XAGNodeID> optimal_cone_tips = optimal_cut |
                                               views::filter([&is_input](auto const entry) { return !is_input(entry.first); }) |
                                               views::keys |
                                               tl::to<std::vector>();

    auto dep_graph = DepGraph();

    std::map<XAGNodeID, DepGraphNodeID> xag_to_dep;
    for (auto const& [i, xag_id] : tl::views::enumerate(optimal_cone_tips)) {
        xag_to_dep[xag_id] = DepGraphNodeID(i);
    }

    for (auto const& xag_id : optimal_cone_tips) {
        auto const dep_id = xag_to_dep[xag_id];
        auto node         = Node(dep_id, xag_id);
        for (auto const& fanin_id : optimal_cut.at(xag_id)) {
            if (is_input(fanin_id)) {
                continue;
            }
            node.dependencies.emplace_back(xag_to_dep[fanin_id]);
        }
        dep_graph.add_node(node);
    }

    for (auto const& output_id : xag.outputs) {
        if (xag.get_node(output_id).is_input()) {
            continue;
        }
        dep_graph.add_output(NodeID(xag_to_dep[output_id]));
    }

    return dep_graph;
}

std::optional<std::vector<std::vector<bool>>> pebble(SatSolver& solver, size_t const num_pebbles, DepGraph graph) {
    size_t const num_nodes = graph.size();  // number of nodes

    auto make_variables = [](SatSolver& solver, const size_t num_nodes, const size_t k) {
        solver.reset();
        auto p = vector<vector<Variable>>(k);
        for (const size_t i : iota(0UL, k)) {
            p[i] = vector<Variable>(num_nodes);
            for (const size_t j : iota(0UL, num_nodes)) {
                p[i][j] = solver.new_var();
            }
        }
        return p;
    };

    auto pebble_inner = [&graph = graph](
                            SatSolver& solver,
                            const vector<vector<Variable>>& p,
                            const size_t num_nodes,
                            const size_t k,
                            const size_t num_pebbles) {
        // Initial and final clauses
        for (const size_t i : iota(0UL, num_nodes)) {
            // at time 0, no node is pebbled
            solver.add_clause({~Literal(p[0][i])});
            // at time K, all output nodes are pebbled, and all other nodes are not pebbled
            solver.add_clause(
                {graph.is_output(DepGraphNodeID(i)) ? Literal(p[k - 1][i])
                                                    : ~Literal(p[k - 1][i])});
        }

        // Move Clauses
        // if a node is pebbled at time i, then all its dependencies must be pebbled at time i + 1
        // if a node is not pebbled at time i, then all its dependencies must be pebbled at time i
        // (a xor b) + cd = (~a + b + c) (~a + b + d) (a + ~b + c) (a + ~b + d)
        for (const size_t i : iota(0UL, k - 1)) {
            for (const auto& node : graph.get_graph() | std::views::values) {
                for (const auto& dep_id : node.dependencies) {
                    auto const a = Literal(p[i][node.id.get()]);
                    auto const b = Literal(p[i + 1][node.id.get()]);
                    auto const c = Literal(p[i][dep_id.get()]);
                    auto const d = Literal(p[i + 1][dep_id.get()]);
                    solver.add_clause({~a, b, c});
                    solver.add_clause({~a, b, d});
                    solver.add_clause({a, ~b, c});
                    solver.add_clause({a, ~b, d});
                }
            }
        }

        // Cardinality Clauses
        for (const size_t i : iota(0UL, k)) {
            auto p_i = p[i] | std::views::transform([](const Variable& var) { return Literal(var); }) | tl::to<std::vector>();

            solver.add_lte_constraint(p_i, num_pebbles);
        }

        return solver.solve() == Result::sat;
    };

    // binary search to find the minimum K
    vector<vector<Variable>> p;
    size_t left  = 2;
    size_t right = num_nodes * num_nodes * 2;
    size_t k     = right;

    p = make_variables(solver, num_nodes, right);
    if (!pebble_inner(solver, p, num_nodes, right, num_pebbles)) {
        return std::nullopt;
    }

    while (left < right) {
        size_t const mid = (left + right) / 2;
        p                = make_variables(solver, num_nodes, mid);
        if (pebble_inner(solver, p, num_nodes, mid, num_pebbles)) {
            right = mid;
            k     = mid;
        } else {
            left = mid + 1;
        }
    }

    if (k == 0) {
        return std::nullopt;
    }
    spdlog::debug("pebbling: found K = {}", k);
    p = make_variables(solver, num_nodes, k);
    pebble_inner(solver, p, num_nodes, k, num_pebbles);
    auto solution = solver.get_solution();
    if (!solution.has_value()) {
        return std::nullopt;
    }

    std::vector<std::vector<bool>> schedule(k, std::vector<bool>(num_nodes, false));
    for (const size_t i : iota(0UL, k)) {
        for (const size_t j : iota(0UL, num_nodes)) {
            schedule[i][j] = (*solution)[p[i][j]];
        }
    }

    return schedule;
}

size_t sanitize_num_pebbles(size_t const num_pebbles, size_t const num_nodes, size_t const max_deps) {
    if (num_pebbles > num_nodes) {
        spdlog::warn("P = {} is too small, using P = {} instead", num_pebbles, num_nodes);
        return num_nodes;
    }
    if (num_pebbles < max_deps + 1) {
        spdlog::warn("P = {} is too small, using P = {} instead", num_pebbles, max_deps + 1);
        return max_deps + 1;
    }
    return num_pebbles;
}

void test_pebble(const size_t num_pebbles, std::istream& input) {
    auto solver = CaDiCalSolver();

    auto graph = from_deps_file(input);

    spdlog::debug("{}", graph.to_string());

    const size_t num_nodes        = graph.size();  // number of nodes
    const size_t max_deps         = std::ranges::max(graph.get_graph() |
                                                     std::views::values |
                                                     std::views::transform([](const Node& node) {
                                                 return node.dependencies.size();
                                             }));
    const size_t real_num_pebbles = sanitize_num_pebbles(num_pebbles, num_nodes, max_deps);

    spdlog::debug("N = {}, P = {}", num_nodes, real_num_pebbles);

    auto schedule = pebble(solver, real_num_pebbles, graph);

    if (!schedule.has_value()) {
        fmt::println("no solution for P = {}, consider increasing P", real_num_pebbles);
        return;
    }

    fmt::println("solution:");
    for (const size_t i : iota(0UL, schedule->size())) {
        fmt::println("time = {:02} : {}", i, fmt::join((*schedule)[i] | std::views::transform([](bool b) { return b ? "*" : "."; }), ""));
    }
}

}  // namespace qsyn::qcir
