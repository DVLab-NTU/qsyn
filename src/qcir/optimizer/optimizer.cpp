/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define class Optimizer member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./optimizer.hpp"

#include <optional>

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
    _hs.clear();
    _xs.clear();
    _zs.clear();
    _swaps.clear();
    _statistics = {};
    _permutation.clear();

    _hs.resize(qcir.get_num_qubits(), false);
    _xs.resize(qcir.get_num_qubits(), false);
    _zs.resize(qcir.get_num_qubits(), false);
    _qubit_available.resize(qcir.get_num_qubits(), false);
    _available_gates.resize(qcir.get_num_qubits(), std::vector<size_t>{});
    _gates.resize(qcir.get_num_qubits(), std::vector<size_t>{});
    for (size_t i = 0; i < qcir.get_num_qubits(); i++) {
        _permutation.emplace_back(i);
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
            _hs[element] = !_hs[element];
            break;
        case ElementType::x:
            _xs[element] = !_xs[element];
            break;
        case ElementType::z:
            _zs[element] = !_zs[element];
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
            if (_hs[e1] && !_hs[e2]) {
                _hs[e1] = false;
                _hs[e2] = true;
            } else if (_hs[e2] && !_hs[e1]) {
                _hs[e2] = false;
                _hs[e1] = true;
            }
            break;
        case ElementType::x:
            if (_xs[e1] && !_xs[e2]) {
                _xs[e1] = false;
                _xs[e2] = true;
            } else if (_xs[e2] && !_xs[e1]) {
                _xs[e2] = false;
                _xs[e1] = true;
            }
            break;
        case ElementType::z:
            if (_zs[e1] && !_zs[e2]) {
                _zs[e1] = false;
                _zs[e2] = true;
            } else if (_zs[e2] && !_zs[e1]) {
                _zs[e2] = false;
                _zs[e1] = true;
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
