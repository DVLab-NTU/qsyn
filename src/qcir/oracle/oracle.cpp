/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for boolean oracle synthesis ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/oracle/oracle.hpp"

#include <algorithm>
#include <cstddef>
#include <gsl/narrow>
#include <gsl/util>
#include <kitty/dynamic_truth_table.hpp>
#include <queue>
#include <ranges>
#include <tl/enumerate.hpp>
#include <tl/slide.hpp>
#include <tl/to.hpp>
#include <tl/zip.hpp>
#include <vector>

#include "./pebble.hpp"
#include "qcir/oracle/k_lut.hpp"
#include "qcir/oracle/pebble.hpp"
#include "qcir/oracle/xag.hpp"
#include "qcir/qcir_gate.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/sat/sat_solver.hpp"

namespace views = std::views;

namespace {
using namespace qsyn::qcir;

std::optional<QCir> build_qcir(
    XAG const& xag,
    std::map<XAGNodeID, XAGCut> const& optimal_cut,
    DepGraph const& dep_graph,
    std::vector<std::vector<bool>> const& schedule,
    LUT const& lut,
    size_t n_ancilla);

}  // namespace

namespace qsyn::qcir {

std::optional<QCir> synthesize_boolean_oracle(XAG xag, size_t n_ancilla, size_t k) {
    size_t num_outputs           = xag.outputs.size();
    auto const& [optimal_cut, _] = k_lut_partition(xag, k);

    fmt::print("xag:\n");
    for (auto const& xag_node : xag.get_nodes()) {
        if (xag_node.is_valid()) {
            fmt::print("    {}\n", xag_node.to_string());
        }
    }
    fmt::print("xag.outputs: [{}]\n", fmt::join(xag.outputs |
                                                    views::transform([](auto const& id) { return id.get(); }),
                                                ", "));
    fmt::print("xag.output_inverted: [{}]\n", fmt::join(xag.outputs_inverted, ", "));
    fmt::print("optimal cut:\n");
    for (auto const& [xag_id, xag_cut] : optimal_cut) {
        fmt::print("{}: ", xag_id.get());
        for (auto const& id : xag_cut) {
            fmt::print("{}, ", id.get());
        }
        fmt::print("\n");
    }

    if (xag.get_node(xag.outputs.front()).is_const_1()) {
        spdlog::warn("output is constant {}, no need to synthesize oracle",
                     xag.outputs_inverted.front() ? "0" : "1");
        return std::nullopt;
    }

    auto _dep_graph = from_xag_cuts(xag, optimal_cut);
    if (_dep_graph == std::nullopt) {
        spdlog::error("failed to build dependency graph");
        return std::nullopt;
    }
    auto dep_graph = *_dep_graph;
    fmt::print("dependency graph: {}\n", dep_graph.to_string());

    const size_t N        = dep_graph.size();  // number of nodes
    const size_t max_deps = std::ranges::max(dep_graph.get_graph() |
                                             views::values |
                                             views::transform([](DepGraphNode const& node) {
                                                 return node.dependencies.size();
                                             }));
    if (n_ancilla > N - num_outputs) {
        spdlog::warn("n_ancilla = {} is too large, using n_ancilla = {} instead",
                     n_ancilla,
                     N - num_outputs);
        n_ancilla = N - num_outputs;
    }
    if (n_ancilla < max_deps + 1 - num_outputs) {
        spdlog::warn("n_ancilla = {} is too small, using n_ancilla = {} instead",
                     n_ancilla,
                     max_deps + 1 - num_outputs);
        n_ancilla = max_deps + 1 - num_outputs;
    }

    dvlab::sat::CaDiCalSolver solver{};
    auto pebble_result = pebble(solver, n_ancilla + num_outputs, dep_graph);

    if (pebble_result == std::nullopt) {
        spdlog::error("no solution for n_ancilla = {}", n_ancilla);
        return std::nullopt;
    }

    auto schedule = *pebble_result;
    fmt::println("solution:");
    for (const size_t i : views::iota(0UL, schedule.size())) {
        fmt::println("time = {:02} : {}", i, fmt::join((*pebble_result)[i] | views::transform([](bool b) { return b ? "*" : "."; }), ""));
    }

    return build_qcir(xag,
                      optimal_cut,
                      dep_graph,
                      schedule,
                      LUT(k),
                      n_ancilla);
}

}  // namespace qsyn::qcir

