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
    for (size_t i = 0; i < qcir.get_qubits().size(); i++) {
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
void Optimizer::_toggle_element(GateType const& type, size_t element) {
    if (type == GateType::h) {
        if (_hadamards.contains(element))
            _hadamards.erase(element);
        else
            _hadamards.emplace(element);
    } else if (type == GateType::x) {
        if (_xs.contains(element))
            _xs.erase(element);
        else
            _xs.emplace(element);
    } else if (type == GateType::z) {
        if (_zs.contains(element))
            _zs.erase(element);
        else
            _zs.emplace(element);
    }
}

/**
 * @brief Swap the element in _hadamards, _xs, and _zs
 *
 * @param type 0: _hadamards, 1: _xs, and 2: _zs
 * @param element
 */
void Optimizer::_swap_element(size_t type, size_t e1, size_t e2) {
    if (type == 0) {
        if (_hadamards.contains(e1) && !_hadamards.contains(e2)) {
            _hadamards.erase(e1);
            _hadamards.emplace(e2);
        } else if (_hadamards.contains(e2) && !_hadamards.contains(e1)) {
            _hadamards.erase(e2);
            _hadamards.emplace(e1);
        }
    } else if (type == 1) {
        if (_xs.contains(e1) && !_xs.contains(e2)) {
            _xs.erase(e1);
            _xs.emplace(e2);
        } else if (_xs.contains(e2) && !_xs.contains(e1)) {
            _xs.erase(e2);
            _xs.emplace(e1);
        }
    } else if (type == 2) {
        if (_zs.contains(e1) && !_zs.contains(e2)) {
            _zs.erase(e1);
            _zs.emplace(e2);
        } else if (_zs.contains(e2) && !_zs.contains(e1)) {
            _zs.erase(e2);
            _zs.emplace(e1);
        }
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
    if (g->get_type() == GateType::p ||
        g->get_type() == GateType::z ||
        g->get_type() == GateType::s ||
        g->get_type() == GateType::sdg ||
        g->get_type() == GateType::t ||
        g->get_type() == GateType::tdg ||
        g->get_type() == GateType::rz)
        return true;
    else
        return false;
}

/**
 * @brief Is single rotate X
 *
 * @param g
 * @return true
 * @return false
 */
bool Optimizer::is_single_x_rotation(QCirGate* g) {
    if (g->get_type() == GateType::x ||
        g->get_type() == GateType::sx ||
        g->get_type() == GateType::rx)
        return true;
    else
        return false;
}

/**
 * @brief Is double qubit gate
 *
 * @param g
 * @return true
 * @return false
 */
bool Optimizer::is_double_qubit_gate(QCirGate* g) {
    if (g->get_type() == GateType::cx ||
        g->get_type() == GateType::cz)
        return true;
    else
        return false;
}

/**
 * @brief Get first available rotate gate along Z-axis on qubit `target`
 *
 * @param target which qubit
 * @return QCirGate*
 */
QCirGate* Optimizer::get_available_z_rotation(size_t target) {
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
