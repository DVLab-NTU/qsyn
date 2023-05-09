/****************************************************************************
  FileName     [ basic_optimization.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define class Optimizer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <assert.h>  // for assert

#include "optimizer.h"

using namespace std;

extern size_t verbose;

/**
 * @brief Parse the circuit and forward and backward iteratively and optimize it
 *
 * @param doSwap permute the qubit if true
 * @param separateCorrection separate corrections if true
 * @param maxIter
 * @return QCir* Optimized Circuit
 */
QCir* Optimizer::parseCircuit(bool doSwap, bool separateCorrection, size_t maxIter) {
    if (verbose >= 3) cout << "Start optimize" << endl;
    _doSwap = doSwap;
    _minimize_czs = false;
    _separateCorrection = separateCorrection;
    _maxIter = maxIter;
    _reversed = false;
    vector<size_t> prev_stats, stats;
    size_t iter = 0;
    if (verbose >= 5) cout << "Start iteration 0" << endl;
    _circuit = parseForward();
    for (auto& g : _corrections) Optimizer::_addGate2Circuit(_circuit, g);
    _corrections.clear();
    prev_stats = Optimizer::stats(_circuit);

    while (true) {
        if (verbose >= 5) cout << "Start iteration " << iter + 1 << endl;
        _reversed = true;
        _circuit = parseForward();
        for (auto& g : _corrections) Optimizer::_addGate2Circuit(_circuit, g);
        // TODO - This line seems to be redundant
        _corrections.clear();

        _reversed = false;
        _circuit = parseForward();
        iter++;
        stats = Optimizer::stats(_circuit);
        vector<size_t> stats = Optimizer::stats(_circuit);
        // TODO - Find a more efficient way
        if (_minimize_czs && (iter >= _maxIter || (prev_stats[0] <= stats[0] && prev_stats[1] <= stats[1] && prev_stats[2] <= stats[2]))) {
            if (verbose >= 5) cout << "Two-qubit gates: " << stats[0] << ",Had gates: " << stats[1] << ",Non-pauli gates: " << stats[2] << ". Stop the optimizer." << endl;
            break;
        }

        for (auto& g : _corrections) Optimizer::_addGate2Circuit(_circuit, g);
        _corrections.clear();

        prev_stats = stats;
        _minimize_czs = true;
    }

    for (auto& g : _corrections) Optimizer::_addGate2Circuit(_circuit, g);
    _corrections.clear();
    if (verbose >= 3) cout << "Optimize finished." << endl;
    if (verbose >= 5) {
        cout << "Final result is" << endl;
        _circuit->printCircuit();
        _circuit->printGates();
    }
    _circuit->setFileName(_name);
    _circuit->addProcedure("Optimize", _procedures);
    return _circuit;
}

/**
 * @brief Parse through the gates according to topological order and optimize the circuit
 *
 * @return QCir* : new Circuit
 */
