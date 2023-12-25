/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./pebble.hpp"

#include <spdlog/spdlog.h>

#include <unordered_set>

#include "util/sat/sat_solver.hpp"

namespace {

// dependency graph node
using Node = struct Node {
    size_t id{};
    std::vector<Node*> dependencies;
};

}  // namespace

namespace qsyn::qcir {

void pebble(QCirMgr& /*qcir_mgr*/, size_t /*target_ancilla_count*/, std::vector<QubitIdType> const& /*ancilla_qubit_indexes*/) {
    using namespace dvlab::sat;
    using std::vector, std::views::iota;

    const size_t N  = 6;  // number of nodes
    const size_t K  = 7;  // time of the final state
    const size_t P  = 4;  // number of pebbles
    auto solver     = CaDiCalSolver();
    auto graph      = vector<Node>(N);
    auto output_ids = std::unordered_set<size_t>{4, 5};

    spdlog::debug("N = {}, K = {}, P = {}", N, K, P);

    for (const size_t i : iota(0UL, N)) {
        graph[i].id = i;
    }
    // add direct dependencies
    graph[2].dependencies.emplace_back(&graph[0]);
    graph[3].dependencies.emplace_back(&graph[1]);
    graph[4].dependencies.emplace_back(&graph[2]);
    graph[4].dependencies.emplace_back(&graph[3]);
    graph[5].dependencies.emplace_back(&graph[0]);

    auto p = vector<vector<Variable>>(K);
    for (const size_t i : iota(0UL, K)) {
        p[i] = vector<Variable>(N);
        for (const std::size_t j : iota(0UL, N)) {
            p[i][j] = solver.new_var();
        }
    }

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

    Result result = solver.solve();

    switch (result) {
        case Result::SAT:
            spdlog::debug("SAT");
            break;
        case Result::UNSAT:
            spdlog::debug("UNSAT");
            return;
        case Result::UNKNOWN:
            spdlog::debug("UNKNOWN");
            return;
    }

    auto _solution = solver.get_solution();
    if (!_solution.has_value()) {
        spdlog::debug("no solution");
        return;
    }
    auto solution = _solution.value();

    spdlog::debug("solution:");
    for (const size_t i : iota(0UL, K)) {
        std::string s = "";
        for (const size_t j : iota(0UL, N)) {
            s += solution[p[i][j]] ? "1" : "0";
        }
        spdlog::debug("time: {} = {}", i, s);
    }
}

}  // namespace qsyn::qcir
