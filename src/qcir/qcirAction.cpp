/****************************************************************************
  FileName     [ qcirAction.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <cstddef>
#include <stack>

#include "qcir/qcir.hpp"
#include "qcir/qcirGate.hpp"
#include "qcir/qcirQubit.hpp"
#include "zx/zxGraph.hpp"

using namespace std;
extern size_t verbose;

/**
 * @brief Append the target to current QCir
 *
 * @param target
 * @return QCir*
 */
QCir* QCir::compose(QCir const& target) {
    QCir copiedQCir{target};
    vector<QCirQubit*> targQubits = copiedQCir.getQubits();
    for (auto& qubit : targQubits) {
        if (getQubit(qubit->getId()) == nullptr)
            insertSingleQubit(qubit->getId());
    }
    copiedQCir.updateTopoOrder();
    for (auto& targGate : copiedQCir.getTopoOrderdGates()) {
        vector<size_t> bits;
        for (const auto& b : targGate->getQubits()) {
            bits.emplace_back(b._qubit);
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
QCir* QCir::tensorProduct(QCir const& target) {
    QCir copiedQCir{target};

    unordered_map<size_t, QCirQubit*> oldQ2NewQ;
    vector<QCirQubit*> targQubits = copiedQCir.getQubits();
    for (auto& qubit : targQubits) {
        oldQ2NewQ[qubit->getId()] = addSingleQubit();
    }
    copiedQCir.updateTopoOrder();
    for (auto& targGate : copiedQCir.getTopoOrderdGates()) {
        vector<size_t> bits;
        for (const auto& b : targGate->getQubits()) {
            bits.emplace_back(oldQ2NewQ[b._qubit]->getId());
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
void QCir::DFS(QCirGate* currentGate) const {
    stack<pair<bool, QCirGate*>> dfs;

    if (!currentGate->isVisited(_globalDFScounter)) {
        dfs.push(make_pair(false, currentGate));
    }
    while (!dfs.empty()) {
        pair<bool, QCirGate*> node = dfs.top();
        dfs.pop();
        if (node.first) {
            _topoOrder.emplace_back(node.second);
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
const vector<QCirGate*>& QCir::updateTopoOrder() const {
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
            if (Info[i]._parent == nullptr)
                continue;
            if (Info[i]._parent->getTime() > max_time)
                max_time = Info[i]._parent->getTime();
        }
        currentGate->setTime(max_time + currentGate->getDelay());
    };
    topoTraverse(Lambda);
}

/**
 * @brief Print ZXGraph of gate following the topological order
 */
void QCir::printZXTopoOrder() {
    auto Lambda = [](QCirGate* gate) {
        cout << "Gate " << gate->getId() << " (" << gate->getTypeStr() << ")" << endl;
        ZXGraph tmp = gate->getZXform();
        tmp.printVertices();
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

    _gateId = 0;
    _ZXNodeId = 0;
    _qubitId = 0;
    _dirty = true;
    _globalDFScounter = 1;
}