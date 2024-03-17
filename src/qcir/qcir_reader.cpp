/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Reader functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <fstream>
#include <string>

#include "qcir/gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/dvlab_string.hpp"
#include "util/phase.hpp"

namespace qsyn::qcir {

/**
 * @brief Read QCir file
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::read_qcir_file(std::filesystem::path const& filepath) {
    auto const extension = filepath.extension();

    if (extension == ".qasm")
        return read_qasm(filepath);
    else if (extension == ".qc")
        return read_qc(filepath);
    else {
        spdlog::error("File format \"{}\" is not supported!!", extension);
        return false;
    }
}

/**
 * @brief Read QASM
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::read_qasm(std::filesystem::path const& filepath) {
    using dvlab::str::str_get_token;
    // read file and open
    _procedures.clear();
    std::fstream qasm_file{filepath};
    if (!qasm_file.is_open()) {
        spdlog::error("Cannot open the QASM file \"{}\"!!", filepath);
        return false;
    }
    std::string str;
    for (int i = 0; i < 6; i++) {
        // OPENQASM 2.0;
        // include "qelib1.inc";
        // qreg q[int];
        qasm_file >> str;
    }

    auto const nqubit = stoul(str.substr(str.find('[') + 1, str.size() - str.find('[') - 3));
    add_qubits(nqubit);
    getline(qasm_file, str);
    while (getline(qasm_file, str)) {
        str = dvlab::str::trim_spaces(dvlab::str::trim_comments(str));
        if (str.empty()) continue;
        std::string type;
        auto const type_end   = str_get_token(str, type);
        std::string phase_str = "0";
        if (str_get_token(str, phase_str, 0, '(') != std::string::npos) {
            auto const stop = str_get_token(str, type, 0, '(');
            str_get_token(str, phase_str, stop + 1, ')');
        } else
            phase_str = "0";
        if (type == "creg" || type == "qreg" || type.empty()) {
            continue;
        }
        QubitIdList qubit_ids;
        std::string token;
        std::string qubit_id_str;
        size_t n = str_get_token(str, token, type_end, ',');
        while (!token.empty()) {
            str_get_token(token, qubit_id_str, str_get_token(token, qubit_id_str, 0, '[') + 1, ']');
            auto qubit_id_num = dvlab::str::from_string<unsigned>(qubit_id_str);
            if (!qubit_id_num.has_value() || qubit_id_num >= nqubit) {
                spdlog::error("invalid qubit id on line {}!!", str);
                return false;
            }
            qubit_ids.emplace_back(qubit_id_num.value());
            n = str_get_token(str, token, n, ',');
        }

        if (auto op = str_to_operation(type); op.has_value()) {
            append(*op, qubit_ids);
            continue;
        }

        auto phase = dvlab::Phase::from_string(phase_str);
        if (!phase.has_value()) {
            spdlog::error("invalid phase on line {}!!", str);
            return false;
        }

        if (auto op = str_to_operation(type, {*phase}); op.has_value()) {
            append(*op, qubit_ids);
            continue;
        }
    }
    return true;
}

/**
 * @brief Read QC
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::read_qc(std::filesystem::path const& filepath) {
    using dvlab::str::str_get_token;
    // read file and open
    std::fstream qc_file{filepath};
    if (!qc_file.is_open()) {
        spdlog::error("Cannot open the QC file \"{}\"!!", filepath);
        return false;
    }

    // ex: qubit_labels = {A,B,C,1,2,3,result}
    //     qubit_id = distance(qubit_labels.begin(), find(qubit_labels.begin(), qubit_labels.end(), token));
    std::vector<std::string> qubit_labels;
    qubit_labels.clear();
    std::string line;
    size_t n_qubit = 0;

    while (getline(qc_file, line)) {
        line = line[line.size() - 1] == '\r' ? line.substr(0, line.size() - 1) : line;
        line = dvlab::str::trim_spaces(line);

        if (line.find('.') == 0)  // find initial statement
        {
            // erase .v .i or .o
            line.erase(0, line.find(' ') + 1);
            size_t pos = 0;
            while (pos != std::string::npos) {
                std::string token;

                pos = str_get_token(line, token, pos);
                if (!count(qubit_labels.begin(), qubit_labels.end(), token)) {
                    qubit_labels.emplace_back(token);
                    n_qubit++;
                }
            }
        } else if (line.find('#') == 0 || line.empty())
            continue;
        else if (line.find("BEGIN") == 0 || line.find("begin") == 0) {
            add_qubits(n_qubit);
        } else if (line.find("END") == 0 || line.find("end") == 0) {
            return true;
        } else  // find a gate
        {
            std::string type, qubit_label;
            QubitIdList qubit_ids;
            size_t pos = str_get_token(line, type, 0);
            while (pos != std::string::npos) {
                pos = str_get_token(line, qubit_label, pos);
                if (count(qubit_labels.begin(), qubit_labels.end(), qubit_label)) {
                    auto const qubit_id = distance(qubit_labels.begin(), find(qubit_labels.begin(), qubit_labels.end(), qubit_label));
                    qubit_ids.emplace_back(qubit_id);
                } else {
                    spdlog::error("encountered a undefined qubit ({})!!", qubit_label);
                    return false;
                }
            }
            if (type == "Tof" || type == "tof") {
                if (qubit_ids.size() == 1) {
                    append(XGate(), qubit_ids);
                } else if (qubit_ids.size() == 2) {
                    append(CXGate(), qubit_ids);
                } else if (qubit_ids.size() == 3) {
                    append(CCXGate(), qubit_ids);
                } else {
                    spdlog::error("Toffoli gates with more than 2 controls are not supported!!");
                    return false;
                }
            } else if ((type == "Z" || type == "z") && qubit_ids.size() == 3) {
                append(CCZGate(), qubit_ids);
            } else {
                auto op = str_to_operation(type);
                if (op.has_value()) {
                    append(*op, qubit_ids);
                }
            }
        }
    }
    return true;
}

}  // namespace qsyn::qcir
