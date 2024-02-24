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
#include <sstream>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>
#include <tl/zip.hpp>
#include <utility>
#include <vector>

#include "qcir/oracle/k_lut.hpp"
#include "qcir/oracle/pebble.hpp"
#include "qcir/oracle/xag.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/phase.hpp"
#include "util/sat/sat_solver.hpp"

namespace views = std::views;

namespace {

using namespace qsyn::qcir;
using qsyn::QubitIdType;

QCir* xag_to_qcir(XAG& xag, XAGNodeID top, XAGCut cut) {
    QCir* qcir = new QCir();
    if (cut.size() == 1) {
        qcir->add_qubits(1);
        return qcir;
    }
    qcir->add_qubits(cut.size() + 1);

    fmt::print("top: {}\n", xag.get_node(top)->to_string());
    fmt::print("cut: {}\n", fmt::join(cut | views::transform([](auto const id) { return id.get(); }), " "));

    std::map<QubitIdType, XAGNodeID> qb_to_xag;
    std::map<XAGNodeID, QubitIdType> xag_to_qb;
    auto cone_node_ids = xag.get_cone_node_ids(top, cut);

    fmt::print("cone: {}\n", fmt::join(cone_node_ids | views::transform([](auto const id) { return id.get(); }), " "));

    auto free_qubits = qcir->get_qubits() |
                       views::transform([](auto& qubit) { return qubit->get_id(); }) |
                       tl::to<std::set>();
    auto unmapped_nodes = std::set<XAGNodeID>(cone_node_ids.begin(), cone_node_ids.end());

    // add_gate arguments
    std::vector<std::tuple<std::string, qsyn::QubitIdList, dvlab::Phase, bool>> operations;

    auto const first_id            = *cone_node_ids.begin();
    auto const first_node          = xag.get_node(first_id);
    QubitIdType const output_qubit = free_qubits.extract(*free_qubits.begin()).value();

    auto cx = [&operations](QubitIdType const& control, QubitIdType const& target) {
        operations.emplace_back("cpx", qsyn::QubitIdList{control, target}, dvlab::Phase(1), true);
    };
    auto ccx = [&operations](QubitIdType const& control1, QubitIdType const& control2, QubitIdType const& target) {
        operations.emplace_back("ccpx", qsyn::QubitIdList{control1, control2, target}, dvlab::Phase(1), true);
    };

    if (first_node->get_type() == XAGNodeType::XOR) {
        auto const q1 = free_qubits.extract(*free_qubits.begin()).value();
        auto const q2 = free_qubits.extract(*free_qubits.begin()).value();
        cx(q1, output_qubit);
        cx(q2, output_qubit);
        qb_to_xag[q1] = xag.get_node(first_node->fanins[0])->get_id();
        qb_to_xag[q2] = xag.get_node(first_node->fanins[1])->get_id();
    } else if (first_node->get_type() == XAGNodeType::AND) {
        auto const q1 = free_qubits.extract(*free_qubits.begin()).value();
        auto const q2 = free_qubits.extract(*free_qubits.begin()).value();
        ccx(q1, q2, output_qubit);
        qb_to_xag[q1] = xag.get_node(first_node->fanins[0])->get_id();
        qb_to_xag[q2] = xag.get_node(first_node->fanins[1])->get_id();
    }

    return qcir;
}

}  // namespace

namespace qsyn::qcir {

void synthesize_boolean_oracle(std::istream& xag_input, QCir* /*qcir*/, size_t n_ancilla, size_t k) {
    XAG xag                      = from_xaag(xag_input);
    size_t const P               = n_ancilla + xag.outputs.size();
    auto const& [optimal_cut, _] = k_lut_partition(xag, k);
    fmt::print("P = {}, k = {}\n", P, k);

    // renumber the nodes in the optimal cut
    std::map<XAGNodeID, DepGrapgNodeID> xag_to_deps;
    std::map<DepGrapgNodeID, XAGNodeID> deps_to_xag;
    std::vector<XAGNodeID> optimal_cone_tips = optimal_cut |
                                               views::keys |
                                               tl::to<std::vector>();
    std::sort(optimal_cone_tips.begin(), optimal_cone_tips.end());
    std::stringstream dep_graph_ss;
    for (auto const& [i, node_id] : tl::views::enumerate(optimal_cone_tips)) {
        xag_to_deps[node_id]           = DepGrapgNodeID(i);
        deps_to_xag[DepGrapgNodeID(i)] = node_id;
    }
    dep_graph_ss << fmt::format("{}", fmt::join(xag.outputs | views::transform([&](auto& xag_id) { return xag_to_deps.at(xag_id).get(); }), " ")) << '\n';
    for (auto i : views::iota(0ul, optimal_cone_tips.size())) {
        auto const dep_id    = DepGrapgNodeID(i);
        auto const xag_id    = deps_to_xag.at(dep_id);
        auto const& xag_node = xag.get_node(xag_id);
        dep_graph_ss << fmt::format("{}", i);
        if (xag_node->get_type() == XAGNodeType::INPUT) {
            dep_graph_ss << '\n';
            continue;
        }
        for (auto const& deps : optimal_cut.at(xag_id)) {
            dep_graph_ss << ' ' << fmt::format("{}", xag_to_deps.at(deps).get());
        }
        dep_graph_ss << '\n';
    }

    std::vector<DepGraphNode> graph;
    std::set<size_t> output_ids;
    create_dependency_graph(dep_graph_ss, graph, output_ids);

    const size_t N        = graph.size();  // number of nodes
    const size_t max_deps = std::ranges::max(graph | std::views::transform([](DepGraphNode const& node) { return node.dependencies.size(); }));
    const size_t _P       = sanitize_P(P, N, max_deps);

    dvlab::sat::CaDiCalSolver solver{};
    auto schedule = pebble(solver, _P, graph, output_ids);

    fmt::println("solution:");
    for (const size_t i : views::iota(0UL, schedule->size())) {
        fmt::println("time = {:02} : {}", i, fmt::join((*schedule)[i] | std::views::transform([](bool b) { return b ? "*" : "."; }), ""));
    }

    xag_to_qcir(xag, XAGNodeID(17), optimal_cut.at(XAGNodeID(17)));
}

}  // namespace qsyn::qcir
