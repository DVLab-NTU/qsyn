/****************************************************************************
  FileName     [ qcirWriter.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Writer functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <filesystem>
#include <fstream>  // for fstream
#include <string>   // for string

#include "qcir.h"  // for QCir

using namespace std;

/**
 * @brief Write QASM
 *
 * @param filename
 * @return true if successfully write
 * @return false if path or file not found
 */
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
        file << curGate->getTypeStr();
        if (curGate->getType() == GateType::MCP || curGate->getType() == GateType::MCPX || curGate->getType() == GateType::MCPY || curGate->getType() == GateType::MCRZ || curGate->getType() == GateType::MCRX || curGate->getType() == GateType::MCRY || curGate->getType() == GateType::P || curGate->getType() == GateType::PX || curGate->getType() == GateType::PY || curGate->getType() == GateType::RZ || curGate->getType() == GateType::RX || curGate->getType() == GateType::RY) {
            file << "(" << curGate->getPhase().getAsciiString() << ")"
                 << " ";
        } else {
            file << " ";
        }
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

/**
 * @brief Draw a quantum circuit onto console or into a file using Qiskit
 *
 * @param drawer `text`, `mpl`, `latex`, or `latex_source`. Here `mpl` means Python's MatPlotLib
 * @param outputPath If specified, output to this path; else output to console. Must be specified for `mpl` and `latex` drawer.
 * @return true if succeeds drawing;
 * @return false if not.
 */
bool QCir::draw(std::string const& drawer, std::string const& outputPath) const {
    // TODO - t5 - Draw quantum circuits by calling qiskit

    // create a temporary directory with a name starting with /tmp/
    // We've wrapped the function `createTempDir(string prefix)` for you.
    string tmpDirName = createTempDir("/tmp/");  // returns something like /tmp/XXXXXX

    // You're welcome to change the prefix of the folder if you want, but it is advised that
    // you create it in /tmp/. This is where MacOS and Linux keeps all the temporary files.

    // write the QCir to a QASM file into this folder. This can be done with QCir::writeQASM(...)

    // perform your system calls. Read the output QASM file with the python script,
    // output using the respective `drawer` to the console or to a file

    // remove the directory and all temporary files in it
    filesystem::remove_all(tmpDirName);

    return true;
}