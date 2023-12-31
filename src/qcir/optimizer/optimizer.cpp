/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define class Optimizer member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./optimizer.hpp"

#include <algorithm>
#include <cassert>

#include "../qcir.hpp"
#include "../qcir_gate.hpp"
#include "../qcir_qubit.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::qcir {

/**
 * @brief Reset the storage
 *
 */
void Optimizer::reset(QCir const& qcir) {
    _gates.clear();
    _available.clear();
    _availty.clear();
    _hadamards.clear();
    _xs.clear();
    _zs.clear();
    _swaps.clear();
    _gate_count = 0;
    _statistics = {};
    for (int i = 0; i < gsl::narrow<QubitIdType>(qcir.get_qubits().size()); i++) {
        _availty.emplace_back(false);
        _available.emplace(i, std::vector<QCirGate*>{});
        _gates.emplace(i, std::vector<QCirGate*>{});
        _permutation[i] = qcir.get_qubits()[i]->get_id();
    }
}

QCir Optimizer::parse_forward(QCir const& qcir, bool do_minimize_czs, BasicOptimizationConfig const& config) {
    return _parse_once(qcir, false, do_minimize_czs, config);
}
QCir Optimizer::parse_backward(QCir const& qcir, bool do_minimize_czs, BasicOptimizationConfig const& config) {
    return _parse_once(qcir, true, do_minimize_czs, config);
}

/**
 * @brief Toggle the element in _hadamards, _xs, and _zs
 *
 * @param type 0: _hadamards, 1: _xs, and 2: _zs
 * @param element
 */
void Optimizer::_toggle_element(Optimizer::_ElementType type, QubitIdType element) {
    switch (type) {
        case _ElementType::h:
            (_hadamards.contains(element)) ? (void)_hadamards.erase(element) : (void)_hadamards.emplace(element);
            break;
        case _ElementType::x:
            (_xs.contains(element)) ? (void)_xs.erase(element) : (void)_xs.emplace(element);
            break;
        case _ElementType::z:
            (_zs.contains(element)) ? (void)_zs.erase(element) : (void)_zs.emplace(element);
            break;
    }
}

/**
 * @brief Swap the element in _hadamards, _xs, and _zs
 *
 * @param type 0: _hadamards, 1: _xs, and 2: _zs
 * @param element
 */
void Optimizer::_swap_element(Optimizer::_ElementType type, QubitIdType e1, QubitIdType e2) {
    switch (type) {
        case _ElementType::h:
            if (_hadamards.contains(e1) && !_hadamards.contains(e2)) {
                _hadamards.erase(e1);
                _hadamards.emplace(e2);
            } else if (_hadamards.contains(e2) && !_hadamards.contains(e1)) {
                _hadamards.erase(e2);
                _hadamards.emplace(e1);
            }
            break;
        case _ElementType::x:
            if (_xs.contains(e1) && !_xs.contains(e2)) {
                _xs.erase(e1);
                _xs.emplace(e2);
            } else if (_xs.contains(e2) && !_xs.contains(e1)) {
                _xs.erase(e2);
                _xs.emplace(e1);
            }
            break;
        case _ElementType::z:
            if (_zs.contains(e1) && !_zs.contains(e2)) {
                _zs.erase(e1);
                _zs.emplace(e2);
            } else if (_zs.contains(e2) && !_zs.contains(e1)) {
                _zs.erase(e2);
                _zs.emplace(e1);
            }
            break;
    }
}

/**
 * @brief Is single rotate Z
 *
 * @param g
 * @return true
 * @return false
 */
bool Optimizer::is_single_z_rotation(QCirGate* g) {
    return g->get_num_qubits() == 1 && (g->get_rotation_category() == GateRotationCategory::pz || g->get_rotation_category() == GateRotationCategory::rz);
}

/**
 * @brief Is single rotate X
 *
 * @param g
 * @return true
 * @return false
 */
bool Optimizer::is_single_x_rotation(QCirGate* g) {
    return g->get_num_qubits() == 1 && (g->get_rotation_category() == GateRotationCategory::px || g->get_rotation_category() == GateRotationCategory::rx);
}

/**
 * @brief Is double qubit gate
 *
 * @param g
 * @return true
 * @return false
 */
bool Optimizer::is_double_qubit_gate(QCirGate* g) {
    return g->get_num_qubits() == 2 && (g->is_cx() || g->is_cz());
}

/**
 * @brief Get first available rotate gate along Z-axis on qubit `target`
 *
 * @param target which qubit
 * @return QCirGate*
 */
QCirGate* Optimizer::get_available_z_rotation(QubitIdType target) {
    for (auto& g : _available[target]) {
        if (is_single_z_rotation(g)) {
            return g;
        }
    }
    return nullptr;
}

/**
 * @brief Add a gate (copy) to the circuit.
 *
 * @param QCir* circuit to add
 * @param QCirGate* The gate to be add
 */
void Optimizer::_add_gate_to_circuit(QCir& circuit, QCirGate* gate, bool prepend) {
    auto bit_range = gate->get_qubits() |
                     std::views::transform([](QubitInfo const& qb) { return qb._qubit; });

    circuit.add_gate(gate->get_type_str(), {bit_range.begin(), bit_range.end()}, gate->get_phase(), !prepend);
}

}  // namespace qsyn::qcir
