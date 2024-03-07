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

    auto const print_predecessors = [](QCirGate const* const gate) {
        auto const get_predecessor_gate_id = [](QubitInfo const& qinfo) -> std::string {
            if (qinfo._prev == nullptr)
                return "Begin";
            return fmt::format("{}", qinfo._prev->get_id());
        };
        fmt::println("- Predecessors: {}", fmt::join(gate->get_qubits() | std::views::transform(get_predecessor_gate_id), ", "));
    };

    auto const print_successors = [](QCirGate const* const gate) {
        auto const get_successor_gate_id = [](QubitInfo const& qinfo) -> std::string {
            if (qinfo._next == nullptr)
                return "End";
            return fmt::format("{}", qinfo._next->get_id());
        };
        fmt::println("- Successors  : {}", fmt::join(gate->get_qubits() | std::views::transform(get_successor_gate_id), ", "));
    };

    auto const times = calculate_gate_times();

    if (gate_ids.empty()) {
        for (auto const* gate : _id_to_gates | std::views::values | std::views::transform([](auto const& p) { return p.get(); })) {
            gate->print_gate(times.at(gate->get_id()));
            if (print_neighbors) {
                print_predecessors(gate);
                print_successors(gate);
            }
        }
    } else {
        for (auto id : gate_ids) {
            auto const gate = get_gate(id);
            if (gate == nullptr) {
                spdlog::error("Gate ID {} not found!!", id);
                continue;
            }
            gate->print_gate(times.at(gate->get_id()));
            if (print_neighbors) {
                print_predecessors(get_gate(id));
                print_successors(get_gate(id));
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
            current   = current->get_qubit(qubit->get_id())._next;
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
bool QCir::print_gate_as_diagram(size_t id, bool show_time) const {
    auto const gate = get_gate(id);
    if (gate == nullptr) {
        spdlog::error("Gate ID {} not found!!", id);
        return false;
    }

    gate->print_gate_info();
    if (show_time) {
        fmt::println("Execute at t= {}", calculate_gate_times().at(id));
    }
    return true;
}

void QCir::print_qcir_info() const {
    auto stat = get_gate_statistics();
    fmt::println("QCir ({} qubits, {} gates, {} 2-qubits gates, {} T-gates, {} depths)", get_num_qubits(), get_num_gates(), stat.twoqubit, stat.tfamily, calculate_depth());
}

}  // namespace qsyn::qcir
