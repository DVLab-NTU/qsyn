/****************************************************************************
  FileName     [ writer.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir writer functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <vector>
#include <unordered_map>
#include <string>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cassert>
#include "qcir.h"

using namespace std;

bool QCir::writeQASM(string filename){
    updateTopoOrder();
    fstream file;
    file.open(filename, std::fstream::out);
    if(!file)
        return false;
    file << "OPENQASM 2.0;\n";
    file << "include \"qelib1.inc\";\n";
    file << "qreg q[" << _qubits.size() << "];\n";
    for(size_t i=0; i< _topoOrder.size(); i++){
        QCirGate* curGate = _topoOrder[i];
        file << curGate->getTypeStr()<< " ";
        vector<BitInfo> pins = curGate->getQubits();
        for(size_t qb = 0; qb < pins.size(); qb++){
            file << "q[" << pins[qb]._qubit << "]";
            if(qb == pins.size()-1) file << ";\n";
            else file << ",";
        }
    }
    return true;
}