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

#include "./basic_gate_type.hpp"
#include "./operation.hpp"
#include "./qcir.hpp"
#include "./qcir_gate.hpp"
#include "./qcir_io.hpp"
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
bool QCir::write_qasm(std::filesystem::path const& filepath) const {
    std::ofstream ofs(filepath);
    if (!ofs) {
        spdlog::error("Cannot open file {}", filepath.string());
        return false;
    }
    ofs << to_qasm(*this);

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
bool QCir::draw(QCirDrawerType drawer, std::filesystem::path const& output_path, float scale) const {
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
    if (!output_path.string().empty()) {
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
    // auto cmd = fmt::format("python3 {} -input {} -drawer {} -scale {}", path_to_script, tmp_qasm.string(), drawer, scale);

    if (!output_path.string().empty()) {
        cmd += " -output " + output_path.string();
    }

    return system(cmd.c_str()) == 0;
}

std::string to_qasm(QCir const& qcir) {
    std::string qasm = "OPENQASM 2.0;\n";
    qasm += "include \"qelib1.inc\";\n";
    qasm += fmt::format("qreg q[{}];\n", qcir.get_num_qubits());

    // Add classical register if there are classical bits
    if (qcir.get_num_classical_bits() > 0) {
        qasm += fmt::format("creg c[{}];\n", qcir.get_num_classical_bits());
    }

    // Emit state preparation gates for qubits with non-|0⟩ initial states.
    // QASM always starts from |0⟩, so we encode other states as gates.
    for (size_t i = 0; i < qcir.get_num_qubits(); ++i) {
        switch (qcir.get_initial_state(i)) {
            case QubitInitialState::zero:
                break;  // default — no gate needed
            case QubitInitialState::one:
                qasm += fmt::format("x q[{}];\n", i);
                break;
            case QubitInitialState::plus:
                qasm += fmt::format("h q[{}];\n", i);
                break;
            case QubitInitialState::minus:
                // |−⟩ = H·X|0⟩ : apply X first, then H
                qasm += fmt::format("x q[{}];\n", i);
                qasm += fmt::format("h q[{}];\n", i);
                break;
        }
    }

    for (auto const* gate : qcir.get_gates()) {
        using namespace std::literals;
        auto const qubits = gate->get_qubits();
        auto repr         = gate->get_operation().get_repr();
        
        // Handle measurement gates — emit basis-appropriate QASM.
        // X-basis: prefix with an H gate so the qubit is rotated before
        // the standard Z-basis measurement instruction.
        if (auto const meas = gate->get_operation().get_underlying_if<MeasurementGate>()) {
            auto const cbit = gate->has_classical_bits() && !gate->get_classical_bits().empty()
                                  ? gate->get_classical_bits()[0]
                                  : qubits[0];  // fallback: use qubit index
            if (meas->is_x_basis()) {
                qasm += fmt::format("h q[{}];\n", qubits[0]);
            }
            qasm += fmt::format("measure q[{}] -> c[{}];\n", qubits[0], cbit);
            continue;
        }

        // Handle reset gates.
        if (gate->get_operation().get_type() == "reset") {
            qasm += fmt::format("reset q[{}];\n", qubits[0]);
            continue;
        }
        
        // Handle if-else gates independently
        if (repr.find("if") == 0) {
            // If-else gates need qubit targets appended
            std::string qubit_str;
            for (size_t i = 0; i < qubits.size(); ++i) {
                if (i > 0) qubit_str += ", ";
                qubit_str += fmt::format("q[{}]", qubits[i]);
            }
            qasm += fmt::format("{} {};\n", repr, qubit_str);
            continue;
        }
        
        // if encountering "π", replace it with "pi"
        size_t pos = 0;
        while ((pos = repr.find("π"s, pos)) != std::string::npos) {
            if (pos == 0 || !std::isdigit(repr[pos - 1])) {
                repr.replace(pos, "π"s.size(), "pi");
            } else {
                repr.replace(pos, "π"s.size(), "*pi");
            }
        }

        // Build qubit string manually to avoid fmt::join view issues
        std::string qubit_str;
        for (size_t i = 0; i < qubits.size(); ++i) {
            if (i > 0) qubit_str += ", ";
            qubit_str += fmt::format("q[{}]", qubits[i]);
        }
        qasm += fmt::format("{} {};\n", repr, qubit_str);
    }
    return qasm;
}

}  // namespace qsyn::qcir
