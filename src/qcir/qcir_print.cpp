/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Print functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <cstdlib>
#include <ranges>
#include <string>

#include "./qcir.hpp"
#include "./qcir_gate.hpp"
#include "./qcir_qubit.hpp"
#include "fmt/core.h"

namespace qsyn::qcir {

/**
 * @brief Print QCir Gates
 */
void QCir::print_gates(bool print_neighbors, std::span<size_t> gate_ids) const {
    fmt::println("Listed by gate ID");

    auto const print_predecessors = [this](size_t gate_id) {
        auto const get_predecessor_gate_id = [](std::optional<size_t> pred) -> std::string {
            return pred.has_value() ? fmt::format("{}", *pred) : "Start";
        };
        auto const predecessors = get_predecessors(gate_id);
        fmt::println("- Predecessors: {}", fmt::join(predecessors | std::views::transform(get_predecessor_gate_id), ", "));
    };

    auto const print_successors = [this](size_t gate_id) {
        auto const get_successor_gate_id = [](std::optional<size_t> succ) -> std::string {
            return succ.has_value() ? fmt::format("{}", *succ) : "End";
        };
        auto const successors = get_successors(gate_id);
        fmt::println("- Successors  : {}", fmt::join(successors | std::views::transform(get_successor_gate_id), ", "));
    };

    auto const times = calculate_gate_times();

    auto const id_print_width =
        std::to_string(std::ranges::max(_id_to_gates | std::views::keys)).size();
    auto const repr_print_width =
        std::ranges::max(_id_to_gates | std::views::values | std::views::transform([](auto const& gate) { return gate->get_operation().get_repr().size(); }));

    auto const time_print_width =
        std::to_string(std::ranges::max(times | std::views::values)).size();
    auto const print_one_gate([&](size_t id) {
        auto const gate   = get_gate(id);
        auto const qubits = gate->get_qubits();
        fmt::println(
            "{0:>{1}} (t={2:>{3}}): {4:<{5}} {6:>5}",
            id, id_print_width,
            times.at(id), time_print_width,
            gate->get_operation().get_repr(), repr_print_width,
            fmt::join(qubits |
                          std::views::transform([](QubitIdType qid) {
                              return fmt::format("q[{}]", qid);
                          }),
                      ", "));
        // gate->print_gate(times.at(id));
        if (print_neighbors) {
            print_predecessors(id);
            print_successors(id);
        }
    });

    if (gate_ids.empty()) {
        for (auto const& [id, gate] : _id_to_gates) {
            auto const qubits = gate->get_qubits();
            print_one_gate(id);
            if (print_neighbors) {
                print_predecessors(id);
                print_successors(id);
            }
        }
    } else {
        for (auto const id : gate_ids) {
            auto const gate = get_gate(id);
            if (gate == nullptr) {
                spdlog::error("Gate ID {} not found!!", id);
                continue;
            }
            print_one_gate(id);
            if (print_neighbors) {
                print_predecessors(id);
                print_successors(id);
            }
        }
    }
}

/**
 * @brief Print QCir
 */
void QCir::print_qcir() const {
    fmt::println("QCir ({} qubits, {} gates)", get_num_qubits(), get_num_gates());
}

/**
 * @brief Print Qubits
 */
void QCir::print_circuit_diagram(spdlog::level::level_enum lvl) const {
    auto const times = calculate_gate_times();

    for (auto const* qubit : _qubits) {
        QCirGate* current = qubit->get_first();
        size_t last_time  = 1;
        std::string line  = fmt::format("Q{:>2}  ", qubit->get_id());
        while (current != nullptr) {
            DVLAB_ASSERT(last_time <= times.at(current->get_id()), "Gate time should not be smaller than last time!!");
            line += fmt::format(
                "{}-{:>2}({:>2})-",
                std::string(8 * (times.at(current->get_id()) - last_time), '-'),
                current->get_type_str().substr(0, 2),
                current->get_id());

            last_time = times.at(current->get_id()) + 1;

            for (size_t i = 0; i < current->get_num_qubits(); ++i) {
                if (current->get_qubit(i) == qubit->get_id()) {
                    current = get_gate(get_successor(current->get_id(), i));
                    break;
                }
            }
        }

        spdlog::log(lvl, "{}", line);
    }
}

/**
 * @brief Print Gate information
 *
 * @param id
 * @param showTime if true, show the time
 */
// bool QCir::print_gate_as_diagram(size_t id, bool show_time) const {
//     auto const gate = get_gate(id);
//     if (gate == nullptr) {
//         spdlog::error("Gate ID {} not found!!", id);
//         return false;
//     }

//     gate->print_gate_info();
//     if (show_time) {
//         fmt::println("Execute at t= {}", calculate_gate_times().at(id));
//     }
//     return true;
// }

void QCir::print_qcir_info() const {
    auto stat = get_gate_statistics();
    fmt::println("QCir ({} qubits, {} gates, {} 2-qubits gates, {} T-gates, {} depths)", get_num_qubits(), get_num_gates(), stat.twoqubit, stat.tfamily, calculate_depth());
}

}  // namespace qsyn::qcir
