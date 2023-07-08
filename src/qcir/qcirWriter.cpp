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
#include "tmpFiles.h"

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
bool QCir::draw(std::string const& drawer, std::string const& outputPath, float scale) {
    namespace dv = dvlab_utils;
    namespace fs = std::filesystem;

    dv::TmpDir tmpDir;
    fs::path tmpQASM = tmpDir.path() / "tmp.qasm";

    this->writeQASM(tmpQASM.string());

    string pathToScript = "scripts/qccdraw_qiskit_interface.py";

    string cmd = "python3 " + pathToScript + " -input " + tmpQASM.string() + " -drawer " + drawer;
    +" -scale " + to_string(scale);

    if (outputPath.size()) {
        cmd += " -output " + outputPath;
    }

    int status = system(cmd.c_str());

    return status == 0;
}