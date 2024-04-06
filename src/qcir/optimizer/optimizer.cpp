/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define class Optimizer member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./optimizer.hpp"

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
    _available_gates.clear();
    _qubit_available.clear();
    _hadamards.clear();
    _xs.clear();
    _zs.clear();
    _swaps.clear();
    _statistics = {};
    _permutation.clear();
    for (size_t i = 0; i < qcir.get_num_qubits(); i++) {
        _qubit_available.emplace_back(false);
        _available_gates.emplace(i, std::vector<size_t>{});
        _gates.emplace(i, std::vector<size_t>{});
        _permutation.emplace(i, i);
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
void Optimizer::_toggle_element(Optimizer::ElementType type, QubitIdType element) {
    switch (type) {
        case ElementType::h:
            (_hadamards.contains(element)) ? (void)_hadamards.erase(element) : (void)_hadamards.emplace(element);
            break;
        case ElementType::x:
            (_xs.contains(element)) ? (void)_xs.erase(element) : (void)_xs.emplace(element);
            break;
        case ElementType::z:
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
void Optimizer::_swap_element(Optimizer::ElementType type, QubitIdType e1, QubitIdType e2) {
    switch (type) {
        case ElementType::h:
            if (_hadamards.contains(e1) && !_hadamards.contains(e2)) {
                _hadamards.erase(e1);
                _hadamards.emplace(e2);
            } else if (_hadamards.contains(e2) && !_hadamards.contains(e1)) {
                _hadamards.erase(e2);
                _hadamards.emplace(e1);
            }
            break;
        case ElementType::x:
            if (_xs.contains(e1) && !_xs.contains(e2)) {
                _xs.erase(e1);
                _xs.emplace(e2);
            } else if (_xs.contains(e2) && !_xs.contains(e1)) {
                _xs.erase(e2);
                _xs.emplace(e1);
            }
            break;
        case ElementType::z:
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
bool Optimizer::is_single_z_rotation(QCirGate const& g) {
    return g.get_operation().is<PZGate>() || g.get_operation().is<RZGate>();
}

/**
 * @brief Is single rotate X
 *
 * @param g
 * @return true
 * @return false
 */
bool Optimizer::is_single_x_rotation(QCirGate const& g) {
    return g.get_operation().is<PXGate>() || g.get_operation().is<RXGate>();
}

/**
 * @brief Is double qubit gate
 *
 * @param g
 * @return true
 * @return false
 */
bool Optimizer::is_cx_or_cz_gate(QCirGate const& g) {
    return g.get_operation() == CXGate() || g.get_operation() == CZGate();
}

/**
 * @brief Get first available rotate gate along Z-axis on qubit `target`
 *
 * @param target which qubit
 * @return QCirGate*
 */
std::optional<size_t> Optimizer::get_available_z_rotation(QubitIdType target) {
    for (auto& g : _available_gates[target]) {
        if (is_single_z_rotation(_storage[g])) {
            return g;
        }
    }
    return std::nullopt;
}

}  // namespace qsyn::qcir
