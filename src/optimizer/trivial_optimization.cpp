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
QCir* Optimizer::trivial_optimization(){
    vector<QCirGate*> FL = getFirstLayerGates(); 
    for(auto g: FL){
        if (g!= nullptr) g->printGate();
    }
    cout << "FromLast" << endl;
    FL = getFirstLayerGates(true); 
    for(auto g: FL){
        if (g!= nullptr) g->printGate();
    }
    cout << "Not implement yet" << endl;
    return nullptr;
}






/**
 * @brief Get the first layer of the circuit (nullptr if the qubit is empty at the layer)
 * @param fromLast: Get the last layer
 *
 * @return vector<QCirGate*> with size = circuit->getNqubit()
 */
vector<QCirGate*> Optimizer::getFirstLayerGates(bool fromLast) {
    vector<QCirGate*> gateList = _circuit->getTopoOrderdGates();
    if (fromLast) reverse(gateList.begin(), gateList.end());
    vector<QCirGate*> result;
    vector<bool> blocked;
    for (size_t i = 0; i < _circuit->getNQubit(); i++) {
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
        if(all_of(blocked.begin(), blocked.end(), [](bool block){ return block;})) break;
    }

    return result;
}