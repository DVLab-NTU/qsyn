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
    for (size_t i = 0; i < _circuit->getQubits().size(); i++){
        _availty.push_back(1);
        _permutation[i] = _circuit->getQubits()[i]->getId();
    }
}

// FIXME - All functions can be modified, i.e. you may need to pass some parameters or change return type into some functions

/**
 * @brief Parse the circuit and forward and backward iteratively and optimize it
 *
 * @param doSwap permute the qubit if true
 * @param separateCorrection separate corrections if true
 * @param maxIter
 * @return QCir* Optimized Circuit
 */
QCir* Optimizer::parseCircuit(bool doSwap, bool separateCorrection, size_t maxIter) {
    // FIXME - Delete after the function is finished
    cout << "Parse Circuit" << endl;
    _doSwap = doSwap;
    _minimize_czs = false;
    _separateCorrection = separateCorrection;
    _maxIter = maxIter;
    cout << "Before Parse Forward" << endl;
    QCir* forward = parseForward();
    cout << "After Parse Circuit" << endl;
    for (auto& g : _corrections) {
        vector<size_t> bits;
        for (auto& b : g->getQubits()) {
            bits.emplace_back(b._qubit);
        }
        forward->addGate(g->getTypeStr(), bits, Phase(0), true);
    }
    // TODO - Only Single iteration now. Please modify it when forward is correct.
    // _circuit->reset();
    // cout << "This is forward" << endl;
    // forward->printSummary();
    // forward->addGate("X", {0}, Phase(0), false);
    // cout << "Forward add a X" << endl;
    // _circuit = forward;
    return _circuit;
}

/**
 * @brief Parse through the gates according to topological order and optimize the circuit
 *
 * @return QCir* : new Circuit
 */
