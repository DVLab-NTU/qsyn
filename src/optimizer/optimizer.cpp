/****************************************************************************
  FileName     [ optimizer.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define class Optimizer member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "optimizer.h"

#include <assert.h>  // for assert

using namespace std;

extern size_t verbose;

/**
 * @brief Reset the storage
 *
 */
void Optimizer::reset() {
    _gates.clear();
    _available.clear();
    _availty.clear();
    _hadamards.clear();
    _xs.clear();
    _zs.clear();
    _corrections.clear();
    _gateCnt = 0;
    for (size_t i = 0; i < _circuit->getQubits().size(); i++)
        _permutation[i] = _circuit->getQubits()[i]->getId();
}

// FIXME - All functions can be modified, i.e. you may need to pass some parameters or change return type into some functions

/**
 * @brief
 *
 * @return QCir*
 */
QCir* Optimizer::parseCircuit() {
    // FIXME - Delete after the function is finished
    cout << "Parse Circuit" << endl;
    return nullptr;
}

/**
 * @brief Parse through the gates according to topological order and optimize the circuit
 *
 * @return QCir* : new Circuit
 */
QCir* Optimizer::parseForward() {
    reset();
    _circuit->updateTopoOrder();
    for (auto& g : _circuit->getTopoOrderdGates()) {
        parseGate(g);
    }
    for (auto& t : _hadamards) {
        addHadamard(t);
    }
    for (auto& t : _zs) {
        addGate(t, Phase(0), 0);
    }
    QCir* tmp = new QCir(-1);
    // NOTE - Below function will add the gate to tmp -
    topologicalSort();
    // ------------------------------------------------
    for (auto& t : _xs) {
        QCirGate* notGate = new XGate(_gateCnt);
        notGate->addQubit(t, true);
        _gateCnt++;
        _corrections.emplace_back(notGate);
    }

    // TODO - Move permutation code out from extractor
    // for (auto& [a, b] : permutation(_permutation)) {
    //     QCirGate* CX0 = new CXGate(_gateCnt);
    //     .....expand SWAP to CXs
    //     _corrections.emplace_back(notGate);
    // }
    return tmp;
}

/**
 * @brief Parse the gate
 *
 * @param gate
 * @return true : Successfully parse a gate,
 * @return false : Encounter a gate that is not supported.
 */
bool Optimizer::parseGate(QCirGate* gate) {
    // REVIEW - No copy
    // NOTE - Permute
    size_t target = -1, control = -1;
    for (auto& [i, j] : _permutation) {
        if (j == gate->getTarget()._qubit) {
            gate->setTargetBit(i);
            target = i;
            break;
        }
    }
    assert(target != size_t(-1));
    if (gate->getType() == GateType::CX || gate->getType() == GateType::CZ) {
        for (auto& [i, j] : _permutation) {
            if (j == gate->getControl()._qubit) {
                gate->setControlBit(i);
                control = i;
                break;
            }
        }
        assert(control != size_t(-1));
    }

    // NOTE - Hadamard
    if (gate->getType() == GateType::H) {
        if (_xs.contains(target) && (!_zs.contains(target))) {
            _xs.erase(target);
            _zs.emplace(target);
        } else if (!(_xs.contains(target)) && _zs.contains(target)) {
            _zs.erase(target);
            _xs.emplace(target);
        }
        // NOTE - H-S-H to Sdg-H-Sdg
        if (_gates[target].size() > 1 && _gates[target][_gates[target].size() - 2]->getType() == GateType::H && isSingleRotateZ(_gates[target][_gates[target].size() - 1])) {
            QCirGate* g2 = _gates[target][_gates[target].size() - 1];
            if (g2->getPhase().getRational().denominator() == 2) {
                // QCirGate* h = _gates[target][_gates[target].size()-2];
                QCirGate* zp = new PGate(_gateCnt);
                _gateCnt++;
                zp->setRotatePhase(-1 * g2->getPhase());  // NOTE - S to Sdg
                g2->setRotatePhase(zp->getPhase());       // NOTE - S to Sdg
                _gates[target].insert(_gates[target].end() - 2, zp);
                return true;
            }
        }
        toggleElement(0, target);
    } else if (gate->getType() == GateType::X) {
        toggleElement(1, target);
    } else if (isSingleRotateZ(gate)) {
        if (_zs.contains(target)) {
            gate->setRotatePhase(gate->getPhase() + Phase(1));
            _zs.erase(target);
        }
        if (gate->getPhase() == Phase(0))
            return true;
        if (_xs.contains(target)) {
            gate->setRotatePhase(-1 * (gate->getPhase()));
        }
        if (gate->getPhase() == Phase(1)) {
            toggleElement(2, target);
            return true;
        }
        // REVIEW - Neglect adjoint due to S and Sdg is separated
        if (_hadamards.contains(target)) {
            addHadamard(target);
        }
        QCirGate* available = getAvailableRotateZ(target);
        if (_availty[target] == 1 && available != nullptr) {
            _available[target].erase(remove(_available[target].begin(), _available[target].end(), available), _available[target].end());
            _gates[target].erase(remove(_gates[target].begin(), _gates[target].end(), available), _gates[target].end());
            Phase ph = available->getPhase() + gate->getPhase();
            if (ph == Phase(1)) {
                toggleElement(2, target);
                return true;
            }
            if (ph != Phase(0)) {
                addGate(target, ph, 0);
            }
        } else {
            if (_availty[target] == 2) {
                _availty[target] = 1;
                _available[target].clear();
            }
            addGate(target, gate->getPhase(), 0);
        }
    } else if (gate->getType() == GateType::CZ) {
        if (control > target) {  // NOTE - Symmetric, let ctrl smaller than targ
            size_t tmp = control;
            gate->setTargetBit(target);
            gate->setControlBit(tmp);
        }
        // NOTE - Push NOT gates trough the CZ
        // REVIEW - Seems strange
        if (_xs.contains(control))
            toggleElement(2, target);
        if (_xs.contains(target))
            toggleElement(2, control);

        if (_hadamards.contains(control) && _hadamards.contains(target)) {
            addHadamard(control);
            addHadamard(target);
        }
        if (!_hadamards.contains(control) && !_hadamards.contains(target)) {
            addCZ(control, target);
        } else if (_hadamards.contains(control))
            addCX(target, control);
        else
            addCX(control, target);
    } else if (gate->getType() == GateType::CX) {
        if (_xs.contains(control))
            toggleElement(1, target);
        if (_zs.contains(target))
            toggleElement(2, control);
        if (_hadamards.contains(control) && _hadamards.contains(target)) {
            addCX(target, control);
        } else if (!_hadamards.contains(control) && !_hadamards.contains(target)) {
            addCX(control, target);
        } else if (_hadamards.contains(target)) {
            if (control > target)
                addCZ(target, control);
            else
                addCZ(control, target);
        } else {
            addHadamard(control);
            addCX(control, target);
        }
    } else {
        cout << "Error: Unsupported Gate type " << gate->getTypeStr() << " !!" << endl;
        return false;
    }
    return true;
}

