/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_gate.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <ranges>
#include <string>
#include <type_traits>

#include "qcir/gate_type.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/util.hpp"

namespace qsyn::qcir {

size_t SINGLE_DELAY   = 1;
size_t DOUBLE_DELAY   = 2;
size_t SWAP_DELAY     = 6;
size_t MULTIPLE_DELAY = 5;

// void QCirGate::print_gate_info() const {
//     auto type_str = get_type_str();
//     if (type_str.starts_with("mc") || type_str.starts_with("cc")) type_str = type_str.substr(2);
//     if (type_str.starts_with("c")) type_str = type_str.substr(1);
//     auto pos = type_str.find("dg");
//     std::for_each(type_str.begin(), pos == std::string::npos ? type_str.end() : dvlab::iterator::next(type_str.begin(), pos), [](char& c) { c = dvlab::str::toupper(c); });
//     auto show_phase = type_str[0] == 'P' || type_str[0] == 'R';
//     _print_single_qubit_or_controlled_gate(type_str, show_phase);
// }

void QCirGate::set_operation(Operation const& op) {
    if (op.get_num_qubits() != _operands.size()) {
        spdlog::error("Operation {} cannot be set with {} qubits!", op.get_type(), _operands.size());
        return;
    }
    /* temporary : a bunch of case-by-case handling of syncing _operation and legacy members */
    // if (op.get_type() == "s") {
    //     _rotation_category = GateRotationCategory::pz;
    //     _phase             = Phase(1, 2);
    // }
    // if (op.get_type() == "sdg") {
    //     _rotation_category = GateRotationCategory::pz;
    //     _phase             = Phase(-1, 2);
    // }

    // should be legacy gate type
    auto legacy        = op.get_underlying<LegacyGateType>();
    _rotation_category = legacy.get_rotation_category();
    _phase             = legacy.get_phase();

    _operation = op;
}

/**
 * @brief Get delay of gate
 *
 * @return size_t
 */
size_t QCirGate::get_delay() const {
    if (get_operation() == SwapGate{})
        return SWAP_DELAY;
    if (_qubits.size() == 1)
        return SINGLE_DELAY;
    if (_qubits.size() == 2)
        return DOUBLE_DELAY;
    return MULTIPLE_DELAY;
}

/**
 * @brief Print Gate brief information
 */
void QCirGate::print_gate(std::optional<size_t> time) const {
    fmt::print("ID:{:>4} ({:>3})      ", _id, get_type_str());
    if (time.has_value())
        fmt::print("Time: {:>4}     ", time.value());

    fmt::print("Qubit: {:>3} ", fmt::join(get_qubits(), " "));
    auto is_special_phase   = get_phase().denominator() == 1 || get_phase().denominator() == 2 || get_phase() == Phase(1, 4) || get_phase() == Phase(-1, 4);
    auto is_p_type_rotation = get_rotation_category() == GateRotationCategory::py || get_rotation_category() == GateRotationCategory::px || get_rotation_category() == GateRotationCategory::pz;

    if (!(is_special_phase && is_p_type_rotation) && !is_fixed_phase_gate(get_rotation_category())) {
        fmt::print("      Phase: {}", get_phase().get_print_string());
    }
    fmt::println("");
}

/**
 * @brief Print multiple-qubit gate.
 *
 * @param gtype
 * @param showRotate
 * @param showTime
 */
// void QCirGate::_print_single_qubit_or_controlled_gate(std::string gtype, bool show_rotation) const {
//     if (_qubits.size() > 1 && gtype.size() % 2 == 0) {
//         gtype = " " + gtype;
//     }
//     auto const max_qubit = std::to_string(std::ranges::max(get_operands()));

//     std::vector<std::string> prevs;
//     for (size_t i = 0; i < _qubits.size(); i++) {
//         if (get_qubits()[i]._prev == nullptr)
//             prevs.emplace_back("Start");
//         else
//             prevs.emplace_back("G" + std::to_string(get_qubits()[i]._prev->get_id()));
//     }
//     auto const max_prev = *max_element(prevs.begin(), prevs.end(), [](std::string const& a, std::string const& b) {
//         return a.size() < b.size();
//     });

//     for (size_t i = 0; i < _qubits.size(); i++) {
//         auto const info        = get_qubits()[i];
//         auto const operand     = get_operand(i);
//         std::string qubit_info = "Q";
//         for (size_t j = 0; j < max_qubit.size() - std::to_string(operand).size(); j++)
//             qubit_info += " ";
//         qubit_info += std::to_string(operand);
//         auto const prev_info = (info._prev == nullptr) ? "Start" : ("G" + std::to_string(info._prev->get_id()));
//         auto const next_info = (info._next == nullptr) ? "End" : ("G" + std::to_string(info._next->get_id()));
//         if (i == _qubits.size() - 1) {
//             fmt::println("{0: ^{1}} ┌─{2:─^{3}}─┐ ", "", max_qubit.size() + max_prev.size() + 3, _qubits.size() > 1 ? "┴" : "", gtype.size());
//             fmt::println("{0} {1:<{4}} ─┤ {3} ├─ {2}", qubit_info, prev_info, next_info, gtype, max_prev.size());
//             fmt::println("{0: ^{1}} └─{0:─^{2}}─┘ ", "", max_qubit.size() + max_prev.size() + 3, gtype.size());
//         } else {
//             fmt::println("{0} {1:<{5}} ───{3:─^{4}}─── {2}", qubit_info, prev_info, next_info, "●", gtype.size(), max_prev.size());
//         }
//     }
//     if (show_rotation)
//         fmt::println("Rotate Phase: {0}", _phase);
// }

void QCirGate::adjoint() {
    _operation = qsyn::qcir::adjoint(_operation);
}

}  // namespace qsyn::qcir