namespace {

void rewire_qcir(QCir& qcir, std::map<qsyn::QubitIdType, qsyn::QubitIdType> const& qubit_map) {
    for (auto& gate : qcir.get_gates()) {
        auto new_qubits = gate->get_qubits() | views::transform([&qubit_map](auto const& qubit) {
                              if (qubit_map.contains(qubit._qubit)) {
                                  return QubitInfo{
                                      ._qubit    = qubit_map.at(qubit._qubit),
                                      ._prev     = qubit._prev,
                                      ._next     = qubit._next,
                                      ._isTarget = qubit._isTarget,

                                  };
                              }
                              return qubit;
                          }) |
                          tl::to<std::vector>();
        gate->set_qubits(new_qubits);
    }
}

std::optional<QCir> build_qcir(
    XAG const& xag,
    std::map<XAGNodeID, XAGCut> const& optimal_cut,
    DepGraph const& dep_graph,
    std::vector<std::vector<bool>> const& schedule,
    LUT const& lut,
    size_t n_ancilla) {
    size_t n_inputs  = xag.inputs.size();
    size_t n_outputs = xag.outputs.size();
    size_t n_qubits  = n_inputs + n_outputs + n_ancilla;

    // 0                    ... n_inputs - 1             : inputs
    // n_inputs             ... n_inputs + n_outputs - 1 : outputs
    // n_inputs + n_outputs ... n_qubits - 1             : ancilla qubits
    auto qcir = QCir(n_qubits);

    auto current_qubit_state = std::map<XAGNodeID, qsyn::QubitIdType>();
    for (auto const& [i, input_id] : tl::views::enumerate(xag.inputs)) {
        current_qubit_state[input_id] = gsl::narrow_cast<qsyn::QubitIdType>(i);
        fmt::print("input_id: {}, qubit_id: {}\n", input_id.get(), i);
    }

    auto free_ancilla_qubits = std::queue<qsyn::QubitIdType>();
    for (auto i : views::iota(n_inputs, n_qubits)) {
        free_ancilla_qubits.push(gsl::narrow_cast<qsyn::QubitIdType>(i));
    }

    auto get_free_ancilla_qubit = [&free_ancilla_qubits]() {
        if (free_ancilla_qubits.empty()) {
            // TODO: check for uncompute and compute at the same time
            throw std::runtime_error("no free ancilla qubits");
        }
        auto qubit = free_ancilla_qubits.front();
        free_ancilla_qubits.pop();
        return qubit;
    };

    auto build_one = [&](size_t pebble_id) {
        auto xag_id       = dep_graph.get_node(DepGraphNodeID(pebble_id)).xag_id;
        auto xag_cone_tip = xag.get_node(xag_id);
        if (xag_cone_tip.is_input()) {
            return;
        }

        auto xag_cut              = optimal_cut.at(xag_id);
        auto const qcir_to_concat = lut[xag.calculate_truth_table(xag_cone_tip.get_id(), xag_cut)];

        qsyn::QubitIdType target_qubit{};
        if (current_qubit_state.contains(xag_id)) {
            target_qubit = current_qubit_state[xag_id];
            fmt::print("compupte: xag_id: {}, qubit_id: {}\n", xag_id.get(), current_qubit_state[xag_id]);
        } else {
            target_qubit = get_free_ancilla_qubit();
            fmt::print("uncompupte: xag_id: {}, qubit_id: {}\n", xag_id.get(), target_qubit);
        }

        auto xag_to_new_qubit_id = std::map<XAGNodeID, qsyn::QubitIdType>();
        for (auto const& [i, xag_id] : tl::views::enumerate(xag_cut)) {
            auto xag_node               = xag.get_node(xag_id);
            xag_to_new_qubit_id[xag_id] = gsl::narrow_cast<qsyn::QubitIdType>(i);
        }
        xag_to_new_qubit_id[xag_cone_tip.get_id()] = gsl::narrow_cast<qsyn::QubitIdType>(xag_cut.size());

        auto concat_qubit_map = std::map<qsyn::QubitIdType, qsyn::QubitIdType>();
        for (auto const& xag_id : xag_cut) {
            concat_qubit_map[xag_to_new_qubit_id[xag_id]] = current_qubit_state[xag_id];
        }
        concat_qubit_map[xag_to_new_qubit_id[xag_cone_tip.get_id()]] = target_qubit;

        qcir.concat(qcir_to_concat, concat_qubit_map);

        if (current_qubit_state.contains(xag_id)) {
            current_qubit_state.erase(xag_id);
            free_ancilla_qubits.push(target_qubit);
        } else {
            current_qubit_state[xag_id] = target_qubit;
        }
    };

    for (auto const& pebble_states : tl::views::slide(schedule, 2)) {
        auto const& curr_pebble = pebble_states.front();
        auto const& next_pebble = pebble_states.back();
        auto pebble_uncomputed  = tl::views::zip(curr_pebble, next_pebble) |
                                 views::transform([](auto const& p) {
                                     auto [curr, next] = p;
                                     return curr && !next;
                                 }) |
                                 tl::to<std::vector>();
        auto pebble_computed = tl::views::zip(curr_pebble, next_pebble) |
                               views::transform([](auto const& p) {
                                   auto [curr, next] = p;
                                   return !curr && next;
                               }) |
                               tl::to<std::vector>();

        for (auto const& [pebble_id, is_changed] : tl::views::enumerate(pebble_uncomputed)) {
            if (!is_changed) {
                continue;
            }
            build_one(pebble_id);
        }
        for (auto const& [pebble_id, is_changed] : tl::views::enumerate(pebble_computed)) {
            if (!is_changed) {
                continue;
            }
            build_one(pebble_id);
        }
    }

    auto current_output_qubit = current_qubit_state[xag.outputs.front()];
    auto target_output_qubit  = gsl::narrow_cast<qsyn::QubitIdType>(n_inputs);
    if (current_output_qubit != target_output_qubit) {
        auto rewire_map                  = std::map<qsyn::QubitIdType, qsyn::QubitIdType>();
        rewire_map[current_output_qubit] = target_output_qubit;
        rewire_map[target_output_qubit]  = current_output_qubit;
        rewire_qcir(qcir, rewire_map);
    }

    if (xag.outputs_inverted.front()) {
        qcir.add_gate("x", {target_output_qubit}, {}, true);
    }

    return qcir;
}

}  // namespace