QCir* Optimizer::parseForward() {
    cout << "1" << endl;
    reset();
    _circuit->updateTopoOrder();
    cout << "2" << endl;
    for (auto& g : _circuit->getTopoOrderdGates()) {
        parseGate(g);
    }
    cout << "3" << endl;
    for (auto& t : _hadamards) {
        addHadamard(t);
    }
    cout << "4" << endl;
    for (auto& t : _zs) {
        addGate(t, Phase(0), 0);
    }
    cout << "5" << endl;
    QCir* tmp = new QCir(-1);
    // NOTE - Below function will add the gate to tmp -
    topologicalSort(tmp);
    cout << "6" << endl;
    // ------------------------------------------------
    for (auto& t : _xs) {
        QCirGate* notGate = new XGate(_gateCnt);
        notGate->addQubit(t, true);
        _gateCnt++;
        _corrections.emplace_back(notGate);
    }
    cout << "7" << endl;

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
    cout << "a" << endl;
    size_t target = -1, control = -1;
    for (auto& [i, j] : _permutation) {
        if (j == gate->getTarget()._qubit) {
            gate->setTargetBit(i);
            target = i;
            break;
        }
    }
    cout << "b" << endl;
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
    cout << "c" << endl;
    cout << gate->getTypeStr() << endl;
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
        cout << "aa" << endl;
        if (_xs.contains(control))
            toggleElement(1, target);
        cout << "ab" << endl;
        if (_zs.contains(target))
            toggleElement(2, control);
        cout << "ac" << endl;
        cout << "Control: " << _hadamards.contains(control) << endl;
        cout << "Target: " << _hadamards.contains(target) << endl;
        if (_hadamards.contains(control) && _hadamards.contains(target)) {
            addCX(target, control);
        } else if (!_hadamards.contains(control) && !_hadamards.contains(target)) {
            cout << "aaa" << endl;
            addCX(control, target);
            cout << "aab" << endl;
        } else if (_hadamards.contains(target)) {
            if (control > target)
                addCZ(target, control);
            else
                addCZ(control, target);
        } else {
            addHadamard(control);
            addCX(control, target);
        }
        cout << "ad" << endl;
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
 * @brief Predicate function called by addCX and addCZ.
 *
 */
bool Optimizer::TwoQubitGateExist(QCirGate* g, GateType gt, size_t ctrl, size_t targ){
    return (g->getType() == gt && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ);
}


/**
 * @brief
 *
 */
void Optimizer::addCZ(size_t t1, size_t t2) {
    size_t ctrl = -1, targ = -1;
    QCirGate* cnot;
    //NOTE - Try to cancel CNOT
    bool found_match = false;
    if (_minimize_czs){
        //NOTE - Checkout t1 as control and t2 as control respectively.
        for (size_t i = 0; i < 2; i++){
            ctrl = i? t1:t2;
            targ = i? t2:t1;
            for (auto& g: _available[ctrl]){
                if(g->getType() == GateType::CX && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ){
                    cnot = g;
                    if (_availty[targ] == 2){
                        if(count_if(_available[targ].begin(), _available[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);})){
                            found_match = true;
                            break;
                        }
                    }
                    //NOTE - According to pyzx "There are Z-like gates blocking the CNOT from usage
                    //       But if the CNOT can be passed all the way up to these Z-like gates
                    //       Then we can commute the CZ gate next to the CNOT and hence use it."
                    //NOTE - looking at the gates behind the Z-like gates
                    for(size_t i=0; i<_gates[targ].size()-_available[targ].size();i++){
                        if(_gates[targ][i]->getType() != GateType::CX || _gates[targ][i]->getTarget()._qubit != targ)
                            break;
                        if(_gates[targ][i]->getType() == GateType::CX && _gates[targ][i]->getControl()._qubit == ctrl && _gates[targ][i]->getTarget()._qubit == targ){
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
    //NOTE - CNOT-CZ = (S* x id)CNOT (S x S)
    if (found_match){
        if (_availty[targ] == 2){
            _availty[targ] = 1;
            _available[targ].clear();
        }
        remove_if(_available[ctrl].begin(), _available[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);});
        remove_if(_gates[ctrl].begin(), _gates[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);});
        remove_if(_gates[targ].begin(), _gates[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);});

        QCirGate* s1 = new SDGGate(_gateCnt);
        s1->addQubit(targ, true);
        _gateCnt++;
        QCirGate* s2 = new SDGGate(_gateCnt);
        s2->addQubit(targ, true);
        _gateCnt++;
        QCirGate* s3 = new SDGGate(_gateCnt);
        s3->addQubit(targ, true);
        _gateCnt++;

        if (_available[targ].size()){
            _gates[targ].insert(_gates[targ].end()-_available[targ].size(), s1);
            _gates[targ].insert(_gates[targ].end()-_available[targ].size(), cnot);
        }else{
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

    if (_availty[t1] == 2){
        _available[t1].clear();
        _availty[t1] = 1;
    }
    if (_availty[t2] == 2){
        _available[t2].clear();
        _availty[t2] = 1;
    }

    //NOTE - Try to cancel CZ
    found_match = false;
    for(auto& g: _available[t2]){
        if((g->getType() == GateType::CZ && g->getControl()._qubit == t1 && g->getTarget()._qubit == t2)||
        (g->getType() == GateType::CZ && g->getControl()._qubit == t2 && g->getTarget()._qubit == t1)){
            found_match = true;
            cnot = g;
            break;
        }
    }

    if(found_match){
        if(count_if(_available[t2].begin(), _available[t2].end(), [&](QCirGate* g){return g == cnot;})){
            remove_if(_available[t1].begin(), _available[t1].end(), [&](QCirGate* g){return g == cnot;});
            remove_if(_available[t2].begin(), _available[t2].end(), [&](QCirGate* g){return g == cnot;});
            remove_if(_gates[t1].begin(), _gates[t1].end(), [&](QCirGate* g){return g == cnot;});
            remove_if(_gates[t2].begin(), _gates[t2].end(), [&](QCirGate* g){return g == cnot;});
        }else{
            found_match = false;
        }
    }
    //NOTE - No cancel found
    if(!found_match){
        QCirGate* cz = new CZGate(_gateCnt);
        cz->addQubit(t1, false);
        cz->addQubit(t2, false);
        // cz->setControlBit(t1);
        // cz->setTargetBit(t2);
        _gateCnt++;
        _gates[t1].emplace_back(cz);
        _gates[t2].emplace_back(cz);
        _available[t1].emplace_back(cz);
        _available[t2].emplace_back(cz);
    }
}

/**
 * @brief Add a Cnot gate to the output and do some optimize if possible.
 * 
 * @param Indexes of the control and target qubits.
 */
void Optimizer::addCX(size_t ctrl, size_t targ) {
    bool found_match = false;
    cout << "ㄅ" << endl;
    if (_availty[ctrl] == 2){
        if (_availty[targ] == 1){
            // TODO - check if reverse is needed
            for (auto& g : _available[ctrl]){
                if (g->getType() == GateType::CX && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ){
                    found_match = true;
                    break;
                }
            
            if (found_match and _doSwap){
                // NOTE -  -  do the CNOT(t,c)CNOT(c,t) = CNOT(c,t)SWAP(c,t) commutation
                if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl);})){
                    QCirGate* cnot = new CXGate(_gateCnt);
                    cnot->setControlBit(ctrl);
                    cnot->setTargetBit(targ);
                    _gateCnt++;
                    remove_if(_gates[ctrl].begin(), _gates[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl);});
                    remove_if(_gates[targ].begin(), _gates[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl);});
                    _availty[ctrl] = 1;
                    _availty[targ] = 2;
                    _gates[ctrl].emplace_back(cnot);
                    _gates[targ].emplace_back(cnot);
                    _available[ctrl].clear();
                    _available[ctrl].emplace_back(cnot);
                    _available[targ].clear();
                    _available[targ].emplace_back(cnot);
                    //TODO - Find another efficient way to swap
                    size_t q1 = _permutation[ctrl];
                    size_t q2 = _permutation[targ];
                    _permutation[ctrl] = q2;
                    _permutation[targ] = q1;
                    Optimizer::swapElement(0, ctrl, targ);
                    Optimizer::swapElement(1, ctrl, targ);
                    Optimizer::swapElement(2, ctrl, targ);
                    return ;
                }
            }
            }
            _available[ctrl].clear();
            _availty[ctrl] = 1;
        }
    }
    cout << "ㄆ" << endl;
    if (_availty[targ] == 1){
        _available[targ].clear();
        _availty[targ] = 2;
    }
    cout << "ㄇ" << endl;
    found_match = false;

    for (auto& g : _available[ctrl]){
        if (g->getType() == GateType::CX && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ){
            found_match = true;
            break;
        }
    }
    cout << "ㄈ" << endl;
    //NOTE - do CNOT(c,t)CNOT(c,t) = id
    if (found_match){
        if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);})){
            remove_if(_available[ctrl].begin(), _available[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);});
            remove_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);});
            remove_if(_gates[ctrl].begin(), _gates[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);});
            remove_if(_gates[targ].begin(), _gates[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);});
        }
        else{
            found_match = false;
        }
    }
    cout << "ㄉ" << endl;
    if (!found_match){
        cout << "1" << endl;
        QCirGate* cnot = new CXGate(_gateCnt);
        cout << "2" << endl;
        cnot->addQubit(ctrl, false);
        cout << "3" << endl;
        cnot->addQubit(targ, true);
        cout << "4" << endl;
        _gateCnt++;
        _gates[ctrl].emplace_back(cnot);
        cout << "5" << endl;
        _gates[targ].emplace_back(cnot);
        cout << "6" << endl;
        _available[ctrl].emplace_back(cnot);
        cout << "7" << endl;
        _available[targ].emplace_back(cnot);
        cout << "8" << endl;
    }
    cout << "ㄊ" << endl;
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
 * @brief Add the gate from storage (_gates) to the circuit
 *
 * @param circuit
 */
void Optimizer::topologicalSort(QCir* circuit) {
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
 * @brief Swap the element in _hadamards, _xs, and _zs
 *
 * @param type 0: _hadamards, 1: _xs, and 2: _zs
 * @param element
 */
void Optimizer::swapElement(size_t type, size_t e1, size_t e2){
    if (type == 0) {
        if (_hadamards.contains(e1) && !_hadamards.contains(e2)){
            _hadamards.erase(e1);
            _hadamards.emplace(e2);
        }
        else if (_hadamards.contains(e2) && !_hadamards.contains(e1)){
            _hadamards.erase(e2);
            _hadamards.emplace(e1);
        }
    }else if (type == 1) {
        if (_xs.contains(e1) && !_xs.contains(e2)){
            _xs.erase(e1);
            _xs.emplace(e2);
        }else if (_xs.contains(e2) && !_xs.contains(e1)){
            _xs.erase(e2);
            _xs.emplace(e1);
        }
    }else if (type == 2) {
        if (_zs.contains(e1) && !_zs.contains(e2)){
            _zs.erase(e1);
            _zs.emplace(e2);
        }
        else if (_zs.contains(e2) && !_zs.contains(e1)){
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