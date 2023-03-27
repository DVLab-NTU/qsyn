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
    _swaps.clear();
    _corrections.clear();
    _gateCnt = 0;
    for (size_t i = 0; i < _circuit->getQubits().size(); i++){
        vector<QCirGate*> empty, empty2;
        _availty.emplace_back(1);
        _available[i] = empty;
        _gates[i] = empty2;
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
    _doSwap = doSwap;
    _minimize_czs = false;
    _separateCorrection = separateCorrection;
    _maxIter = maxIter;
    vector<size_t> prev_stats, stats;
    size_t iter = 0;
    cout << "Before Parse Forward" << endl;
    // QCir* forward = parseForward();
    _circuit = parseForward(false);
    cout << "After Parse Circuit" << endl;
    _circuit->printGates();
    cout << "corrections size: " << _corrections.size() << endl;
    for (auto& g : _corrections) {
        vector<size_t> bits;
        for (auto& b : g->getQubits()) {
            bits.emplace_back(b._qubit);
        }
        cout << "Add gate" << endl;
        g->printGate();
        _circuit->addGate(g->getTypeStr(), bits, Phase(0), true);
    }
    cout << "After add corrections" << endl;
    _circuit->printGates();
    _corrections.clear();
    prev_stats = Optimizer::stats(_circuit);

    // TODO - Only Single iteration now. Please modify it when forward is correct.
    size_t _count = 1;
    
    while (true){
        cout << "###########  Number " << _count << " iter #############" << endl;
        _count++;
        _circuit = parseForward(true);
        cout << "corrections size1: " << _corrections.size() << endl;
        for (auto& g : _corrections) {
            vector<size_t> bits;
            for (auto& b : g->getQubits()) {
                bits.emplace_back(b._qubit);
            }
            _circuit->addGate(g->getTypeStr(), bits, Phase(0), true);
        }
        _corrections.clear();
        
        _circuit = parseForward(false);
        iter++;
        stats = Optimizer::stats(_circuit);
        vector<size_t> stats = Optimizer::stats(_circuit);
        if(_minimize_czs && (iter >= _maxIter || 
        (prev_stats[0] <= stats[0] && prev_stats[1] <= stats[1] && prev_stats[2] <= stats[2]))){
            break;
        }

        cout << "corrections size2: " << _corrections.size() << endl;
        for (auto& g : _corrections) {
            vector<size_t> bits;
            for (auto& b : g->getQubits()) {
                bits.emplace_back(b._qubit);
            }
            _circuit->addGate(g->getTypeStr(), bits, Phase(0), true);
        }
        _corrections.clear();
        
        prev_stats = stats;
        _minimize_czs = true;

    }
    
    
    
    
    // _circuit = forward;
    // _circuit->printQubits();
    for (auto& g : _corrections) {
        vector<size_t> bits;
        for (auto& b : g->getQubits()) {
            bits.emplace_back(b._qubit);
        }
        _circuit->addGate(g->getTypeStr(), bits, Phase(0), true);
    }
    cout << "Final result" << endl;
    _circuit->printGates();
    _circuit->printQubits();
    return _circuit;
}

/**
 * @brief Parse through the gates according to topological order and optimize the circuit
 *
 * @return QCir* : new Circuit
 */
