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
#include <kitty/dynamic_truth_table.hpp>
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
    size_t P                     = n_ancilla + num_outputs;
    auto const& [optimal_cut, _] = k_lut_partition(xag, k);

    auto dep_graph = from_xag_cuts(optimal_cut, xag.outputs);

    const size_t N        = dep_graph.size();  // number of nodes
    const size_t max_deps = std::ranges::max(dep_graph.get_graph() |
                                             views::values |
                                             views::transform([](DepGraphNode const& node) {
                                                 return node.dependencies.size();
                                             }));

    if (P > N) {
        spdlog::warn("n_ancilla = {} is too large, using n_ancilla = {} instead",
                     n_ancilla,
                     N - num_outputs);
        P = N;
    }
    if (P < max_deps + 1) {
        spdlog::warn("n_ancilla = {} is too small, using n_ancilla = {} instead",
                     n_ancilla,
                     max_deps + 1 - num_outputs);
        P = max_deps + 1;
    }

    dvlab::sat::CaDiCalSolver solver{};
    auto pebble_result = pebble(solver, P, dep_graph);

    if (pebble_result == std::nullopt) {
        spdlog::error("no solution for P = {}, consider increasing P", P);
        return std::nullopt;
    }

    auto schedule = *pebble_result;
    fmt::println("solution:");
    for (const size_t i : views::iota(0UL, schedule.size())) {
        fmt::println("time = {:02} : {}", i, fmt::join((*pebble_result)[i] | views::transform([](bool b) { return b ? "*" : "."; }), ""));
    }

    auto lut = LUT(k);

    return build_qcir(xag,
                      optimal_cut,
                      dep_graph,
                      schedule,
                      lut,
                      P - num_outputs);
}

}  // namespace qsyn::qcir

namespace {

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

    auto pebble_to_qubit = std::vector<qsyn::QubitIdType>(n_qubits, 0);
    {
        auto qcir_qubit   = gsl::narrow_cast<qsyn::QubitIdType>(n_inputs + n_outputs);
        auto output_qubit = gsl::narrow_cast<qsyn::QubitIdType>(n_inputs);
        for (auto const& [i, is_pebbled] : tl::views::enumerate(schedule.back())) {
            if (is_pebbled) {
                pebble_to_qubit[i] = output_qubit++;
            } else {
                pebble_to_qubit[i] = qcir_qubit++;
            }
        }
    }

    auto current_ancilla_state = std::map<XAGNodeID, qsyn::QubitIdType>();
    for (auto const& [i, input_id] : tl::views::enumerate(xag.inputs)) {
        current_ancilla_state[input_id] = gsl::narrow_cast<qsyn::QubitIdType>(i);
    }

    for (auto const& pebble_states : tl::views::slide(schedule, 2)) {
        auto const& curr_pebble = pebble_states.front();
        auto const& next_pebble = pebble_states.back();
        auto pebble_changed     = tl::views::zip(curr_pebble, next_pebble) |
                              views::transform([](auto const& p) {
                                  auto [curr, next] = p;
                                  return curr != next;
                              }) |
                              tl::to<std::vector>();
        for (auto const& [pebble_id, is_pebbled] : tl::views::enumerate(pebble_changed)) {
            auto target_qubit = pebble_to_qubit[pebble_id];
            auto dep_node     = dep_graph.get_node(DepGraphNodeID(pebble_id));
            auto dependencies = dep_node.dependencies;

            auto xag_cone_tip         = xag.get_node(dep_node.xag_id);
            auto xag_cut              = optimal_cut.at(dep_node.xag_id);
            auto const qcir_to_concat = lut[xag.calculate_truth_table(xag_cone_tip.get_id(), xag_cut)];

            if (xag_cone_tip.is_input()) {
                continue;
            }

            auto xag_to_new_qubit_id = std::map<XAGNodeID, qsyn::QubitIdType>();
            for (auto const& [i, xag_id] : tl::views::enumerate(xag_cut)) {
                auto xag_node               = xag.get_node(xag_id);
                xag_to_new_qubit_id[xag_id] = gsl::narrow_cast<qsyn::QubitIdType>(i);
            }
            xag_to_new_qubit_id[xag_cone_tip.get_id()] = gsl::narrow_cast<qsyn::QubitIdType>(xag_cut.size());

            auto concat_qubit_map = std::map<qsyn::QubitIdType, qsyn::QubitIdType>();
            for (auto const& xag_id : xag_cut) {
                concat_qubit_map[xag_to_new_qubit_id[xag_id]] = current_ancilla_state[xag_id];
            }
            concat_qubit_map[xag_to_new_qubit_id[xag_cone_tip.get_id()]] = target_qubit;

            qcir.concat(qcir_to_concat, concat_qubit_map);

            if (current_ancilla_state.contains(dep_node.xag_id)) {
                current_ancilla_state.erase(dep_node.xag_id);
            } else {
                current_ancilla_state[dep_node.xag_id] = target_qubit;
            }
        }
    }

    return qcir;
}

}  // namespace
