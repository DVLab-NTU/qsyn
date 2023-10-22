/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_gate.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <iomanip>
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

std::ostream& operator<<(std::ostream& stream, GateRotationCategory const& type) {
    return stream << static_cast<typename std::underlying_type<GateRotationCategory>::type>(type);
}

std::string QCirGate::get_type_str() const {
    return gate_type_to_str(_rotation_category, get_num_qubits(), _phase);
}

void QCirGate::print_gate_info(bool show_time) const {
    auto type_str = get_type_str();
    if (type_str.starts_with("mc") || type_str.starts_with("cc")) type_str = type_str.substr(2);
    if (type_str.starts_with("c")) type_str = type_str.substr(1);
    auto pos = type_str.find("dg");
    std::for_each(type_str.begin(), pos == std::string::npos ? type_str.end() : dvlab::iterator::next(type_str.begin(), pos), [](char& c) { c = dvlab::str::toupper(c); });
    auto show_phase = type_str[0] == 'P' || type_str[0] == 'R';
    _print_single_qubit_or_controlled_gate(type_str, show_phase, show_time);
}

void QCirGate::set_rotation_category(GateRotationCategory type) {
    _rotation_category = type;
    if (is_fixed_phase_gate(type)) {
        _phase = get_fixed_phase(type);
    }
}
void QCirGate::set_phase(dvlab::Phase p) {
    if (is_fixed_phase_gate(_rotation_category) && p != get_fixed_phase(_rotation_category)) {
        spdlog::error("Gate type {} cannot be set with phase {}!", get_type_str(), p);
        return;
    }
    _phase = p;
}

/**
 * @brief Get delay of gate
 *
 * @return size_t
 */
size_t QCirGate::get_delay() const {
    if (is_swap())
        return SWAP_DELAY;
    if (_qubits.size() == 1)
        return SINGLE_DELAY;
    else if (_qubits.size() == 2)
        return DOUBLE_DELAY;
    else
        return MULTIPLE_DELAY;
}

/**
 * @brief Get Qubit.
 *
 * @param qubit
 * @return BitInfo
 */
QubitInfo QCirGate::get_qubit(QubitIdType qubit) const {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]._qubit == qubit)
            return _qubits[i];
    }
    std::cerr << "Not Found" << std::endl;
    return _qubits[0];
}

/**
 * @brief Add qubit to a gate
 *
 * @param qubit
 * @param isTarget
 */
void QCirGate::add_qubit(QubitIdType qubit, bool is_target) {
    auto const temp = QubitInfo{._qubit = qubit, ._prev = nullptr, ._next = nullptr, ._isTarget = is_target};
    // _qubits.emplace_back(temp);
    if (is_target)
        _qubits.emplace_back(temp);
    else
        _qubits.emplace(_qubits.begin(), temp);
}

/**
 * @brief Set the bit of target
 *
 * @param qubit
 */
void QCirGate::set_target_qubit(QubitIdType qubit) {
    _qubits[_qubits.size() - 1]._qubit = qubit;
}

/**
 * @brief Set parent to the gate on qubit.
 *
 * @param qubit
 * @param p
 */
void QCirGate::set_parent(QubitIdType qubit, QCirGate* p) {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]._qubit == qubit) {
            _qubits[i]._prev = p;
            break;
        }
    }
}

/**
 * @brief Add dummy child c to gate
 *
 * @param c
 */
void QCirGate::add_dummy_child(QCirGate* c) {
    _qubits.push_back({._qubit = 0, ._prev = nullptr, ._next = c, ._isTarget = false});
}

/**
 * @brief Set child to gate on qubit.
 *
 * @param qubit
 * @param c
 */
void QCirGate::set_child(QubitIdType qubit, QCirGate* c) {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]._qubit == qubit) {
            _qubits[i]._next = c;
            break;
        }
    }
}

/**
 * @brief Print Gate brief information
 */
void QCirGate::print_gate() const {
    std::cout << "ID:" << std::right << std::setw(4) << _id;
    std::cout << " (" << std::right << std::setw(3) << get_type_str() << ") ";
    std::cout << "     Time: " << std::right << std::setw(4) << _time << "     Qubit: ";
    for (size_t i = 0; i < _qubits.size(); i++) {
        std::cout << std::right << std::setw(3) << _qubits[i]._qubit << " ";
    }
    if ((get_rotation_category() == GateRotationCategory::pz || get_rotation_category() == GateRotationCategory::rx || get_rotation_category() == GateRotationCategory::ry || get_rotation_category() == GateRotationCategory::rz) &&
        get_phase().denominator() != 1 && get_phase().denominator() != 2 && get_phase().denominator() != 4) {
        std::cout << "      Phase: " << std::right << std::setw(4) << get_phase() << " ";
    }
    std::cout << std::endl;
}

/**
 * @brief Print multiple-qubit gate.
 *
 * @param gtype
 * @param showRotate
 * @param showTime
 */
void QCirGate::_print_single_qubit_or_controlled_gate(std::string gtype, bool show_rotation, bool show_time) const {
    if (_qubits.size() > 1 && gtype.size() % 2 == 0) {
        gtype = " " + gtype;
    }
    auto const max_qubit = std::to_string(max_element(_qubits.begin(), _qubits.end(), [](QubitInfo const a, QubitInfo const b) {
                                              return a._qubit < b._qubit;
                                          })->_qubit);

    std::vector<std::string> prevs;
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (get_qubits()[i]._prev == nullptr)
            prevs.emplace_back("Start");
        else
            prevs.emplace_back("G" + std::to_string(get_qubits()[i]._prev->get_id()));
    }
    auto const max_prev = *max_element(prevs.begin(), prevs.end(), [](std::string const& a, std::string const& b) {
        return a.size() < b.size();
    });

    for (size_t i = 0; i < _qubits.size(); i++) {
        auto const info        = get_qubits()[i];
        std::string qubit_info = "Q";
        for (size_t j = 0; j < max_qubit.size() - std::to_string(info._qubit).size(); j++)
            qubit_info += " ";
        qubit_info += std::to_string(info._qubit);
        auto const prev_info = (info._prev == nullptr) ? "Start" : ("G" + std::to_string(info._prev->get_id()));
        auto const next_info = (info._next == nullptr) ? "End" : ("G" + std::to_string(info._next->get_id()));
        if (info._isTarget) {
            fmt::println("{0: ^{1}} ┌─{2:─^{3}}─┐ ", "", max_qubit.size() + max_prev.size() + 3, _qubits.size() > 1 ? "┴" : "", gtype.size());
            fmt::println("{0} {1:<{4}} ─┤ {3} ├─ {2}", qubit_info, prev_info, next_info, gtype, max_prev.size());
            fmt::println("{0: ^{1}} └─{0:─^{2}}─┘ ", "", max_qubit.size() + max_prev.size() + 3, gtype.size());
        } else {
            fmt::println("{0} {1:<{5}} ───{3:─^{4}}─── {2}", qubit_info, prev_info, next_info, "●", gtype.size(), max_prev.size());
        }
    }
    if (show_rotation)
        fmt::println("Rotate Phase: {0}", _phase);
    if (show_time)
        fmt::println("Execute at t= {}", get_time());
}

}  // namespace qsyn::qcir
