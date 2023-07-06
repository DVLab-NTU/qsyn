/****************************************************************************
  FileName     [ trivial_optimization.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Implement the trivial optimization ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <assert.h>  // for assert

#include "optimizer.h"

using namespace std;

extern size_t verbose;

/**
 * @brief Trivial optimization
 *
 * @return QCir*
 */
QCir* Optimizer::trivial_optimization() {
    QCir* temp = new QCir(-1);
    temp->addQubit(_circuit->getNQubit());
    vector<QCirGate*> gateList = _circuit->getTopoOrderdGates();
    for (auto gate : gateList) {
        vector<QCirGate*> LastLayer = getFirstLayerGates(temp, true);
        size_t qubit = gate->getTarget()._qubit;
        if (LastLayer[qubit] == nullptr) {
            Optimizer::_addGate2Circuit(temp, gate);
            continue;
        }
        QCirGate* previousGate = LastLayer[qubit];
        if (isDoubleQubitGate(gate)) {
            size_t q2 = gate->getTarget()._qubit;
            if (previousGate->getId() != LastLayer[q2]->getId()) {
                // 2-qubit gate do not match up
                Optimizer::_addGate2Circuit(temp, gate);
                continue;
            }
            CheckDoubleGate(temp, previousGate, gate);
        } else if (isSingleRotateZ(gate) && isSingleRotateZ(previousGate)) {
            FuseZPhase(temp, previousGate, gate);
        } else if (gate->getType() == previousGate->getType()) {
            temp->removeGate(previousGate->getId());
        } else {
            Optimizer::_addGate2Circuit(temp, gate);
        }
    }
    _circuit = temp;
    return _circuit;
}

/**
 * @brief Get the first layer of the circuit (nullptr if the qubit is empty at the layer)
 * @param QC: the input circuit
 * @param fromLast: Get the last layer
 *
 * @return vector<QCirGate*> with size = circuit->getNqubit()
 */
vector<QCirGate*> Optimizer::getFirstLayerGates(QCir* QC, bool fromLast) {
    vector<QCirGate*> gateList = QC->updateTopoOrder();
    if (fromLast) reverse(gateList.begin(), gateList.end());
    vector<QCirGate*> result;
    vector<bool> blocked;
    for (size_t i = 0; i < QC->getNQubit(); i++) {
        result.emplace_back(nullptr);
        blocked.emplace_back(false);
    }

    for (auto gate : gateList) {
        vector<BitInfo> qubits = gate->getQubits();
        bool gateIsNotBlocked = all_of(qubits.begin(), qubits.end(), [&](BitInfo qubit) { return blocked[qubit._qubit] == false; });
        for (auto q : qubits) {
            size_t qubit = q._qubit;
            if (gateIsNotBlocked) result[qubit] = gate;
            blocked[qubit] = true;
        }
        if (all_of(blocked.begin(), blocked.end(), [](bool block) { return block; })) break;
    }

    return result;
}

/**
 * @brief Fuse the incoming ZPhase gate with the last layer in circuit
 * @param QC: the circuit
 * @param previousGate: previous gate
 * @param gate: the incoming gate
 *
 * @return modified circuit
 */
void Optimizer::FuseZPhase(QCir* QC, QCirGate* previousGate, QCirGate* gate) {
    Phase p = previousGate->getPhase() + gate->getPhase();
    if (p == Phase(0)) {
        QC->removeGate(previousGate->getId());
        return;
    }
    if (previousGate->getType() == GateType::P)
        previousGate->setRotatePhase(p);
    else {
        vector<size_t> qubit_list;
        qubit_list.emplace_back(previousGate->getTarget()._qubit);
        QC->removeGate(previousGate->getId());
        QC->addGate("p", qubit_list, p, true);
    }
}

/**
 * @brief Cancel if CX-CX / CZ-CZ, otherwise append it.
 * @param QC: the circuit
 * @param previousGate: previous gate
 * @param gate: the incoming gate
 *
 * @return modified circuit
 */
void Optimizer::CheckDoubleGate(QCir* QC, QCirGate* previousGate, QCirGate* gate) {
    if (previousGate->getType() != gate->getType()) {
        Optimizer::_addGate2Circuit(QC, gate);
        return;
    }
    if (previousGate->getType() == GateType::CZ || previousGate->getControl()._qubit == gate->getControl()._qubit)
        QC->removeGate(previousGate->getId());
    else
        Optimizer::_addGate2Circuit(QC, gate);
}
