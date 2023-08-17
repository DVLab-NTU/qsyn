/****************************************************************************
  FileName     [ optimizer.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define class Optimizer member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./optimizer.hpp"

#include <algorithm>
#include <cassert>

#include "qcir/qcir.hpp"
#include "qcir/qcirGate.hpp"
#include "qcir/qcirQubit.hpp"

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
    _gateCnt = 0;
    _statistics = {};
    for (size_t i = 0; i < qcir.getQubits().size(); i++) {
        _availty.emplace_back(false);
        _available.emplace(i, std::vector<QCirGate*>{});
        _gates.emplace(i, std::vector<QCirGate*>{});
        _permutation[i] = qcir.getQubits()[i]->getId();
    }
}

QCir Optimizer::parseForward(QCir const& qcir, bool minimizeCZ, BasicOptimizationConfig const& config) {
    return parseOnce(qcir, false, minimizeCZ, config);
}
QCir Optimizer::parseBackward(QCir const& qcir, bool minimizeCZ, BasicOptimizationConfig const& config) {
    return parseOnce(qcir, true, minimizeCZ, config);
}

/**
 * @brief Toggle the element in _hadamards, _xs, and _zs
 *
 * @param type 0: _hadamards, 1: _xs, and 2: _zs
 * @param element
 */
void Optimizer::toggleElement(GateType const& type, size_t element) {
    if (type == GateType::H) {
        if (_hadamards.contains(element))
            _hadamards.erase(element);
        else
            _hadamards.emplace(element);
    } else if (type == GateType::X) {
        if (_xs.contains(element))
            _xs.erase(element);
        else
            _xs.emplace(element);
    } else if (type == GateType::Z) {
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
void Optimizer::swapElement(size_t type, size_t e1, size_t e2) {
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
bool Optimizer::isSingleRotateZ(QCirGate* g) {
    if (g->getType() == GateType::P ||
        g->getType() == GateType::Z ||
        g->getType() == GateType::S ||
        g->getType() == GateType::SDG ||
        g->getType() == GateType::T ||
        g->getType() == GateType::TDG ||
        g->getType() == GateType::RZ)
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
bool Optimizer::isSingleRotateX(QCirGate* g) {
    if (g->getType() == GateType::X ||
        g->getType() == GateType::SX ||
        g->getType() == GateType::RX)
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
bool Optimizer::isDoubleQubitGate(QCirGate* g) {
    if (g->getType() == GateType::CX ||
        g->getType() == GateType::CZ)
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
QCirGate* Optimizer::getAvailableRotateZ(size_t target) {
    for (auto& g : _available[target]) {
        if (isSingleRotateZ(g)) {
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
void Optimizer::_addGate2Circuit(QCir& circuit, QCirGate* gate, bool prepend) {
    auto bit_range = gate->getQubits() |
                     std::views::transform([](BitInfo const& qb) { return qb._qubit; });

    circuit.addGate(gate->getTypeStr(), {bit_range.begin(), bit_range.end()}, gate->getPhase(), !prepend);
}
