/****************************************************************************
  PackageName  [ qcir/deancilla ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./deancilla.hpp"

#include <spdlog/spdlog.h>

#include <unordered_set>

#include "qcir/qcir_gate.hpp"
#include "util/sat/sat_solver.hpp"

namespace {

// dependency graph node
using Node = struct Node {
    size_t id{};
    std::vector<Node*> dependencies;
};

// std::pair<std::vector<qsyn::qcir::QubitInfo>, std::vector<qsyn::qcir::QubitInfo>> get_controls_targets(qsyn::qcir::QCir* qcir) {
//     std::vector<qsyn::qcir::QubitInfo> controls;
//     std::vector<qsyn::qcir::QubitInfo> targets;
//     for (auto const& gate : qcir->get_gates()) {
//         auto qubits = gate->get_qubits();
//         for (auto const& qubit : qubits) {
//             if (qubit._isTarget) {
//                 targets.push_back(qubit);
//             } else {
//                 controls.push_back(qubit);
//             }
//         }
//     }
//     return {controls, targets};
// }

}  // namespace

namespace qsyn::qcir {

void deancilla(QCirMgr& /*qcir_mgr*/, size_t /*target_ancilla_count*/, std::vector<QubitIdType> const& /*ancilla_qubit_indexes*/) {
    using namespace dvlab::sat;
    using std::vector, std::views::iota;

    const size_t N  = 6;  // number of nodes
    const size_t K  = 4;  // time of the final state
    auto solver     = CaDiCalSolver();
    auto graph      = vector<Node>(N);
    auto output_ids = std::unordered_set<size_t>{4, 5};

    for (const int i : iota(0, int(N))) {
        graph[i].id = i;
    }

    // add direct dependencies
    graph[2].dependencies.emplace_back(&graph[0]);
    graph[3].dependencies.emplace_back(&graph[1]);
    graph[4].dependencies.emplace_back(&graph[2]);
    graph[4].dependencies.emplace_back(&graph[3]);
    graph[5].dependencies.emplace_back(&graph[0]);

    // add indirect dependencies
    for (const int i : iota(0, int(N))) {
        vector<bool> visited(N, false);
        vector<Node*> s;
        std::unordered_set<Node*> deps;
        visited[i] = true;
        for (auto const& dep : graph[i].dependencies) {
            s.emplace_back(dep);
            deps.insert(dep);
        }
        while (!s.empty()) {
            const auto node = s.back();
            s.pop_back();
            if (visited[node->id]) {
                continue;
            }
            deps.insert(node);
            visited[node->id] = true;
            for (auto const& dep : node->dependencies) {
                s.emplace_back(dep);
            }
        }
        graph[i].dependencies.clear();
        graph[i].dependencies.insert(graph[i].dependencies.end(), deps.begin(), deps.end());
    }

    for (const int i : iota(0, int(N))) {
        spdlog::debug("node {} depends on: ", i);
        for (auto const& dep : graph[i].dependencies) {
            spdlog::debug("{}", dep->id);
        }
    }

    spdlog::debug("creating variables");

    auto p = vector<vector<Variable>>(K);
    for (const int i : iota(0, int(K))) {
        // placeholders for the pebbling variables
        p[i] = vector<Variable>(N, Variable(1));
        for (const int j : iota(0, int(N))) {
            p[i][j] = solver.new_var();
        }
    }

    spdlog::debug("creating clauses");

    for (const int i : iota(0, int(N))) {
        // at time 0, no node is pebbled
        solver.add_clause({~Literal(p[0][i])});
        // at time K, all output nodes are pebbled, and all other nodes are not pebbled
        solver.add_clause({Literal(p[K - 1][i], !output_ids.contains(i))});
    }
}

}  // namespace qsyn::qcir
