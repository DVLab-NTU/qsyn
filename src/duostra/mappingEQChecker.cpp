/****************************************************************************
  FileName     [ mappingEQChecker.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class MappingEQChecker member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "mappingEQChecker.h"
using namespace std;

/**
 * @brief Construct a new Ext Checker:: Ext Checker object
 *
 * @param phy
 * @param log
 * @param dev
 * @param init
 * @param reverse check reversily if true
 */
MappingEQChecker::MappingEQChecker(QCir* phy, QCir* log, Device dev, std::vector<size_t> init, bool reverse) : _physical(phy), _logical(log), _device(dev), _reverse(reverse) {
    if (init.empty()) {
        auto placer = getPlacer();
        init = placer->placeAndAssign(_device);
    } else
        _device.place(init);
    _physical->updateTopoOrder();
    for (const auto& qubit : _logical->getQubits()) {
        _dependency[qubit->getId()] = _reverse ? qubit->getLast() : qubit->getFirst();
    }
}

/**
 * @brief Check physical circuit
 *
 * @return true
 * @return false
 */
bool MappingEQChecker::check() {
    vector<QCirGate*> executeOrder = _physical->getTopoOrderdGates();
    if (_reverse) reverse(executeOrder.begin(), executeOrder.end());  // NOTE - Now the order starts from back
    // NOTE - Traverse all physical gates, should match dependency of logical gate
    unordered_set<QCirGate*> swaps;
    for (const auto& phyGate : executeOrder) {
        if (swaps.contains(phyGate)) {
            continue;
        }
        GateType gateType = phyGate->getType();
        if (gateType == GateType::CX || gateType == GateType::CZ) {
            if (isSwap(phyGate)) {
                bool correct = executeSwap(phyGate, swaps);
                if (!correct) return false;
            } else {
                bool correct = executeDouble(phyGate);
                if (!correct) return false;
            }
        } else if (phyGate->getNQubit() > 1) {
            return false;
        } else {
            bool correct = executeSingle(phyGate);
            if (!correct) return false;
        }
    }
    // REVIEW - check remaining gates in logical are swaps
    checkRemain();
    // Should end with all qubits in _dependency = nullptr;
    return true;
}

/**
 * @brief Check the gate and its previous two gates constitute a swap
 *
 * @param candidate
 * @return true
 * @return false
 */
bool MappingEQChecker::isSwap(QCirGate* candidate) {
    if (candidate->getType() != GateType::CX) return false;
    QCirGate* q0Gate = getNext(candidate->getQubits()[0]);
    QCirGate* q1Gate = getNext(candidate->getQubits()[1]);

    if (q0Gate != q1Gate || q0Gate == nullptr || q1Gate == nullptr) return false;
    if (q0Gate->getType() != GateType::CX) return false;
    // q1gate == q0 gate
    if (candidate->getQubits()[0]._qubit != q1Gate->getQubits()[1]._qubit ||
        candidate->getQubits()[1]._qubit != q0Gate->getQubits()[0]._qubit) return false;

    candidate = q0Gate;
    q0Gate = getNext(candidate->getQubits()[0]);
    q1Gate = getNext(candidate->getQubits()[1]);

    if (q0Gate != q1Gate || q0Gate == nullptr || q1Gate == nullptr) return false;
    if (q0Gate->getType() != GateType::CX) return false;
    // q1gate == q0 gate
    if (candidate->getQubits()[0]._qubit != q1Gate->getQubits()[1]._qubit ||
        candidate->getQubits()[1]._qubit != q0Gate->getQubits()[0]._qubit) return false;

    // NOTE - If it is actually a gate in dependency, it can not be changed into swap
    size_t phyCtrlId = candidate->getQubits()[0]._qubit;
    size_t phyTargId = candidate->getQubits()[1]._qubit;

    QCirGate* logGate0 = _dependency[_device.getPhysicalQubit(phyCtrlId).getLogicalQubit()];
    QCirGate* logGate1 = _dependency[_device.getPhysicalQubit(phyTargId).getLogicalQubit()];
    if (logGate0 != logGate1 || logGate0 == nullptr)
        return true;
    else if (logGate0->getType() != GateType::CX)
        return true;
    else
        return false;
}

/**
 * @brief Execute swap gate
 *
 * @param first first gate of swap
 * @param swaps container to mark
 * @return true
 * @return false
 */
