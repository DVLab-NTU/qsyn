/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for boolean oracle synthesis ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/oracle/oracle.hpp"

#include <algorithm>
#include <ranges>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>
#include <tl/zip.hpp>
#include <vector>

#include "./pebble.hpp"
#include "qcir/oracle/k_lut.hpp"
#include "qcir/oracle/pebble.hpp"
#include "qcir/oracle/xag.hpp"
#include "util/sat/sat_solver.hpp"

namespace views = std::views;

namespace qsyn::qcir {

std::optional<QCir> synthesize_boolean_oracle(XAG xag, size_t n_ancilla, size_t k) {
    size_t const P               = n_ancilla + xag.outputs.size();
    auto const& [optimal_cut, _] = k_lut_partition(xag, k);
    fmt::print("P = {}, k = {}\n", P, k);

    auto dep_graph = from_xag_graph(xag, optimal_cut);

    const size_t N        = dep_graph.size();  // number of nodes
    const size_t max_deps = std::ranges::max(dep_graph.get_graph() |
                                             views::values |
                                             views::transform([](DepGraphNode const& node) {
                                                 return node.dependencies.size();
                                             }));
    const size_t _P       = sanitize_P(P, N, max_deps);

    dvlab::sat::CaDiCalSolver solver{};
    auto schedule = pebble(solver, _P, dep_graph);

    fmt::println("solution:");
    for (const size_t i : views::iota(0UL, schedule->size())) {
        fmt::println("time = {:02} : {}", i, fmt::join((*schedule)[i] | views::transform([](bool b) { return b ? "*" : "."; }), ""));
    }

    // TODO: actually synthesize the oracle
    return std::nullopt;
}

}  // namespace qsyn::qcir