QCir* Optimizer::parseForward(bool reverse=false) {
    reset();
    cout << "Start parseForward" << endl;
    _circuit->printGates();
    _circuit->updateTopoOrder();
    vector<QCirGate*> gs = _circuit->getTopoOrderdGates();

    if (reverse) {
        // cout << "Before reverse" << endl;
        // for(auto& g: gs){
        //     g->printGate();
        // }
        std::reverse(gs.begin(), gs.end());
        cout << "reverse" << endl;
        // cout << "After reverse" << endl;
        // for(auto& g: gs){
        //     g->printGate();
        // }
    }
    for (auto& g : gs) {
        parseGate(g);
    }
    for (auto& t : _hadamards) {
        addHadamard(t);
    }
    for (auto& t : _zs) {
        cout << "Into zs" << endl;
        QCirGate* zgate = addGate(t, Phase(0), 0);
        _gates[t].emplace_back(zgate);
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

    for (auto& [c, t] : _swaps) {
        QCirGate* cnot_1 = new CXGate(_gateCnt);
        cnot_1->addQubit(c, false);
        cnot_1->addQubit(t, true);
        QCirGate* cnot_2 = new CXGate(_gateCnt+1);
        cnot_2->addQubit(t, false);
        cnot_2->addQubit(c, true);
        QCirGate* cnot_3 = new CXGate(_gateCnt+2);
        cnot_3->addQubit(c, false);
        cnot_3->addQubit(t, true);
        _gateCnt+=3;
        _corrections.emplace_back(cnot_1);
        _corrections.emplace_back(cnot_2);
        _corrections.emplace_back(cnot_3);
        
    }

    // TODO - Move permutation code out from extractor
    // for (auto& [a, b] : permutation(_permutation)) {
    //     QCirGate* CX0 = new CXGate(_gateCnt);
    //     .....expand SWAP to CXs
    //     _corrections.emplace_back(notGate);
    // }
    cout << "tmp is" << endl;
    tmp->printGates();
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
        // cout << "_permutation["<< i <<"] "<< j <<endl;
        // cout << "Target: " << gate->getTarget()._qubit << endl;
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
        cout << "issingleZ" << endl;
        if (_zs.contains(target)) {
            cout << "Into z1 loop" << endl;
            gate->setRotatePhase(gate->getPhase() + Phase(1));
            _zs.erase(target);
        }
        if (gate->getPhase() == Phase(0) && gate->getType() == GateType::RZ){
            cout << "Into z2 loop" << endl;
            return true;
        }
            
        if (_xs.contains(target)) {
            cout << "Into z3 loop" << endl;
            gate->setRotatePhase(-1 * (gate->getPhase()));
        }
        if (gate->getPhase() == Phase(1) || gate->getType() == GateType::Z) {
            cout << "Into z4 loop" << endl;
            toggleElement(2, target);
            return true;
        }
        // REVIEW - Neglect adjoint due to S and Sdg is separated
        if (_hadamards.contains(target)) {
            cout << "Into z6 loop" << endl;
            addHadamard(target);
        }
        QCirGate* available = getAvailableRotateZ(target);
        if (_availty[target] == 1 && available != nullptr) {
            cout << "Into z7 loop" << endl;
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
            cout << "Into z6 else loop" << endl;
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
 * @brief Predicate function called by addCX and addCZ.
 *
 */
bool Optimizer::TwoQubitGateExist(QCirGate* g, GateType gt, size_t ctrl, size_t targ){
    // cout << "TwoQubitGateExist()" << endl;
    // cout << (g->getType() == gt && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ) << endl;
    return (g->getType() == gt && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ);
}


/**
 * @brief
 *
 */
void Optimizer::addCZ(size_t t1, size_t t2) {
    cout<<"_______________APPLY ADDCZ_______________" << endl;
    size_t ctrl = -1, targ = -1;
    QCirGate* cnot;
    //NOTE - Try to cancel CNOT
    bool found_match = false;
    if (_minimize_czs){
        cout << "Into first loop" << endl;
        //NOTE - Checkout t1 as control and t2 as control respectively.
        for (size_t i = 0; i < 2; i++){
            cout << "Into for loop" << endl;
            ctrl = i? t1:t2;
            targ = i? t2:t1;
            for (auto& g: _available[ctrl]){
                cout << "Into second for loop" << endl;
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
                    for(size_t i=_gates[targ].size()-_available[targ].size()-1; i>=0;i--){
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
        cout << "Into second loop" << endl;
        cout << "CZ CNOT" << endl; 
        if (_availty[targ] == 2){
            _availty[targ] = 1;
            _available[targ].clear();
        }
        _available[ctrl].erase(find_if(_available[ctrl].begin(), _available[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);}));
        _gates[ctrl].erase(find_if(_gates[ctrl].begin(), _gates[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);}));
        _gates[targ].erase(find_if(_gates[targ].begin(), _gates[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);}));

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
        cout << "Into 3rd loop" << endl;
        _available[t1].clear();
        _availty[t1] = 1;
    }
    if (_availty[t2] == 2){
        cout << "Into 4th loop" << endl;
        _available[t2].clear();
        _availty[t2] = 1;
    }

    //NOTE - Try to cancel CZ
    found_match = false;
    // TODO - checkout if "reverse" is necessary 
    for(auto& g: _available[t2]){
        cout << "Into for loop" << endl;
        if((g->getType() == GateType::CZ && g->getControl()._qubit == t1 && g->getTarget()._qubit == t2)||
        (g->getType() == GateType::CZ && g->getControl()._qubit == t2 && g->getTarget()._qubit == t1)){
            found_match = true;
            cnot = g;
            break;
        }
    }

    if(found_match){
        cout << "Into 5th loop" << endl;
        if(count_if(_available[t2].begin(), _available[t2].end(), [&](QCirGate* g){return g == cnot;})){
            _available[t1].erase(find_if(_available[t1].begin(), _available[t1].end(), [&](QCirGate* g){return g == cnot;}));
            _available[t2].erase(find_if(_available[t2].begin(), _available[t2].end(), [&](QCirGate* g){return g == cnot;}));
            _gates[t1].erase(find_if(_gates[t1].begin(), _gates[t1].end(), [&](QCirGate* g){return g == cnot;}));
            _gates[t2].erase(find_if(_gates[t2].begin(), _gates[t2].end(), [&](QCirGate* g){return g == cnot;}));
        }else{
            found_match = false;
        }
    }
    //NOTE - No cancel found
    if(!found_match){
        cout << "Into 6th loop" << endl;
        QCirGate* cz = new CZGate(_gateCnt);
        // ctrl < targ
        if(t1<t2){
            cz->addQubit(t1, false);
            cz->addQubit(t2, true); 
        }else{
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
 * @brief Add a Cnot gate to the output and do some optimize if possible.
 * 
 * @param Indexes of the control and target qubits.
 */
void Optimizer::addCX(size_t ctrl, size_t targ) {
    cout<<"_______________APPLY ADDCX_______________" << endl;
    bool found_match = false;
    cout<<"Control: "<< ctrl << " Target: " << targ <<endl;
    if (_availty[ctrl] == 2){
        cout << "Into first loop" << endl;
        if (_availty[targ] == 1){
            for (int i=_available[ctrl].size()-1; i>=0; i--){
                QCirGate* g = _available[ctrl][i];
                if (g->getType() == GateType::CX && g->getControl()._qubit == targ && g->getTarget()._qubit == ctrl){
                    found_match = true;
                    break;
                }
            }
            if (found_match && _doSwap){
                cout << "doswap" << endl;
                // NOTE -  -  do the CNOT(t,c)CNOT(c,t) = CNOT(c,t)SWAP(c,t) commutation
                if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl);})){
                    QCirGate* cnot = new CXGate(_gateCnt);
                    cnot->addQubit(ctrl, false);
                    cnot->addQubit(targ, true);
                    _gateCnt++;
                    _gates[ctrl].erase(find_if(_gates[ctrl].begin(), _gates[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl);}));
                    _gates[targ].erase(find_if(_gates[targ].begin(), _gates[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl);}));
                    _availty[ctrl] = 1;
                    _availty[targ] = 2;
                    _gates[ctrl].emplace_back(cnot);
                    _gates[targ].emplace_back(cnot);
                    _available[ctrl].clear();
                    _available[ctrl].emplace_back(cnot);
                    _available[targ].clear();
                    _available[targ].emplace_back(cnot);

                    //TODO - Implement the permutation swap
                    swap(_permutation.at(targ), _permutation.at(ctrl));
                    _swaps.emplace_back(make_pair(targ, ctrl));
                    // cout << "After swap " << endl;
                    cout << _permutation[ctrl] << endl;
                    // cout << "Size: " << _permutation.size() << endl;
                    // for(auto i: _permutation){
                    //     cout << "_permutation["<< i.first <<"] "<< i.second <<endl;
                    // }

                    Optimizer::swapElement(0, ctrl, targ);
                    Optimizer::swapElement(1, ctrl, targ);
                    Optimizer::swapElement(2, ctrl, targ);
                    cout << "End doswap" << endl;
                    for (size_t i = 0; i < _gates.size(); i++)
                    {
                        cout << "_gates["<<i<<"]" << endl;
                        for (size_t j = 0; j < _gates[i].size(); j++)
                        {
                            _gates[i][j]->printGate();
                        }
                    }
                    for (size_t i = 0; i < _available.size(); i++)
                    {
                        cout << "_available["<<i<<"]" << endl;
                        for (size_t j = 0; j < _available[i].size(); j++)
                        {
                            _available[i][j]->printGate();
                        }
                    }
                    for (size_t i = 0; i < _availty.size(); i++)
                    {
                        cout << "_availy["<<i<<"]: "<<_availty[i]<<"  ";
                    }
                    cout << endl;
                    return ;
                }
            }
        }
        _available[ctrl].clear();
        _availty[ctrl] = 1;
    }
    if (_availty[targ] == 1){
        cout << "Into second loop" << endl;
        _available[targ].clear();
        _availty[targ] = 2;
    }
    found_match = false;

    // cout<<"New iteration" << endl;
    // cout << "G to added" << "(" << ctrl << ", " << targ << ")" << endl;
    for (int i=_available[ctrl].size()-1; i>=0; i--){
        cout << "into for loop " << endl;
        QCirGate* g = _available[ctrl][i];
        // cout << "Type " << (g->getType() == GateType::CX) << endl;
        // cout << "Control " << (g->getControl()._qubit == ctrl) << endl;
        // cout << "Target " << (g->getTarget()._qubit == targ) << endl;
        // cout << "G in _ava" << "(" << g->getControl()._qubit << ", " << g->getTarget()._qubit << ")" << endl;
        if (g->getType() == GateType::CX && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ){
            found_match = true;
            break;
        }
    }
    // cout << "Found match: " << found_match << endl;
    //NOTE - do CNOT(c,t)CNOT(c,t) = id
    if (found_match){
        cout << "Into third loop(found match)" << endl;
        if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);})){          
            _available[ctrl].erase(find_if(_available[ctrl].begin(), _available[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);}));
            _available[targ].erase(find_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);}));
            _gates[ctrl].erase(find_if(_gates[ctrl].begin(), _gates[ctrl].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);}));
            _gates[targ].erase(find_if(_gates[targ].begin(), _gates[targ].end(), [&](QCirGate* g){return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ);}));
        }else{
            found_match = false;
        }
    }
    if (!found_match){
        cout << "Into fourth loop(!found match)" << endl;
        QCirGate* cnot = new CXGate(_gateCnt);
        cnot->addQubit(ctrl, false);
        cnot->addQubit(targ, true);
        // cout << "Target cnot" << endl;
        // cnot->printGate();
        _gateCnt++;
        // cout << "Before emplace cnot: (" << _gates[ctrl].size() << ", " << _gates[targ].size() << ", " << _available[ctrl].size() << ", " << _available[targ].size() << ")" << endl;
        _gates[ctrl].emplace_back(cnot);
        _gates[targ].emplace_back(cnot);
        _available[ctrl].emplace_back(cnot);
        _available[targ].emplace_back(cnot);
        // cout << "After emplace cnot: (" << _gates[ctrl].size() << ", " << _gates[targ].size() << ", " << _available[ctrl].size() << ", " << _available[targ].size() << ")" << endl;
    }
    cout << "After the iter:" <<endl;
    for (size_t i = 0; i < _gates.size(); i++)
    {
        cout << "_gates["<<i<<"]" << endl;
        for (size_t j = 0; j < _gates[i].size(); j++)
        {
            _gates[i][j]->printGate();
        }
    }
    for (size_t i = 0; i < _available.size(); i++)
    {
        cout << "_available["<<i<<"]" << endl;
        for (size_t j = 0; j < _available[i].size(); j++)
        {
            _available[i][j]->printGate();
        }
    }
    for (size_t i = 0; i < _availty.size(); i++)
    {
        cout << "_availy["<<i<<"]: "<<_availty[i]<<"  ";
    }
    cout << endl;
    
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
 * @param 
 */
