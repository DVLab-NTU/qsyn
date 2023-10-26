/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Reader functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cstddef>
#include <fstream>
#include <string>

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
bool QCir::read_qcir_file(std::string const& filename) {
    auto const lastname = filename.substr(filename.find_last_of('/') + 1);

    auto const extension = (lastname.find('.') != std::string::npos) ? lastname.substr(lastname.find_last_of('.')) : "";

    if (extension == ".qasm")
        return read_qasm(filename);
    else if (extension == ".qc")
        return read_qc(filename);
    else if (extension == ".qsim")
        return read_qsim(filename);
    else if (extension == ".quipper")
        return read_quipper(filename);
    else if (extension == "") {
        std::ifstream verify{filename};
        if (!verify.is_open()) {
            spdlog::error("Cannot open the file \"{}\"!!", filename);
            return false;
        }
        std::string first_item;
        verify >> first_item;

        if (first_item == "Inputs:")
            return read_quipper(filename);
        else if (isdigit(first_item[0]))
            return read_qsim(filename);
        else {
            spdlog::error("Cannot derive the type of file \"{}\"!!", filename);
            return false;
        }
        return true;
    } else {
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
bool QCir::read_qasm(std::string const& filename) {
    using dvlab::str::str_get_token;
    // read file and open
    auto const lastname = filename.substr(filename.find_last_of('/') + 1);
    _procedures.clear();
    std::fstream qasm_file{filename};
    if (!qasm_file.is_open()) {
        spdlog::error("Cannot open the QASM file \"{}\"!!", filename);
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
        if (str == "") continue;
        std::string type;
        auto const type_end   = str_get_token(str, type);
        std::string phase_str = "0";
        if (str_get_token(str, phase_str, 0, '(') != std::string::npos) {
            auto const stop = str_get_token(str, type, 0, '(');
            str_get_token(str, phase_str, stop + 1, ')');
        } else
            phase_str = "0";
        if (type == "creg" || type == "qreg" || type == "") {
            continue;
        }
        QubitIdList qubit_ids;
        std::string token;
        std::string qubit_id_str;
        size_t n = str_get_token(str, token, type_end, ',');
        while (token.size()) {
            str_get_token(token, qubit_id_str, str_get_token(token, qubit_id_str, 0, '[') + 1, ']');
            auto qubit_id_num = dvlab::str::from_string<unsigned>(qubit_id_str);
            if (!qubit_id_num.has_value() || qubit_id_num >= nqubit) {
                spdlog::error("invalid qubit id on line {}!!", str);
                return false;
            }
            qubit_ids.emplace_back(qubit_id_num.value());
            n = str_get_token(str, token, n, ',');
        }

        auto phase = dvlab::Phase::from_string(phase_str);
        if (!phase.has_value()) {
            spdlog::error("invalid phase on line {}!!", str);
            return false;
        }
        add_gate(type, qubit_ids, phase.value(), true);
    }
    update_gate_time();
    return true;
}

/**
 * @brief Read QC
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::read_qc(std::string const& filename) {
    using dvlab::str::str_get_token;
    // read file and open
    std::fstream qc_file{filename};
    if (!qc_file.is_open()) {
        spdlog::error("Cannot open the QC file \"{}\"!!", filename);
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
        } else if (line.find('#') == 0 || line == "")
            continue;
        else if (line.find("BEGIN") == 0) {
            add_qubits(n_qubit);
        } else if (line.find("END") == 0) {
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
                if (qubit_ids.size() == 1)
                    add_gate("x", qubit_ids, dvlab::Phase(1), true);
                else if (qubit_ids.size() == 2)
                    add_gate("cx", qubit_ids, dvlab::Phase(1), true);
                else if (qubit_ids.size() == 3)
                    add_gate("ccx", qubit_ids, dvlab::Phase(1), true);
                else {
                    spdlog::error("Toffoli gates with more than 2 controls are not supported!!");
                    return false;
                }
            } else
                add_gate(type, qubit_ids, dvlab::Phase(1), true);
        }
    }
    return true;
}

/**
 * @brief Read QSIM
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::read_qsim(std::string const& filename) {
    // read file and open
    std::ifstream qsim_file{filename};
    if (!qsim_file.is_open()) {
        spdlog::error("Cannot open the QSIM file \"{}\"!!", filename);
        return false;
    }

    std::string line;
    static std::vector<std::string> const single_gate_list{"x", "y", "z", "h", "t", "x_1_2", "y_1_2", "rx", "rz", "s"};
    // decide qubit number
    int n_qubit = 0;
    getline(qsim_file, line);
    n_qubit = stoi(line);
    add_qubits(n_qubit);

    // add the gate
    // Todo: implentment hz_1_2 gate and fs gate

    while (getline(qsim_file, line)) {
        using dvlab::str::str_get_token;
        if (line == "") continue;
        std::string time, type, phase_str, qubit_id;
        QubitIdList qubit_ids;
        size_t pos = 0;
        pos        = str_get_token(line, time, pos);
        pos        = str_get_token(line, type, pos);
        if (type == "cx" || type == "cz") {
            // add 2 qubit gate
            pos = str_get_token(line, qubit_id, pos);
            qubit_ids.emplace_back(stoul(qubit_id));
            str_get_token(line, qubit_id, pos);
            qubit_ids.emplace_back(stoul(qubit_id));
            add_gate(type, qubit_ids, dvlab::Phase(1), true);
        } else if (type == "rx" || type == "rz") {
            // add phase gate
            pos = str_get_token(line, qubit_id, pos);
            qubit_ids.emplace_back(stoul(qubit_id));
            str_get_token(line, phase_str, pos);
            auto phase = dvlab::Phase::from_string(phase_str);
            if (!phase.has_value()) {
                spdlog::error("invalid phase on line {}!!", line);
                return false;
            }
            add_gate(type, qubit_ids, phase.value(), true);
        } else if (count(single_gate_list.begin(), single_gate_list.end(), type)) {
            // add single qubit gate
            str_get_token(line, qubit_id, pos);
            qubit_ids.emplace_back(stoul(qubit_id));
            // FIXME - pass in the correct phase
            add_gate(type, qubit_ids, dvlab::Phase(0), true);
        } else {
            spdlog::error("Gate type {} is not supported!!", type);
            return false;
        }
    }
    return true;
}

/**
 * @brief Read QUIPPER
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::read_quipper(std::string const& filename) {
    // read file and open
    std::ifstream quipper_file{filename};
    if (!quipper_file.is_open()) {
        spdlog::error("Cannot open the QUIPPER file \"{}\"!!", filename);
        return false;
    }

    std::string line;
    size_t n_qubit = 0;
    std::vector<std::string> single_list{"X", "T", "S", "H", "Z", "not"};

    // Count qubit number
    getline(quipper_file, line);
    n_qubit = count(line.begin(), line.end(), 'Q');
    add_qubits(n_qubit);

    while (getline(quipper_file, line)) {
        if (line.find("QGate") == 0) {
            // addgate
            std::string type    = line.substr(line.find('[') + 2, line.find(']') - line.find('[') - 3);
            size_t qubit_target = 0;
            if (find(single_list.begin(), single_list.end(), type) != single_list.end()) {
                qubit_target = stoul(line.substr(line.find('(') + 1, line.find(')') - line.find('(') - 1));
                QubitIdList qubit_ids;

                if (line.find("controls=") != std::string::npos) {
                    // have control
                    std::string ctrls_info;
                    ctrls_info = line.substr(line.find_last_of('[') + 1, line.find_last_of(']') - line.find_last_of('[') - 1);

                    if (ctrls_info.find(std::to_string(qubit_target)) != std::string::npos) {
                        spdlog::error("Control qubit and target cannot be the same!!");
                        return false;
                    }

                    if (count(line.begin(), line.end(), '+') == 1) {
                        // one control
                        if (type != "not" && type != "X" && type != "Z") {
                            spdlog::error("Unsupported controlled gate type!! Only `cnot`, `CX` and `CZ` are supported.");
                            return false;
                        }
                        auto const qubit_control = stoul(ctrls_info.substr(1));
                        qubit_ids.emplace_back(qubit_control);
                        qubit_ids.emplace_back(qubit_target);
                        type.insert(0, "C");
                        add_gate(type, qubit_ids, dvlab::Phase(1), true);
                    } else if (count(line.begin(), line.end(), '+') == 2) {
                        // 2 controls
                        if (type != "not" && type != "X" && type != "Z") {
                            spdlog::error("Unsupported doubly-controlled gate type!! Only `ccx` and `ccz` are supported.");
                            return false;
                        }
                        size_t qubit_control1 = 0, qubit_control2 = 0;
                        qubit_control1 = stoul(ctrls_info.substr(1, ctrls_info.find(',') - 1));
                        qubit_control2 = stoul(ctrls_info.substr(ctrls_info.find(',') + 2));
                        qubit_ids.emplace_back(qubit_control1);
                        qubit_ids.emplace_back(qubit_control2);
                        qubit_ids.emplace_back(qubit_target);
                        type.insert(0, "CC");
                        add_gate(type, qubit_ids, dvlab::Phase(1), true);
                    } else {
                        spdlog::error("Controlled gates with more than 2 controls are not supported!!");
                        return false;
                    }
                } else {
                    // without control
                    qubit_ids.emplace_back(qubit_target);
                    // FIXME - pass in the correct phase
                    add_gate(type, qubit_ids, dvlab::Phase(0), true);
                }

            } else {
                spdlog::error("Unsupported gate type {}!!", type);
                return false;
            }
            continue;
        } else if (line.find("Outputs") == 0) {
            return true;
        } else if (line.find("Comment") == 0 || line.find("QTerm0") == 0 || line.find("QMeas") == 0 || line.find("QDiscard") == 0)
            continue;
        else if (line.find("QInit0") == 0) {
            spdlog::error("Unsupported expression: QInit0");
            return false;
        } else if (line.find("QRot") == 0) {
            spdlog::error("Unsupported expression: QRot");
            return false;
        } else {
            spdlog::error("Unsupported expression: {}", line);
        }
    }
    return true;
}

}  // namespace qsyn::qcir