QCir* Optimizer::parseForward() {
    if (verbose >= 6) cout << "Start parseForward" << endl;
    reset();
    _circuit->updateTopoOrder();
    vector<QCirGate*> gs = _circuit->getTopoOrderdGates();
    if (_reversed) {
        std::reverse(gs.begin(), gs.end());
        if (verbose >= 6) cout << "Parse the circuit from the end." << endl;
    }
    for (auto& g : gs) parseGate(g);

    // TODO - Find a method to avoid using parameter "erase"
    for (auto& t : _hadamards) {
        addHadamard(t, false);
    }
    _hadamards.clear();

    for (auto& t : _zs) {
        addGate(t, Phase(1), 0);
    }

    QCir* tmp = new QCir(-1);
    // NOTE - Below function will add the gate to tmp -
    topologicalSort(tmp);
    // ------------------------------------------------
    for (auto& t : _xs) {
        QCirGate* notGate = new XGate(_gateCnt);
        notGate->addQubit(t, true);
        _gateCnt++;
        _corrections.emplace_back(notGate);
    }

    _swaps = Optimizer::get_swap_path();

    for (auto& [c, t] : _swaps) {
        // TODO - Use SWAP gate to replace cnots
        QCirGate* cnot_1 = new CXGate(_gateCnt);
        cnot_1->addQubit(c, false);
        cnot_1->addQubit(t, true);
        QCirGate* cnot_2 = new CXGate(_gateCnt + 1);
        cnot_2->addQubit(t, false);
        cnot_2->addQubit(c, true);
        QCirGate* cnot_3 = new CXGate(_gateCnt + 2);
        cnot_3->addQubit(c, false);
        cnot_3->addQubit(t, true);
        _gateCnt += 3;
        _corrections.emplace_back(cnot_1);
        _corrections.emplace_back(cnot_2);
        _corrections.emplace_back(cnot_3);
    }

    if (verbose >= 6) {
        cout << "End parseForward. The temp circuit is" << endl;
        tmp->printCircuit();
        tmp->printGates();
    }

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
    if (verbose >= 8) {
        cout << "Parse the gate" << endl;
        gate->printGate();
    }

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
            if (verbose >= 9) cout << "Transform X gate in Z" << endl;
            _xs.erase(target);
            _zs.emplace(target);
        } else if (!(_xs.contains(target)) && _zs.contains(target)) {
            if (verbose >= 9) cout << "Transform Z into X" << endl;
            _zs.erase(target);
            _xs.emplace(target);
        }
        // NOTE - H-S-H to Sdg-H-Sdg
        if (_gates[target].size() > 1 && _gates[target][_gates[target].size() - 2]->getType() == GateType::H && isSingleRotateZ(_gates[target][_gates[target].size() - 1])) {
            if (verbose >= 9) cout << "Transform H-RZ(ph)-H into RZ(-ph)-H-RZ(-ph)" << endl;
            QCirGate* g2 = _gates[target][_gates[target].size() - 1];
            if (g2->getPhase().getRational().denominator() == 2) {
                // QCirGate* h = _gates[target][_gates[target].size()-2];
                QCirGate* zp = new PGate(_gateCnt);
                zp->addQubit(target, true);
                _gateCnt++;
                zp->setRotatePhase(-1 * g2->getPhase());  // NOTE - S to Sdg
                g2->setRotatePhase(zp->getPhase());       // NOTE - S to Sdg
                _gates[target].insert(_gates[target].end() - 2, zp);
                return true;
            }
        }
        toggleElement(0, target);
    } else if (gate->getType() == GateType::X) {
        if (verbose >= 9) cout << "Cancel X-X into Id" << endl;
        toggleElement(1, target);
    } else if (isSingleRotateZ(gate)) {
        if (_zs.contains(target)) {
            // TODO - Add S/T gate
            _zs.erase(target);
            if (gate->getType() == GateType::RZ || gate->getType() == GateType::P) {
                gate->setRotatePhase(gate->getPhase() + Phase(1));
            } else if (gate->getType() == GateType::Z) {
                return true;
            } else {
                // NOTE - Trans S/S*/T/T* into PGate
                QCirGate* temp = new PGate(_gateCnt);
                _gateCnt++;
                temp->addQubit(target, true);
                temp->setRotatePhase(gate->getPhase() + Phase(1));
                gate = temp;
            }
        }
        if (gate->getPhase() == Phase(0)) {
            if (verbose >= 9) cout << "Cancel with previous RZ" << endl;
            return true;
        }

        if (_xs.contains(target)) {
            gate->setRotatePhase(-1 * (gate->getPhase()));
        }
        if (gate->getPhase() == Phase(1) || gate->getType() == GateType::Z) {
            toggleElement(2, target);
            return true;
        }
        // REVIEW - Neglect adjoint due to S and Sdg is separated
        if (_hadamards.contains(target)) {
            addHadamard(target, true);
        }
        QCirGate* available = getAvailableRotateZ(target);
        if (_availty[target] == false && available != nullptr) {
            std::erase(_available[target], available);
            std::erase(_gates[target], available);
            Phase ph = available->getPhase() + gate->getPhase();
            if (ph == Phase(1)) {
                toggleElement(2, target);
                return true;
            }
            if (ph != Phase(0)) {
                addGate(target, ph, 0);
            }
        } else {
            if (_availty[target] == true) {
                _availty[target] = false;
                _available[target].clear();
            }
            addGate(target, gate->getPhase(), 0);
        }
    } else if (gate->getType() == GateType::CZ) {
        if (control > target) {  // NOTE - Symmetric, let ctrl smaller than targ
            size_t tmp = control;
            gate->setTargetBit(target);
            gate->setControlBit(tmp);
            if (verbose >= 9) cout << "Permutated control at " << control << " target at " << target << endl;
        }
        // NOTE - Push NOT gates trough the CZ
        // REVIEW - Seems strange
        if (_xs.contains(control))
            toggleElement(2, target);
        if (_xs.contains(target))
            toggleElement(2, control);
        if (_hadamards.contains(control) && _hadamards.contains(target)) {
            addHadamard(control, true);
            addHadamard(target, true);
        }
        if (!_hadamards.contains(control) && !_hadamards.contains(target)) {
            addCZ(control, target);
        } else if (_hadamards.contains(control))
            addCX(target, control);
        else
            addCX(control, target);
    } else if (gate->getType() == GateType::CX) {
        if (verbose >= 9) cout << "Permutated control at " << control << " target at " << target << endl;
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
            addHadamard(control, true);
            addCX(control, target);
        }
    } else {
        return false;
    }
    return true;
}

