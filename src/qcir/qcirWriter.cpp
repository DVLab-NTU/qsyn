/****************************************************************************
  FileName     [ qcirWriter.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define QCir Writer functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "qcir.h"

using namespace std;

/// @brief Write QASM
/// @param filename
/// @return true if successfully write
/// @return false if path or file not found
bool QCir::writeQASM(string filename) {
    updateTopoOrder();
    fstream file;
    file.open(filename, std::fstream::out);
    if (!file)
        return false;
    file << "OPENQASM 2.0;\n";
    file << "include \"qelib1.inc\";\n";
    file << "qreg q[" << _qubits.size() << "];\n";
    for (size_t i = 0; i < _topoOrder.size(); i++) {
        QCirGate* curGate = _topoOrder[i];
        file << curGate->getTypeStr() << " ";
        vector<BitInfo> pins = curGate->getQubits();
        for (size_t qb = 0; qb < pins.size(); qb++) {
            file << "q[" << pins[qb]._qubit << "]";
            if (qb == pins.size() - 1)
                file << ";\n";
            else
                file << ",";
        }
    }
    return true;
}