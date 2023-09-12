/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Writer functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <filesystem>
#include <fstream>
#include <string>

#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "util/tmp_files.hpp"

using namespace std;

/**
 * @brief Write QASM
 *
 * @param filename
 * @return true if successfully write
 * @return false if path or file not found
 */
bool QCir::write_qasm(string const& filename) {
    update_topological_order();
    fstream file;
    file.open(filename, std::fstream::out);
    if (!file)
        return false;
    file << "OPENQASM 2.0;\n";
    file << "include \"qelib1.inc\";\n";
    file << "qreg q[" << _qubits.size() << "];\n";
    for (size_t i = 0; i < _topological_order.size(); i++) {
        QCirGate* cur_gate = _topological_order[i];
        file << cur_gate->get_type_str();
        if (cur_gate->get_type() == GateType::mcp || cur_gate->get_type() == GateType::mcpx || cur_gate->get_type() == GateType::mcpy || cur_gate->get_type() == GateType::mcrz || cur_gate->get_type() == GateType::mcrx || cur_gate->get_type() == GateType::mcry || cur_gate->get_type() == GateType::p || cur_gate->get_type() == GateType::px || cur_gate->get_type() == GateType::py || cur_gate->get_type() == GateType::rz || cur_gate->get_type() == GateType::rx || cur_gate->get_type() == GateType::ry) {
            file << "(" << cur_gate->get_phase().get_ascii_string() << ")"
                 << " ";
        } else {
            file << " ";
        }
        vector<QubitInfo> pins = cur_gate->get_qubits();
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
 * @brief Draw a quantum circuit onto terminal or into a file using Qiskit
 *
 * @param drawer `text`, `mpl`, `latex`, or `latex_source`. Here `mpl` means Python's MatPlotLib
 * @param outputPath If specified, output to this path; else output to terminal. Must be specified for `mpl` and `latex` drawer.
 * @return true if succeeds drawing;
 * @return false if not.
 */
bool QCir::draw(QCirDrawerType drawer, std::filesystem::path const& output_path, float scale) {
    namespace dv = dvlab::utils;
    namespace fs = std::filesystem;

    // check if output_path is valid
    if (output_path.string().size()) {
        std::ofstream fout{output_path};
        if (!fout) {
            std::cerr << "Cannot open file " << output_path << std::endl;
            return false;
        }
    }

    dv::TmpDir tmp_dir;
    fs::path tmp_qasm = tmp_dir.path() / "tmp.qasm";

    this->write_qasm(tmp_qasm.string());

    string path_to_script = "scripts/qccdraw_qiskit_interface.py";

    string cmd = "python3 " + path_to_script + " -input " + tmp_qasm.string() + " -drawer " + fmt::format("{}", drawer);
    +" -scale " + to_string(scale);

    if (output_path.string().size()) {
        cmd += " -output " + output_path.string();
    }

    int status = system(cmd.c_str());

    return status == 0;
}