void Optimizer::_addGate2Circuit(QCir* circuit, QCirGate* gate){
    // cout << "AddGate2Circuit " << gate->getTypeStr() <<endl;
    vector<size_t> qubit_list;
    if(gate->getType() == GateType::CX || gate->getType() == GateType::CZ){
        qubit_list.emplace_back(gate->getControl()._qubit);
    }
    qubit_list.emplace_back(gate->getTarget()._qubit);
    circuit->addGate(gate->getTypeStr(), qubit_list, gate->getPhase(), true);
}

/**
 * @brief Add the gate from storage (_gates) to the circuit
 *
 * @param circuit
 */
void Optimizer::topologicalSort(QCir* circuit) {
    ordered_hashset<size_t> available_id;
    circuit->addQubit(_circuit->getNQubit());
    cout << "start topo sort" << endl;
    for (size_t i = 0; i < _gates.size(); i++)
    {
        cout << "_gates["<<i<<"]" << endl;
        for (size_t j = 0; j < _gates[i].size(); j++)
        {
            _gates[i][j]->printGate();
        }
    }
    for (size_t i = 0; i < _available.size(); i++)
    {
        cout << "_available["<<i<<"]" << endl;
        for (size_t j = 0; j < _available[i].size(); j++)
        {
            _available[i][j]->printGate();
        }
    }
    for (size_t i = 0; i < _availty.size(); i++)
    {
        cout << "_availty["<<i<<"]: "<<_availty[i]<<"  ";
    }
    cout << endl;
    int count = 0;
    while(any_of(_gates.begin(), _gates.end(), [](auto& p_g){return p_g.second.size();})){
        cout << "In to while: " << count << endl;
        for (auto& [q, gs] : _gates){
            cout << "q: " << q << " size: " << gs.size() <<endl; 
            while(gs.size()){
                cout << "Into small while loop" << endl;
                QCirGate* g = gs[0];
                g->printGate();
                if(g->getType() != GateType::CX && g->getType() != GateType::CZ){
                    cout << "case I" << endl;
                    Optimizer::_addGate2Circuit(circuit, g);
                    gs.erase(gs.begin());
                }else if(available_id.contains(g->getId())){
                    cout << "case II" << endl;
                    available_id.erase(g->getId());
                    size_t q2 = (q==g->getControl()._qubit)?g->getTarget()._qubit:g->getControl()._qubit;
                    _gates[q2].erase(find_if(_gates[q2].begin(), _gates[q2].end(), [&](QCirGate* _g){return g->getId()==_g->getId();}));
                    Optimizer::_addGate2Circuit(circuit, g);
                    gs.erase(gs.begin());
                }else{
                    cout << "case III" << endl;
                    bool type = (g->getType() == GateType::CZ || g->getControl()._qubit ==q);
                    vector<size_t> removed;
                    available_id.emplace(g->getId());

                    for(size_t i=1; i<gs.size();i++){
                        QCirGate* g = gs[i];
                        if((!type && isSingleRotateZ(g)) || (type && isSingleRotateX(g))){
                            Optimizer::_addGate2Circuit(circuit, g);
                            removed.emplace(removed.begin(), i);
                        }else if(g->getType() != GateType::CX && g->getType() != GateType::CZ){
                            break;
                        }else if((!type && (g->getType() == GateType::CZ || g->getControl()._qubit == q))||
                                (type && (g->getType() == GateType::CX || g->getTarget()._qubit == q))){
                            if(available_id.contains(g->getId())){
                                available_id.erase(g->getId());
                                size_t q2 = q==g->getControl()._qubit?g->getTarget()._qubit:g->getControl()._qubit;
                                _gates[q2].erase(find_if(_gates[q2].begin(), _gates[q2].end(), [&](QCirGate* _g){return g->getId()==_g->getId();}));
                                Optimizer::_addGate2Circuit(circuit, g);
                                removed.emplace(removed.begin(), i);
                            }else{
                                available_id.emplace(g->getId());
                            }
                        }else {
                            break;
                        }
                    }
                    for(size_t i: removed){
                        gs.erase(gs.begin()+i);
                    }
                    break;
                }
            }
        }
    }
    
}


/**
 * @brief Get the number of two qubit gate, H gate and not pauli gate in a circuit.
 *
 */
vector<size_t> Optimizer::stats(QCir* circuit){
    size_t two_qubit =0, had =0, non_pauli =0;
    vector<size_t> stats;
    vector<QCirGate*> gs = circuit->getTopoOrderdGates();
    for (auto g: gs){
        GateType type = g->getType();
        
        if(type == GateType::CX || type == GateType::CZ){
            two_qubit++;
        }else if(type == GateType::H){
            had++;
        }else if(type != GateType::X && type != GateType::Y && type != GateType::Z && g->getPhase() != Phase(1)){
            non_pauli++;
        }
    }
    stats.emplace_back(two_qubit);
    stats.emplace_back(had);
    stats.emplace_back(non_pauli);
    return stats;
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