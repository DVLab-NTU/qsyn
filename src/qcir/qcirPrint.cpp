/****************************************************************************
  FileName     [ qcirPrint.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Print functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <cstdlib>
#include <string>

#include "./qcir.hpp"
#include "./qcirGate.hpp"
#include "./qcirQubit.hpp"

using namespace std;
extern size_t verbose;

/**
 * @brief Print QCir Gates
 */
void QCir::printGates() {
    if (_dirty)
        updateGateTime();
    cout << "Listed by gate ID" << endl;
    for (size_t i = 0; i < _qgates.size(); i++) {
        _qgates[i]->printGate();
    }
}

/**
 * @brief Print Depth of QCir
 *
 */
void QCir::printDepth() {
    cout << "Depth       : " << getDepth() << endl;
}

/**
 * @brief Print QCir
 */
void QCir::printCircuit() {
    cout << "QCir " << _id << "( "
         << _qubits.size() << " qubits, "
         << _qgates.size() << " gates)\n";
}

/**
 * @brief Print QCir Summary
 */
void QCir::printSummary() {
    printCircuit();
    countGate();
    printDepth();
}

/**
 * @brief Print Qubits
 */
void QCir::printQubits() {
    if (_dirty)
        updateGateTime();

    for (size_t i = 0; i < _qubits.size(); i++)
        _qubits[i]->printBitLine();
}

/**
 * @brief Print Gate information
 *
 * @param id
 * @param showTime if true, show the time
 */
bool QCir::printGateInfo(size_t id, bool showTime) {
    if (getGate(id) != nullptr) {
        if (showTime && _dirty)
            updateGateTime();
        getGate(id)->printGateInfo(showTime);
        return true;
    } else {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    }
}

void QCir::printCirInfo() {
    vector<int> info = countGate(false, false);
    cout << "QCir " << _id << "( "
         << _qgates.size() << " gates, "
         << info[1] << " 2-qubits gates, "
         << info[2] << " T-gates, "
         << getDepth() << " depths)\n";
}