/**
 * @brief Add the gate from storage (_gates) to the circuit
 *
 * @param circuit
 */
void Optimizer::topologicalSort(QCir* circuit) {
    ordered_hashset<size_t> available_id;
    assert(circuit->getNQubit() == 0);

    circuit->addQubit(_circuit->getNQubit());
    while (any_of(_gates.begin(), _gates.end(), [](auto& p_g) { return p_g.second.size(); })) {
        available_id.clear();
        for (auto& [q, gs] : _gates) {
            while (gs.size()) {
                QCirGate* g = gs[0];
                if (g->getType() != GateType::CX && g->getType() != GateType::CZ) {
                    Optimizer::_addGate2Circuit(circuit, g);
                    gs.erase(gs.begin());
                } else if (available_id.contains(g->getId())) {
                    available_id.erase(g->getId());
                    size_t q2 = (q == g->getControl()._qubit) ? g->getTarget()._qubit : g->getControl()._qubit;
                    _gates[q2].erase(--(find_if(_gates[q2].rbegin(), _gates[q2].rend(), [&](QCirGate* _g) { return g->getId() == _g->getId(); })).base());
                    Optimizer::_addGate2Circuit(circuit, g);
                    gs.erase(gs.begin());
                } else {
                    bool type = !(g->getType() == GateType::CZ || g->getControl()._qubit == q);
                    vector<size_t> removed;
                    available_id.emplace(g->getId());
                    for (size_t i = 1; i < gs.size(); i++) {
                        QCirGate* g2 = gs[i];
                        if ((!type && isSingleRotateZ(g2)) || (type && isSingleRotateX(g2))) {
                            Optimizer::_addGate2Circuit(circuit, g2);
                            removed.emplace(removed.begin(), i);
                        } else if (g2->getType() != GateType::CX && g2->getType() != GateType::CZ) {
                            break;
                        } else if ((!type && (g2->getType() == GateType::CZ || g2->getControl()._qubit == q)) ||
                                   (type && (g2->getType() == GateType::CX && g2->getTarget()._qubit == q))) {
                            if (available_id.contains(g2->getId())) {
                                available_id.erase(g2->getId());
                                size_t q2 = q == g2->getControl()._qubit ? g2->getTarget()._qubit : g2->getControl()._qubit;
                                _gates[q2].erase(--(find_if(_gates[q2].rbegin(), _gates[q2].rend(), [&](QCirGate* _g) { return g2->getId() == _g->getId(); })).base());
                                Optimizer::_addGate2Circuit(circuit, g2);
                                removed.emplace(removed.begin(), i);
                            } else {
                                available_id.emplace(g2->getId());
                            }
                        } else {
                            break;
                        }
                    }
                    for (size_t i : removed) {
                        gs.erase(gs.begin() + i);
                    }
                    break;
                }
            }
        }
    }
}

/**
 * @brief Add a Hadamard gate to the output. Called by `parseGate`
 *
 * @param target Index of the target qubit
 */
void Optimizer::addHadamard(size_t target, bool erase) {
    QCirGate* had = new HGate(_gateCnt);
    had->addQubit(target, true);
    _gateCnt++;
    _gates[target].emplace_back(had);
    if (erase) _hadamards.erase(target);
    _available[target].clear();
    _availty[target] = false;
}

