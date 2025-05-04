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
#include <optional>
#include <string>

#include "./basic_gate_type.hpp"
#include "./qcir.hpp"
#include "./qcir_io.hpp"
#include "util/dvlab_string.hpp"
#include "util/phase.hpp"

namespace qsyn::qcir {

/**
 * @brief Read QCir file
 *
 * @param filename
 */
std::optional<QCir> from_file(std::filesystem::path const& filepath) {
    auto const extension = filepath.extension();

    if (extension == ".qasm")
        return qcir::from_qasm(filepath);
    else if (extension == ".qc")
        return qcir::from_qc(filepath);
    else {
        spdlog::error("File format \"{}\" is not supported!!", extension);
        return std::nullopt;
    }
}

/**
 * @brief Read QASM
 *
 * @param filename
 */
std::optional<QCir> from_qasm(std::filesystem::path const& filepath) {
    using dvlab::str::str_get_token;

    // read file and open
    std::ifstream qasm_file{filepath};
    if (!qasm_file.is_open()) {
        spdlog::error("Cannot open the QASM file \"{}\"!!", filepath);
        return std::nullopt;
    }
    std::string str;
    for (int i = 0; i < 6; i++) {
        // OPENQASM 2.0;
        // include "qelib1.inc";
        // qreg q[int];
        qasm_file >> str;
    }

    auto const nqubit = stoul(str.substr(str.find('[') + 1, str.size() - str.find('[') - 3));

    QCir qcir{nqubit};

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

        std::string phase_gate_type;
        str_get_token(str, phase_gate_type, 0, '(');
        if (phase_gate_type == "U3" || phase_gate_type == "U" || phase_gate_type == "u") {
            std::string theta_str, phi_str, lambda_str;
            size_t pos = str.find('(');
            if (pos != std::string::npos) {
                size_t end = str.find(')', pos);

                std::string params = str.substr(pos + 1, end - pos - 1);
                std::string qubit_id_str;
                size_t qubit_pos = end + 1;
                str_get_token(str, qubit_id_str, str_get_token(str, qubit_id_str, qubit_pos, '[') + 1, ']');
                auto qubit_id_num = dvlab::str::from_string<unsigned>(qubit_id_str);
                if (!qubit_id_num.has_value() || qubit_id_num >= nqubit) {
                    spdlog::error("invalid qubit id on line {}!!", str);
                    return std::nullopt;
                }
                std::stringstream ss(params);
                std::getline(ss, theta_str, ',');
                std::getline(ss, phi_str, ',');
                std::getline(ss, lambda_str, ',');
                theta_str = dvlab::str::trim_spaces(theta_str);
                phi_str = dvlab::str::trim_spaces(phi_str);
                lambda_str = dvlab::str::trim_spaces(lambda_str);
                auto theta = dvlab::Phase::from_string(theta_str);
                auto phi = dvlab::Phase::from_string(phi_str);
                auto lambda = dvlab::Phase::from_string(lambda_str);
                auto op = str_to_operation(type, {*theta, *phi, *lambda});
                if (!op.has_value()) {
                    spdlog::error("invalid phase on line {}!!", str);
                    return std::nullopt;
                }
                qcir.append(*op, {qubit_id_num.value()});
                continue;
            }
        }
        else{
            QubitIdList qubit_ids;
            std::string token;
            std::string qubit_id_str;
            size_t n = str_get_token(str, token, type_end, ',');
            while (!token.empty()) {            
                str_get_token(token, qubit_id_str, str_get_token(token, qubit_id_str, 0, '[') + 1, ']');
                auto qubit_id_num = dvlab::str::from_string<unsigned>(qubit_id_str);
                if (!qubit_id_num.has_value() || qubit_id_num >= nqubit) {
                    spdlog::error("invalid qubit id on line {}!!", str);
                    return std::nullopt;
                }
                qubit_ids.emplace_back(qubit_id_num.value());
                n = str_get_token(str, token, n, ',');
            }

            if (!QCirGate::qubit_id_is_unique(qubit_ids)) {
                spdlog::error("duplicate qubit id on line {}!!", str);
                return std::nullopt;
            }

            if (auto op = str_to_operation(type); op.has_value()) {
                qcir.append(*op, qubit_ids);
                continue;
            }

            auto phase = dvlab::Phase::from_string(phase_str);
            if (!phase.has_value()) {
                spdlog::error("invalid phase on line {}!!", str);
                return std::nullopt;
            }
            if (auto op = str_to_operation(type, {*phase}); op.has_value()) {
                qcir.append(*op, qubit_ids);
                continue;
            }
        }
    }
    return qcir;
}

/**
 * @brief Read QC
 *
 * @param filename
 */
std::optional<QCir> from_qc(std::filesystem::path const& filepath) {
    using dvlab::str::str_get_token;
    // read file and open
    std::ifstream qc_file{filepath};
    if (!qc_file.is_open()) {
        spdlog::error("Cannot open the QC file \"{}\"!!", filepath);
        return std::nullopt;
    }

    // ex: qubit_labels = {A,B,C,1,2,3,result}
    std::vector<std::string> qubit_labels;
    qubit_labels.clear();
    std::string line;
    size_t n_qubit = 0;

    QCir qcir;

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
                if (!std::ranges::count(qubit_labels, token)) {
                    qubit_labels.emplace_back(token);
                    n_qubit++;
                }
            }
        } else if (line.find('#') == 0 || line.empty())
            continue;
        else if (line.find("BEGIN") == 0 || line.find("begin") == 0) {
            qcir.add_qubits(n_qubit);
        } else if (line.find("END") == 0 || line.find("end") == 0) {
            return qcir;
        } else  // find a gate
        {
            std::string type, qubit_label;
            QubitIdList qubit_ids;
            size_t pos = str_get_token(line, type, 0);
            while (pos != std::string::npos) {
                pos = str_get_token(line, qubit_label, pos);
                if (std::ranges::count(qubit_labels, qubit_label)) {
                    auto const qubit_id =
                        std::ranges::distance(
                            qubit_labels.begin(),
                            std::ranges::find(qubit_labels, qubit_label));
                    qubit_ids.emplace_back(qubit_id);
                } else {
                    spdlog::error("encountered a undefined qubit ({})!!", qubit_label);
                    return std::nullopt;
                }
            }
            if (type == "Tof" || type == "tof") {
                if (qubit_ids.size() == 1) {
                    qcir.append(XGate(), qubit_ids);
                } else if (qubit_ids.size() == 2) {
                    qcir.append(CXGate(), qubit_ids);
                } else if (qubit_ids.size() == 3) {
                    qcir.append(CCXGate(), qubit_ids);
                } else {
                    spdlog::error("Toffoli gates with more than 2 controls are not supported!!");
                    return std::nullopt;
                }
            } else if ((type == "Z" || type == "z" || type == "Zd") && qubit_ids.size() == 3) {
                qcir.append(CCZGate(), qubit_ids);
            } else if ((type == "Z" || type == "z") && qubit_ids.size() == 2) {
                qcir.append(CZGate(), qubit_ids);
            } else {
                auto op = str_to_operation(type);
                if (op.has_value()) {
                    qcir.append(*op, qubit_ids);
                } else {
                    spdlog::error("unsupported gate type ({})!!", type);
                    return std::nullopt;
                }
            }
        }
    }
    return qcir;
}

}  // namespace qsyn::qcir
