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
#include <ranges>
#include <unordered_set>

#include "util/sat/sat_solver.hpp"

using namespace dvlab::sat;
using std::vector, std::views::iota;

namespace {

// dependency graph node
using Node = struct Node {
    size_t id{};
    std::vector<Node*> dependencies;
};

}  // namespace

namespace qsyn::qcir {

void test_pebble(const size_t _P) {
    auto solver     = CaDiCalSolver();
    auto graph      = vector<Node>(6);
    auto output_ids = std::unordered_set<size_t>{4, 5};
    const size_t N  = graph.size();  // number of nodes

    for (const size_t i : iota(0UL, N)) {
        graph[i].id = i;
    }
    // add direct dependencies
    graph[2].dependencies.emplace_back(&graph[0]);
    graph[3].dependencies.emplace_back(&graph[1]);
    graph[4].dependencies.emplace_back(&graph[2]);
    graph[4].dependencies.emplace_back(&graph[3]);
    graph[5].dependencies.emplace_back(&graph[0]);

    const size_t max_deps = std::max_element(graph.begin(), graph.end(), [](const Node& a, const Node& b) { return a.dependencies.size() < b.dependencies.size(); })
                                ->dependencies.size();

    const size_t P = std::max(std::min(_P, N), max_deps + 1);
    if (P < _P) {
        spdlog::warn("P = {} is too large, using P = {} instead", _P, P);
    }
    if (P > _P) {
        spdlog::warn("P = {} is too small, using P = {} instead", _P, max_deps);
    }

    spdlog::debug("N = {}, P = {}", N, P);

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

    auto pebble = [&output_ids, &graph](SatSolver& solver, const vector<vector<Variable>>& p, const size_t N, const size_t K, const size_t P) {
        // Initial and final clauses
        for (const size_t i : iota(0UL, N)) {
            // at time 0, no node is pebbled
            solver.add_clause({~Literal(p[0][i])});
            // at time K, all output nodes are pebbled, and all other nodes are not pebbled
            solver.add_clause({Literal(p[K - 1][i], !output_ids.contains(i))});
        }

        // Move CLauses
        // if a node is pebbled at time i, then all its dependencies must be pebbled at time i + 1
        // if a node is not pebbled at time i, then all its dependencies must be pebbled at time i
        // (a xor b) + cd = (~a + b + c) (~a + b + d) (a + ~b + c) (a + ~b + d)
        for (const size_t i : iota(0UL, K - 1)) {
            for (const auto& node : graph) {
                for (const auto& dep : node.dependencies) {
                    auto const a = Literal(p[i][node.id]);
                    auto const b = Literal(p[i + 1][node.id]);
                    auto const c = Literal(p[i][dep->id]);
                    auto const d = Literal(p[i + 1][dep->id]);
                    solver.add_clause({~a, b, c});
                    solver.add_clause({~a, b, d});
                    solver.add_clause({a, ~b, c});
                    solver.add_clause({a, ~b, d});
                }
            }
        }

        // Cardinality Clauses
        std::vector<Literal> literals;
        for (const size_t i : iota(0UL, K)) {
            literals.clear();
            // sum(p_ij) <= P -> sum(~p_ij) >= N - P
            for (const size_t j : iota(0UL, N)) {
                literals.emplace_back(p[i][j], true);
            }
            solver.add_gte_constraint(literals, N - P);
        }

        return solver.solve() == Result::SAT;
    };

    // binary search to find the minimum K
    vector<vector<Variable>> p;
    size_t left  = 2;
    size_t right = N * N * 2;
    size_t K     = 0;
    while (left < right) {
        size_t mid = (left + right) / 2;
        spdlog::debug("left = {}, right = {}, mid = {}", left, right, mid);
        p = make_variables(solver, N, mid);
        if (pebble(solver, p, N, mid, P)) {
            right = mid;
            K     = mid;
        } else {
            left = mid + 1;
        }
        spdlog::debug("p.size() = {}", p.size());
    }

    if (K == 0) {
        fmt::println("no solution for P = {}, consider increasing P", P);
        return;
    }
    p = make_variables(solver, N, K);
    pebble(solver, p, N, K, P);
    auto _solution = solver.get_solution();
    if (!_solution.has_value()) {
        fmt::println("no solution for P = {}, consider increasing P", P);
        return;
    }
    auto solution = _solution.value();

    fmt::println("solution:");
    for (const size_t i : iota(0UL, K)) {
        std::string s = "";
        for (const size_t j : iota(0UL, N)) {
            s += solution[p[i][j]] ? "1" : "0";
        }
        fmt::println("time = {}: {}", i, s);
    }
}

}  // namespace qsyn::qcir