/**
 * @brief Add a Cnot gate to the output and do some optimize if possible.
 *
 * @param Indexes of the control and target qubits.
 */
void Optimizer::addCX(size_t ctrl, size_t targ) {
    bool found_match = false;

    if (_availty[ctrl] == true) {
        if (_availty[targ] == false) {
            for (int i = _available[ctrl].size() - 1; i >= 0; i--) {
                QCirGate* g = _available[ctrl][i];
                if (g->getType() == GateType::CX && g->getControl()._qubit == targ && g->getTarget()._qubit == ctrl) {
                    found_match = true;
                    break;
                }
            }
            if (found_match && _doSwap) {
                // NOTE -  -  do the CNOT(t,c)CNOT(c,t) = CNOT(c,t)SWAP(c,t) commutation
                if (verbose >= 9) cout << "Apply a do _swap commutation" << endl;
                if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl); })) {
                    QCirGate* cnot = new CXGate(_gateCnt);
                    cnot->addQubit(ctrl, false);
                    cnot->addQubit(targ, true);
                    _gateCnt++;
                    _gates[ctrl].erase(--(find_if(_gates[ctrl].rbegin(), _gates[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl); })).base());
                    _gates[targ].erase(--(find_if(_gates[targ].rbegin(), _gates[targ].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl); })).base());
                    _availty[ctrl] = false;
                    _availty[targ] = true;
                    _gates[ctrl].emplace_back(cnot);
                    _gates[targ].emplace_back(cnot);
                    _available[ctrl].clear();
                    _available[ctrl].emplace_back(cnot);
                    _available[targ].clear();
                    _available[targ].emplace_back(cnot);

                    swap(_permutation.at(targ), _permutation.at(ctrl));

                    Optimizer::swapElement(0, ctrl, targ);
                    Optimizer::swapElement(1, ctrl, targ);
                    Optimizer::swapElement(2, ctrl, targ);
                    return;
                }
            }
        }
        _available[ctrl].clear();
        _availty[ctrl] = false;
    }
    if (_availty[targ] == false) {
        _available[targ].clear();
        _availty[targ] = true;
    }
    found_match = false;

    for (int i = _available[ctrl].size() - 1; i >= 0; i--) {
        QCirGate* g = _available[ctrl][i];
        if (g->getType() == GateType::CX && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ) {
            found_match = true;
            break;
        }
    }
    // NOTE - do CNOT(c,t)CNOT(c,t) = id
    if (found_match) {
        if (verbose >= 9) cout << "Canncel with previous CX" << endl;
        if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })) {
            _available[ctrl].erase(--(find_if(_available[ctrl].rbegin(), _available[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
            _available[targ].erase(--(find_if(_available[targ].rbegin(), _available[targ].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
            _gates[ctrl].erase(--(find_if(_gates[ctrl].rbegin(), _gates[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
            _gates[targ].erase(--(find_if(_gates[targ].rbegin(), _gates[targ].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
        } else {
            found_match = false;
        }
    }
    if (!found_match) {
        QCirGate* cnot = new CXGate(_gateCnt);
        cnot->addQubit(ctrl, false);
        cnot->addQubit(targ, true);
        _gateCnt++;
        _gates[ctrl].emplace_back(cnot);
        _gates[targ].emplace_back(cnot);
        _available[ctrl].emplace_back(cnot);
        _available[targ].emplace_back(cnot);
    }
}

/**
 * @brief
 *
 */
void Optimizer::addCZ(size_t t1, size_t t2) {
    size_t ctrl = -1, targ = -1;
    QCirGate* cnot;
    bool found_match = false;
    if (_minimize_czs) {
        // NOTE - Checkout t1 as control and t2 as control respectively.
        for (size_t i = 0; i < 2; i++) {
            ctrl = !i ? t1 : t2;
            targ = !i ? t2 : t1;
            for (auto& g : _available[ctrl]) {
                if (g->getType() == GateType::CX && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ) {
                    cnot = new CXGate(_gateCnt);
                    _gateCnt++;
                    cnot->addQubit(g->getControl()._qubit, false);
                    cnot->addQubit(g->getTarget()._qubit, true);
                    if (_availty[targ] == true) {
                        if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* _g) { return Optimizer::TwoQubitGateExist(_g, GateType::CX, ctrl, targ); })) {
                            found_match = true;
                            break;
                        } else
                            continue;
                    }
                    // NOTE - According to pyzx "There are Z-like gates blocking the CNOT from usage
                    //        But if the CNOT can be passed all the way up to these Z-like gates
                    //        Then we can commute the CZ gate next to the CNOT and hence use it."
                    // NOTE - looking at the gates behind the Z-like gates
                    for (int i = _gates[targ].size() - _available[targ].size() - 1; i >= 0; i--) {
                        if (_gates[targ][i]->getType() != GateType::CX || _gates[targ][i]->getTarget()._qubit != targ)
                            break;
                        // TODO - "_gates[targ][i] == g " might be available too
                        if (_gates[targ][i]->getType() == GateType::CX && _gates[targ][i]->getControl()._qubit == ctrl && _gates[targ][i]->getTarget()._qubit == targ) {
                            found_match = true;
                            break;
                        }
                    }
                    if (found_match) break;
                }
            }
            if (found_match) break;
        }
    }
    // NOTE - CNOT-CZ = (S* x id)CNOT (S x S)
    if (found_match) {
        if (verbose >= 9) cout << "Tranform CNOT-CZ into (S* x id)CNOT(S x S)" << endl;
        if (_availty[targ] == true) {
            // REVIEW -  pyzx/optimize/line.339 has a bug
            //  _availty[targ] = 1;
            _available[targ].clear();
        }
        _available[ctrl].erase(--(find_if(_available[ctrl].rbegin(), _available[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
        _gates[ctrl].erase(--(find_if(_gates[ctrl].rbegin(), _gates[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
        _gates[targ].erase(--(find_if(_gates[targ].rbegin(), _gates[targ].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());

        QCirGate* s1 = new SDGGate(_gateCnt);
        s1->addQubit(targ, true);
        _gateCnt++;
        QCirGate* s2 = new SGate(_gateCnt);
        s2->addQubit(targ, true);
        _gateCnt++;
        QCirGate* s3 = new SGate(_gateCnt);
        s3->addQubit(ctrl, true);
        _gateCnt++;

        if (_available[targ].size()) {
            _gates[targ].insert(_gates[targ].end() - _available[targ].size(), s1);
            _gates[targ].insert(_gates[targ].end() - _available[targ].size(), cnot);
        } else {
            _gates[targ].emplace_back(s1);
            _gates[targ].emplace_back(cnot);
        }
        _gates[targ].emplace_back(s2);
        _available[targ].emplace_back(s2);

        _available[ctrl].emplace_back(cnot);
        _available[ctrl].emplace_back(s3);
        _gates[ctrl].emplace_back(cnot);
        _gates[ctrl].emplace_back(s3);
        return;
    }

    if (_availty[t1] == true) {
        _available[t1].clear();
        _availty[t1] = false;
    }
    if (_availty[t2] == true) {
        _available[t2].clear();
        _availty[t2] = false;
    }

    // NOTE - Try to cancel CZ
    found_match = false;
    QCirGate* targ_cz = nullptr;
    for (auto& g : _available[t1]) {
        if ((g->getType() == GateType::CZ && g->getControl()._qubit == t1 && g->getTarget()._qubit == t2) ||
            (g->getType() == GateType::CZ && g->getControl()._qubit == t2 && g->getTarget()._qubit == t1)) {
            found_match = true;
            targ_cz = g;
            break;
        }
    }

    if (found_match) {
        if (verbose >= 9) cout << "Cancel with previous CZ" << endl;
        if (count_if(_available[t2].begin(), _available[t2].end(), [&](QCirGate* g) { return g == targ_cz; })) {
            _available[t1].erase(--(find_if(_available[t1].rbegin(), _available[t1].rend(), [&](QCirGate* g) { return g == targ_cz; })).base());
            _available[t2].erase(--(find_if(_available[t2].rbegin(), _available[t2].rend(), [&](QCirGate* g) { return g == targ_cz; })).base());
            _gates[t1].erase(--(find_if(_gates[t1].rbegin(), _gates[t1].rend(), [&](QCirGate* g) { return g == targ_cz; })).base());
            _gates[t2].erase(--(find_if(_gates[t2].rbegin(), _gates[t2].rend(), [&](QCirGate* g) { return g == targ_cz; })).base());
        } else {
            found_match = false;
        }
    }
    // NOTE - No cancel found
    if (!found_match) {
        QCirGate* cz = new CZGate(_gateCnt);
        if (t1 < t2) {
            cz->addQubit(t1, false);
            cz->addQubit(t2, true);
        } else {
            cz->addQubit(t2, false);
            cz->addQubit(t1, true);
        }

        _gateCnt++;
        _gates[t1].emplace_back(cz);
        _gates[t2].emplace_back(cz);
        _available[t1].emplace_back(cz);
        _available[t2].emplace_back(cz);
    }
}

/**
 * @brief Predicate function called by addCX and addCZ.
 *
 */
bool Optimizer::TwoQubitGateExist(QCirGate* g, GateType gt, size_t ctrl, size_t targ) {
    return (g->getType() == gt && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ);
}

/**
 * @brief Add a single qubit rotate gate to the output.
 *
 * @param target Index of the target qubit
 * @param ph Phase of the gate
 * @param type 0: Z-axis, 1: X-axis, 2: Y-axis
 */
QCirGate* Optimizer::addGate(size_t target, Phase ph, size_t type) {
    QCirGate* rotate = nullptr;
    if (type == 0) {
        rotate = new PGate(_gateCnt);
    } else if (type == 1) {
        rotate = new PXGate(_gateCnt);
    } else if (type == 2) {
        rotate = new PYGate(_gateCnt);
    } else {
        cerr << "Error: wrong type!! Type shoud be 0, 1, or 2";
        return nullptr;
    }
    rotate->setRotatePhase(ph);
    rotate->addQubit(target, true);
    _gates[target].emplace_back(rotate);
    _available[target].emplace_back(rotate);
    _gateCnt++;
    return rotate;
}

/**
 * @brief Add a gate to the circuit.
 *
 * @param QCir* circuit to add
 * @param QCirGate* The gate to be add
 */
void Optimizer::_addGate2Circuit(QCir* circuit, QCirGate* gate) {
    vector<size_t> qubit_list;
    if (gate->getType() == GateType::CX || gate->getType() == GateType::CZ) {
        qubit_list.emplace_back(gate->getControl()._qubit);
    }
    qubit_list.emplace_back(gate->getTarget()._qubit);
    circuit->addGate(gate->getTypeStr(), qubit_list, gate->getPhase(), !_reversed);
}

/**
 * @brief Get the number of two qubit gate, H gate and not pauli gate in a circuit.
 *
 */
vector<size_t> Optimizer::stats(QCir* circuit) {
    size_t two_qubit = 0, had = 0, non_pauli = 0;
    vector<size_t> stats;
    for (const auto& g : circuit->updateTopoOrder()) {
        GateType type = g->getType();
        if (type == GateType::CX || type == GateType::CZ) {
            two_qubit++;
        } else if (type == GateType::H) {
            had++;
        } else if (type != GateType::X && type != GateType::Y && type != GateType::Z && g->getPhase() != Phase(1)) {
            non_pauli++;
        }
    }
    stats.emplace_back(two_qubit);
    stats.emplace_back(had);
    stats.emplace_back(non_pauli);
    return stats;
}

/**
 * @brief Get the swap path from intial permutaion to _permutation now
 *
 * @return vector<pair<size_t, size_t>> swap_path
 */
vector<pair<size_t, size_t>> Optimizer::get_swap_path() {
    vector<std::pair<size_t, size_t>> swap_path;
    unordered_map<size_t, size_t> inv_permutation;
    for (auto [i, j] : _permutation) {
        inv_permutation.emplace(j, i);
    }

    for (size_t i = 0; i < _permutation.size(); i++) {
        if (_permutation[i] == i) continue;
        size_t q1 = _permutation[i], q2 = inv_permutation[i];
        swap_path.emplace_back(make_pair(i, q2));
        _permutation[q2] = q1;
        inv_permutation[q1] = q2;
    }
    return swap_path;
}