/**
 * @brief Add a Hadamard gate to the output. Called by `parseGate`
 *
 * @param target Index of the target qubit
 */
void Optimizer::addHadamard(size_t target) {
    QCirGate* had = new HGate(_gateCnt);
    had->addQubit(target, true);
    _gateCnt++;
    _gates[target].emplace_back(had);
    _hadamards.erase(target);
    _available[target].clear();
    _availty[target] = 1;
}

/**
 * @brief
 *
 */
void Optimizer::addCZ(size_t ctrl, size_t targ) {
}

/**
 * @brief
 *
 */
void Optimizer::addCX(size_t ctrl, size_t targ) {
}

/**
 * @brief Add a single qubit rotate gate to the output.
 *
 * @param target Index of the target qubit
 * @param ph Phase of the gate
 * @param type 0: Z-axis, 1: X-axis, 2: Y-axis
 */
void Optimizer::addGate(size_t target, Phase ph, size_t type) {
    QCirGate* rotate = nullptr;
    if (type == 0) {
        rotate = new PGate(_gateCnt);
    } else if (type == 1) {
        rotate = new PXGate(_gateCnt);
    } else if (type == 2) {
        rotate = new PYGate(_gateCnt);
    } else {
        cerr << "Error: wrong type!! Type shoud be 0, 1, or 2";
        return;
    }
    rotate->setRotatePhase(ph);
    rotate->addQubit(target, true);
    _gates[target].emplace_back(rotate);
    _available[target].emplace_back(rotate);
    _gateCnt++;
}

/**
 * @brief
 *
 */
void Optimizer::topologicalSort() {
}

/**
 * @brief Toggle the element in _hadamards, _xs, and _zs
 *
 * @param type 0: _hadamards, 1: _xs, and 2: _zs
 * @param element
 */
void Optimizer::toggleElement(size_t type, size_t element) {
    if (type == 0) {
        if (_hadamards.contains(element))
            _hadamards.erase(element);
        else
            _hadamards.emplace(element);
    } else if (type == 1) {
        if (_xs.contains(element))
            _xs.erase(element);
        else
            _xs.emplace(element);
    } else if (type == 2) {
        if (_zs.contains(element))
            _zs.erase(element);
        else
            _zs.emplace(element);
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

QCirGate* Optimizer::getAvailableRotateZ(size_t target) {
    for (auto& g : _available[target]) {
        if (isSingleRotateZ(g)) {
            return g;
        }
    }
    return nullptr;
}