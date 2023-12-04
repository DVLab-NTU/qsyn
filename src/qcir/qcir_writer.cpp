/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Writer functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/ostream.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "util/sysdep.hpp"
#include "util/tmp_files.hpp"

namespace qsyn::qcir {

/**
 * @brief Write QASM
 *
 * @param filename
 * @return true if successfully write
 * @return false if path or file not found
 */
bool QCir::write_qasm(std::string const& filename) {
    update_topological_order();
    std::ofstream file;
    file.open(filename, std::fstream::out);
    if (!file)
        return false;
    fmt::println(file, "OPENQASM 2.0;");
    fmt::println(file, "include \"qelib1.inc\";");
    fmt::println(file, "qreg q[{}];", _qubits.size());

    for (size_t i = 0; i < _topological_order.size(); i++) {
        QCirGate* cur_gate          = _topological_order[i];
        auto type_str               = cur_gate->get_type_str();
        std::vector<QubitInfo> pins = cur_gate->get_qubits();
        auto is_clifford_t          = cur_gate->get_phase().denominator() == 1 || cur_gate->get_phase().denominator() == 2 || cur_gate->get_phase() == Phase(1, 4) || cur_gate->get_phase() == Phase(-1, 4);
        fmt::println(file, "{}{} {};",
                     cur_gate->get_type_str(),
                     is_clifford_t ? "" : fmt::format("({})", cur_gate->get_phase().get_ascii_string()),
                     fmt::join(pins | std::views::transform([](QubitInfo const& pin) { return fmt::format("q[{}]", pin._qubit); }), ", "));
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

    if (!dv::python_package_exists("qiskit")) {
        spdlog::error("qiskit is not installed in the system!!");
        spdlog::error("Please install qiskit first or check if you have used the correct python environment!!");
        return false;
    }

    if (drawer == QCirDrawerType::mpl || drawer == QCirDrawerType::latex) {
        if (dv::python_package_exists("pylatexenc") == 0) {
            spdlog::error("pylatexenc is not installed in the system!!");
            spdlog::error("Please install pylatexenc first or check if you have used the correct python environment!!");
            return false;
        }
    }

    if (drawer == QCirDrawerType::latex) {
        if (!dvlab::utils::pdflatex_exists()) {
            spdlog::error("pdflatex is not installed in the system. Please install pdflatex first!!");
            return false;
        }
    }

    // check if output_path is valid
    if (output_path.string().size()) {
        if (!std::ofstream{output_path}) {
            spdlog::error("Cannot open file {}", output_path.string());
            return false;
        }
    }

    dv::TmpDir const tmp_dir;
    fs::path const tmp_qasm = tmp_dir.path() / "tmp.qasm";

    this->write_qasm(tmp_qasm.string());

    auto const path_to_script = "scripts/qccdraw_qiskit_interface.py";

    auto cmd = fmt::format("python3 {} -input {} -drawer {} -scale {}", path_to_script, tmp_qasm.string(), drawer, scale);

    if (output_path.string().size()) {
        cmd += " -output " + output_path.string();
    }

    return system(cmd.c_str()) == 0;
}

}  // namespace qsyn::qcir