bool MappingEQChecker::executeSwap(QCirGate* first, unordered_set<QCirGate*>& swaps) {
    bool connect = _device.getPhysicalQubit(first->getQubits()[0]._qubit).isAdjacency(_device.getPhysicalQubit(first->getQubits()[1]._qubit));
    if (!connect) return false;

    QCirGate* next;
    swaps.emplace(first);
    next = getNext(first->getQubits()[0]);
    swaps.emplace(next);
    next = getNext(next->getQubits()[0]);
    swaps.emplace(next);
    _device.applySwapCheck(first->getQubits()[0]._qubit, first->getQubits()[1]._qubit);
    return true;
}

/**
 * @brief Execute single-qubit gate
 *
 * @param gate
 * @return true
 * @return false
 */
bool MappingEQChecker::executeSingle(QCirGate* gate) {
    assert(gate->getQubits()[0]._isTarget == true);
    size_t phyId = gate->getQubits()[0]._qubit;

    QCirGate* logical = _dependency[_device.getPhysicalQubit(phyId).getLogicalQubit()];
    if (logical == nullptr) {
        cout << "Error: corresponding logical gate of gate " << gate->getId() << " is nullptr!!" << endl;
        return false;
    }

    if (logical->getType() != gate->getType()) {
        cout << logical->getTypeStr() << "(" << logical->getId() << ") " << gate->getTypeStr() << endl;
        cout << "Error: type of gate " << gate->getId() << " mismatches!!" << endl;
        return false;
    }
    if (logical->getPhase() != gate->getPhase()) {
        cout << "Error: phase of gate " << gate->getId() << " mismatches!!" << endl;
        return false;
    }
    if (logical->getQubits()[0]._qubit != _device.getPhysicalQubit(phyId).getLogicalQubit()) {
        cout << "Error: target of gate " << gate->getId() << " mismatches!!" << endl;
        return false;
    }

    _dependency[logical->getQubits()[0]._qubit] = getNext(logical->getQubits()[0]);
    return true;
}

/**
 * @brief Execute double-qubit gate
 *
 * @param gate
 * @return true
 * @return false
 */
bool MappingEQChecker::executeDouble(QCirGate* gate) {
    assert(gate->getQubits()[0]._isTarget == false);
    assert(gate->getQubits()[1]._isTarget == true);
    size_t phyCtrlId = gate->getQubits()[0]._qubit;
    size_t phyTargId = gate->getQubits()[1]._qubit;

    if (_dependency[_device.getPhysicalQubit(phyCtrlId).getLogicalQubit()] != _dependency[_device.getPhysicalQubit(phyTargId).getLogicalQubit()]) {
        cout << "Error: gate " << gate->getId() << " violates dependency graph!!" << endl;
        return false;
    }
    QCirGate* logical = _dependency[_device.getPhysicalQubit(phyTargId).getLogicalQubit()];
    if (logical == nullptr) {
        cout << "Error: corresponding logical gate of gate " << gate->getId() << " is nullptr!!" << endl;
        return false;
    }

    if (logical->getType() != gate->getType()) {
        cout << "Error: type of gate " << gate->getId() << " mismatches!!" << endl;
        return false;
    }
    if (logical->getPhase() != gate->getPhase()) {
        cout << "Error: phase of gate " << gate->getId() << " mismatches!!" << endl;
        return false;
    }
    if (logical->getQubits()[0]._qubit != _device.getPhysicalQubit(phyCtrlId).getLogicalQubit()) {
        cout << "Error: control of gate " << gate->getId() << " mismatches!!" << endl;
        return false;
    }
    if (logical->getQubits()[1]._qubit != _device.getPhysicalQubit(phyTargId).getLogicalQubit()) {
        cout << "Error: target of gate " << gate->getId() << " mismatches!!" << endl;
        return false;
    }

    bool connect = _device.getPhysicalQubit(phyCtrlId).isAdjacency(_device.getPhysicalQubit(phyTargId));
    if (!connect) return false;

    _dependency[logical->getQubits()[0]._qubit] = getNext(logical->getQubits()[0]);
    _dependency[logical->getQubits()[1]._qubit] = getNext(logical->getQubits()[1]);

    return true;
}

/**
 * @brief Check gates in remaining logical circuit are all swaps
 *
 * @return true
 * @return false
 */
bool MappingEQChecker::checkRemain() {
    for (const auto& [q, g] : _dependency) {
        if (g != nullptr) {
            cout << "Note: qubit " << q << " has gates remaining" << endl;
        }
    }
    return true;
}

/**
 * @brief Get next gate
 *
 * @param info
 * @return QCirGate*
 */
QCirGate* MappingEQChecker::getNext(const BitInfo& info) {
    if (_reverse)
        return info._parent;
    else
        return info._child;
}