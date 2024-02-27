/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./pebble.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <optional>
#include <ranges>
#include <sstream>
#include <tl/to.hpp>
#include <vector>

#include "util/sat/sat_solver.hpp"

using namespace dvlab::sat;
using std::vector, std::views::iota;
using namespace qsyn::qcir;
using Node   = qsyn::qcir::DepGraphNode;
using NodeID = qsyn::qcir::DepGraphNodeID;

namespace qsyn::qcir {

DepGraph create_dependency_graph(std::istream& ifs) {
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

std::optional<std::vector<std::vector<bool>>> pebble(SatSolver& solver, size_t const P, DepGraph graph) {
    size_t const N = graph.size();  // number of nodes

    auto make_variables = [](SatSolver& solver, const size_t N, const size_t K) {
        solver.reset();
        auto p = vector<vector<Variable>>(K);
        for (const size_t i : iota(0UL, K)) {
            p[i] = vector<Variable>(N);
            for (const std::size_t j : iota(0UL, N)) {
                p[i][j] = solver.new_var();
            }
        }
        return p;
    };

    auto pebble = [&graph = graph](
                      SatSolver& solver,
                      const vector<vector<Variable>>& p,
                      const size_t N,
                      const size_t K,
                      const size_t P) {
        // Initial and final clauses
        for (const size_t i : iota(0UL, N)) {
            // at time 0, no node is pebbled
            solver.add_clause({~Literal(p[0][i])});
            // at time K, all output nodes are pebbled, and all other nodes are not pebbled
            solver.add_clause({Literal(p[K - 1][i], !graph.is_output(DepGraphNodeID(i)))});
        }

        // Move CLauses
        // if a node is pebbled at time i, then all its dependencies must be pebbled at time i + 1
        // if a node is not pebbled at time i, then all its dependencies must be pebbled at time i
        // (a xor b) + cd = (~a + b + c) (~a + b + d) (a + ~b + c) (a + ~b + d)
        for (const size_t i : iota(0UL, K - 1)) {
            for (const auto& node : graph.get_graph() | std::views::values) {
                for (const auto& dep_id : node.dependencies) {
                    auto dep     = graph.get_node(dep_id);
                    auto const a = Literal(p[i][node.id.get()]);
                    auto const b = Literal(p[i + 1][node.id.get()]);
                    auto const c = Literal(p[i][dep.id.get()]);
                    auto const d = Literal(p[i + 1][dep.id.get()]);
                    solver.add_clause({~a, b, c});
                    solver.add_clause({~a, b, d});
                    solver.add_clause({a, ~b, c});
                    solver.add_clause({a, ~b, d});
                }
            }
        }

        // Cardinality Clauses
        for (const size_t i : iota(0UL, K)) {
            auto p_i = p[i] | std::views::transform([](const Variable& var) { return Literal(var); }) | tl::to<std::vector>();
            solver.add_lte_constraint(p_i, P);
        }

        return solver.solve() == Result::SAT;
    };

    // binary search to find the minimum K
    vector<vector<Variable>> p;
    size_t left  = 2;
    size_t right = N * N * 2;
    size_t K     = N;

    spdlog::debug("trying K = {}", right);
    p = make_variables(solver, N, right);
    if (!pebble(solver, p, N, right, P)) {
        return {};
    }

    while (left < right) {
        size_t mid = (left + right) / 2;
        spdlog::debug("trying K = {}", mid);
        p = make_variables(solver, N, mid);
        if (pebble(solver, p, N, mid, P)) {
            right = mid;
            K     = mid;
        } else {
            left = mid + 1;
        }
    }

    if (K == 0) {
        return {};
    }
    p = make_variables(solver, N, K);
    pebble(solver, p, N, K, P);
    auto _solution = solver.get_solution();
    if (!_solution.has_value()) {
        return {};
    }
    auto solution = _solution.value();

    std::vector<std::vector<bool>> schedule(K, std::vector<bool>(N, false));
    for (const size_t i : iota(0UL, K)) {
        for (const size_t j : iota(0UL, N)) {
            schedule[i][j] = solution[p[i][j]];
        }
    }

    return schedule;
}

size_t sanitize_P(size_t const P, size_t const N, size_t const max_deps) {
    if (P > N) {
        spdlog::warn("P = {} is too small, using P = {} instead", P, N);
        return N;
    }
    if (P < max_deps + 1) {
        spdlog::warn("P = {} is too small, using P = {} instead", P, max_deps + 1);
        return max_deps + 1;
    }
    return P;
}

void test_pebble(const size_t _P, std::istream& input) {
    auto solver = CaDiCalSolver();

    auto graph = create_dependency_graph(input);

    const size_t N        = graph.size();  // number of nodes
    const size_t max_deps = std::ranges::max(graph.get_graph() |
                                             std::views::values |
                                             std::views::transform([](const Node& node) { return node.dependencies.size(); }));
    const size_t P        = sanitize_P(_P, N, max_deps);

    spdlog::debug("N = {}, P = {}", N, P);

    auto schedule = pebble(solver, P, graph);

    if (!schedule.has_value()) {
        fmt::println("no solution for P = {}, consider increasing P", P);
        return;
    }

    fmt::println("solution:");
    for (const size_t i : iota(0UL, schedule->size())) {
        fmt::println("time = {:02} : {}", i, fmt::join((*schedule)[i] | std::views::transform([](bool b) { return b ? "*" : "."; }), ""));
    }
}

}  // namespace qsyn::qcir
