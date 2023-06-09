/****************************************************************************
  FileName     [ qcirAction.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>  // for assert
#include <cstddef>  // for NULL, size_t
#include <stack>

#include "qcir.h"      // for QCir
#include "qcirGate.h"  // for QCirGate
#include "zxGraph.h"

using namespace std;
extern size_t verbose;

/**
 * @brief Copy a circuit
 *
 * @return QCir*
 */
QCir* QCir::copy() {
    updateTopoOrder();
    QCir* newCircuit = new QCir(0);
    unordered_map<QCirQubit*, QCirQubit*> oldQ2newQ;
    unordered_map<QCirGate*, QCirGate*> oldG2newG;
    newCircuit->addQubit(_qubits.size());

    size_t biggestQubit = 0;
    size_t biggestGate = 0;

    for (size_t i = 0; i < _qubits.size(); i++) {
        oldQ2newQ[_qubits[i]] = newCircuit->getQubits()[i];
        // NOTE - Update Id
        if (_qubits[i]->getId() > biggestQubit) biggestQubit = _qubits[i]->getId();
        oldQ2newQ[_qubits[i]]->setId(_qubits[i]->getId());
    }

    for (const auto& gate : _topoOrder) {
        string type = gate->getTypeStr();
        vector<size_t> bits;
        for (const auto& b : gate->getQubits()) {
            bits.push_back(b._qubit);
        }
        oldG2newG[gate] = newCircuit->addGate(gate->getTypeStr(), bits, gate->getPhase(), true);
    }

    // NOTE - Update Id
    for (const auto& [oldG, newG] : oldG2newG) {
        if (oldG->getId() > biggestGate) biggestGate = oldG->getId();
        newG->setId(oldG->getId());
    }
    newCircuit->setNextGateId(biggestGate + 1);
    newCircuit->setNextQubitId(biggestQubit + 1);
    return newCircuit;
}

/**
 * @brief Append the target to current QCir
 *
 * @param target
 * @return QCir*
 */
QCir* QCir::compose(QCir* target) {
    QCir* copiedQCir = target->copy();
    vector<QCirQubit*> targQubits = copiedQCir->getQubits();
    for (auto& qubit : targQubits) {
        if (getQubit(qubit->getId()) == NULL)
            insertSingleQubit(qubit->getId());
    }
    copiedQCir->updateTopoOrder();
    for (auto& targGate : copiedQCir->getTopoOrderdGates()) {
        vector<size_t> bits;
        for (const auto& b : targGate->getQubits()) {
            bits.push_back(b._qubit);
        }
        addGate(targGate->getTypeStr(), bits, targGate->getPhase(), true);
    }
    return this;
}

/**
 * @brief Tensor the target to current tensor of QCir
 *
 * @param target
 * @return QCir*
 */
QCir* QCir::tensorProduct(QCir* target) {
    QCir* copiedQCir = target->copy();

    unordered_map<size_t, QCirQubit*> oldQ2NewQ;
    vector<QCirQubit*> targQubits = copiedQCir->getQubits();
    for (auto& qubit : targQubits) {
        oldQ2NewQ[qubit->getId()] = addSingleQubit();
    }
    copiedQCir->updateTopoOrder();
    for (auto& targGate : copiedQCir->getTopoOrderdGates()) {
        vector<size_t> bits;
        for (const auto& b : targGate->getQubits()) {
            bits.push_back(oldQ2NewQ[b._qubit]->getId());
        }
        addGate(targGate->getTypeStr(), bits, targGate->getPhase(), true);
    }
    return this;
}

/**
 * @brief Perform DFS from currentGate
 *
 * @param currentGate the gate to start DFS
 */
void QCir::DFS(QCirGate* currentGate) {
    stack<pair<bool, QCirGate*>> dfs;

    if (!currentGate->isVisited(_globalDFScounter)) {
        dfs.push(make_pair(false, currentGate));
    }
    while (!dfs.empty()) {
        pair<bool, QCirGate*> node = dfs.top();
        dfs.pop();
        if (node.first) {
            _topoOrder.push_back(node.second);
            continue;
        }
        if (node.second->isVisited(_globalDFScounter)) {
            continue;
        }
        node.second->setVisited(_globalDFScounter);
        dfs.push(make_pair(true, node.second));

        for (const auto& Info : node.second->getQubits()) {
            if ((Info)._child != nullptr) {
                if (!((Info)._child->isVisited(_globalDFScounter))) {
                    dfs.push(make_pair(false, (Info)._child));
                }
            }
        }
    }
}

/**
 * @brief Update topological order
 *
 * @return const vector<QCirGate*>&
 */
const vector<QCirGate*>& QCir::updateTopoOrder() {
    _topoOrder.clear();
    _globalDFScounter++;
    QCirGate* dummy = new HGate(-1);
    for (size_t i = 0; i < _qubits.size(); i++) {
        dummy->addDummyChild(_qubits[i]->getFirst());
    }
    DFS(dummy);
    _topoOrder.pop_back();  // pop dummy
    reverse(_topoOrder.begin(), _topoOrder.end());
    assert(_topoOrder.size() == _qgates.size());
    delete dummy;

    return _topoOrder;
}

/**
 * @brief Print topological order
 */
bool QCir::printTopoOrder() {
    auto testLambda = [](QCirGate* G) { cout << G->getId() << endl; };
    topoTraverse(testLambda);
    return true;
}

/**
 * @brief Update execution time of gates
 */
void QCir::updateGateTime() {
    updateTopoOrder();
    auto Lambda = [](QCirGate* currentGate) {
        vector<BitInfo> Info = currentGate->getQubits();
        size_t max_time = 0;
        for (size_t i = 0; i < Info.size(); i++) {
            if (Info[i]._parent == NULL)
                continue;
            if (Info[i]._parent->getTime() > max_time)
                max_time = Info[i]._parent->getTime();
        }
        currentGate->setTime(max_time + currentGate->getDelay());
    };
    topoTraverse(Lambda);
}

/**
 * @brief Print ZX-graph of gate following the topological order
 */
void QCir::printZXTopoOrder() {
    auto Lambda = [](QCirGate* G) {
        cout << "Gate " << G->getId() << " (" << G->getTypeStr() << ")" << endl;
        ZXGraph* tmp = G->getZXform();
        tmp->printVertices();
    };
    topoTraverse(Lambda);
}

/**
 * @brief Reset QCir
 *
 */
void QCir::reset() {
    _qgates.clear();
    _qubits.clear();
    _topoOrder.clear();
    _ZXGraphList.clear();
    _qubit2pin.clear();

    _gateId = 0;
    _ZXNodeId = 0;
    _qubitId = 0;
    _dirty = true;
    _globalDFScounter = 1;
    _tensor = nullptr;
}