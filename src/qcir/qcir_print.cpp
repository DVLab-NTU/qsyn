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
    if (_dirty)
        update_gate_time();
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

    if (gate_ids.empty()) {
        for (auto const* gate : _qgates) {
            gate->print_gate();
            if (print_neighbors) {
                print_predecessors(gate);
                print_successors(gate);
            }
        }
    } else {
        for (auto id : gate_ids) {
            if (get_gate(id) == nullptr) {
                spdlog::error("Gate ID {} not found!!", id);
                continue;
            }
            get_gate(id)->print_gate();
            if (print_neighbors) {
                print_predecessors(get_gate(id));
                print_successors(get_gate(id));
            }
        }
    }
}

/**
 * @brief Print Depth of QCir
 *
 */
void QCir::print_depth() const {
    fmt::println("Depth       : {}", calculate_depth());
}

/**
 * @brief Print QCir
 */
void QCir::print_qcir() const {
    fmt::println("QCir ({} qubits, {} gates)", _qubits.size(), _qgates.size());
}

/**
 * @brief Print Qubits
 */
void QCir::print_circuit_diagram(spdlog::level::level_enum lvl) const {
    if (_dirty)
        update_gate_time();

    for (size_t i = 0; i < _qubits.size(); i++)
        _qubits[i]->print_qubit_line(lvl);
}

/**
 * @brief Print Gate information
 *
 * @param id
 * @param showTime if true, show the time
 */
bool QCir::print_gate_as_diagram(size_t id, bool show_time) const {
    if (get_gate(id) == nullptr) {
        spdlog::error("Gate ID {} not found!!", id);
        return false;
    }

    if (show_time && _dirty)
        update_gate_time();
    get_gate(id)->print_gate_info(show_time);
    return true;
}

void QCir::print_qcir_info() const {
    auto stat = get_gate_statistics();
    fmt::println("QCir ({} qubits, {} gates, {} 2-qubits gates, {} T-gates, {} depths)", _qubits.size(), _qgates.size(), stat.twoqubit, stat.tfamily, calculate_depth());
}

}  // namespace qsyn::qcir
