/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Print functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <cstdlib>
#include <string>

#include "./qcir.hpp"
#include "./qcir_gate.hpp"
#include "./qcir_qubit.hpp"
#include "fmt/core.h"

namespace qsyn::qcir {

/**
 * @brief Print QCir Gates
 */
void QCir::print_gates() {
    if (_dirty)
        update_gate_time();
    std::cout << "Listed by gate ID" << std::endl;
    for (size_t i = 0; i < _qgates.size(); i++) {
        _qgates[i]->print_gate();
    }
}

/**
 * @brief Print Depth of QCir
 *
 */
void QCir::print_depth() {
    std::cout << "Depth       : " << get_depth() << std::endl;
}

/**
 * @brief Print QCir
 */
void QCir::print_qcir() {
    fmt::println("QCir ({} qubits, {} gates)", _qubits.size(), _qgates.size());
}

/**
 * @brief Print QCir Summary
 */
void QCir::print_summary() {
    print_qcir();
    count_gates();
    print_depth();
}

/**
 * @brief Print Qubits
 */
void QCir::print_qubits(spdlog::level::level_enum lvl) {
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
bool QCir::print_gate_info(size_t id, bool show_time) {
    if (get_gate(id) != nullptr) {
        if (show_time && _dirty)
            update_gate_time();
        get_gate(id)->print_gate_info(show_time);
        return true;
    } else {
        std::cerr << "Error: id " << id << " not found!!" << std::endl;
        return false;
    }
}

void QCir::print_qcir_info() {
    std::vector<int> info = count_gates(false, false);
    fmt::println("QCir ({} qubits, {} gates, {} 2-qubits gates, {} T-gates, {} depths)", _qubits.size(), _qgates.size(), info[1], info[2], get_depth());
}

}  // namespace qsyn::qcir
