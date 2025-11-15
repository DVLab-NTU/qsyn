/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Reader functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "./basic_gate_type.hpp"
#include "./qcir.hpp"
#include "./qcir_io.hpp"
#include "util/dvlab_string.hpp"
#include "util/phase.hpp"

namespace qsyn::qcir {

/**
 * @brief Apply a single measurement gate from qubit i to classical bit j
 *
 * @param qcir Reference to the QCir object to add measurement gates
 * @param qubit_id Qubit index to measure
 * @param classical_bit_id Classical bit index to store result
 * @param total_qubits Total number of qubits available
 * @param total_classical_bits Total number of classical bits available
 * @return true if measurement was applied successfully, false otherwise
 */
bool apply_measurement_gate(QCir& qcir, size_t qubit_id, size_t classical_bit_id,
                           size_t total_qubits, size_t total_classical_bits) {
    // Validate indices exist
    if (qubit_id >= total_qubits) {
        spdlog::error("Qubit index {} out of range (max: {})", qubit_id, total_qubits - 1);
        return false;
    }
    if (classical_bit_id >= total_classical_bits) {
        spdlog::error("Classical bit index {} out of range (max: {})", classical_bit_id, total_classical_bits - 1);
        return false;
    }
    
    // Add measurement gate - QCir will handle the rest
    qcir.append(MeasurementGate(), qubit_id, classical_bit_id);
    return true;
}

/**
 * @brief Parse measurement operation from string and validate qubit/bit indices
 *
 * @param str The measurement string (e.g., "measure q -> c" or "measure q[0] -> c[0]")
 * @param qcir Reference to the QCir object to add measurement gates
 * @param total_qubits Total number of qubits available
 * @param total_classical_bits Total number of classical bits available
 * @return true if parsing was successful, false otherwise
 */
bool parse_measurement_operation(const std::string& str, QCir& qcir,
                                size_t total_qubits, size_t total_classical_bits) {
    using dvlab::str::str_get_token;
    
    // Parse measurement: measure {qubit} -> {classical}
    std::string measurement_str = str;
    
    // Remove semicolon from the end if present
    if (!measurement_str.empty() && measurement_str.back() == ';') {
        measurement_str.pop_back();
    }
    
    // Extract "measure" token
    std::string measure_token;
    size_t pos = str_get_token(measurement_str, measure_token, 0);
    if (measure_token != "measure") {
        spdlog::error("Invalid measurement syntax: must start with 'measure', got '{}'", measure_token);
        return false;
    }
    
    // Extract qubit part (everything before "->")
    std::string qubit_part;
    size_t arrow_pos = measurement_str.find("->");
    if (arrow_pos == std::string::npos) {
        spdlog::error("Invalid measurement syntax: must contain '->', got '{}'", str);
        return false;
    }
    
    qubit_part = measurement_str.substr(pos, arrow_pos - pos);
    qubit_part = dvlab::str::trim_spaces(qubit_part);
    
    // Extract classical part (everything after "->")
    std::string classical_part = measurement_str.substr(arrow_pos + 2);
    classical_part = dvlab::str::trim_spaces(classical_part);
    
    // Check if it's bulk measurement (q -> c) or individual measurement (q[i] -> c[j])
    bool is_bulk = (qubit_part == "q" && classical_part == "c");
    
    if (is_bulk) {
        // Bulk measurement: measure q -> c
        // Call individual measurement function recursively for each qubit
        size_t min_size = std::min(total_qubits, total_classical_bits);
        for (size_t i = 0; i < min_size; ++i) {
            if (!apply_measurement_gate(qcir, i, i, total_qubits, total_classical_bits)) {
                return false;
            }
        }
        
        if (total_classical_bits < total_qubits) {
            spdlog::warn("Not enough classical bits for bulk measurement. Only measuring first {} qubits", min_size);
        }
        
    } else {
        // Individual measurement: measure q[i] -> c[j]
        
        // Parse qubit index using str_get_token
        std::string qubit_name, qubit_index_str;
        size_t q_bracket_pos = str_get_token(qubit_part, qubit_name, 0, '[');
        if (q_bracket_pos != std::string::npos) {
            str_get_token(qubit_part, qubit_index_str, q_bracket_pos + 1, ']');
        } else {
            spdlog::error("Invalid qubit format in measurement: '{}'", qubit_part);
            return false;
        }
        
        // Parse classical bit index using str_get_token
        std::string classical_name, classical_index_str;
        size_t c_bracket_pos = str_get_token(classical_part, classical_name, 0, '[');
        if (c_bracket_pos != std::string::npos) {
            str_get_token(classical_part, classical_index_str, c_bracket_pos + 1, ']');
        } else {
            spdlog::error("Invalid classical bit format in measurement: '{}'", classical_part);
            return false;
        }
        
        // Convert indices to numbers
        auto q_index = dvlab::str::from_string<size_t>(qubit_index_str);
        auto c_index = dvlab::str::from_string<size_t>(classical_index_str);
        
        if (q_index.has_value() && c_index.has_value()) {
            // Use the individual measurement function
            return apply_measurement_gate(qcir, q_index.value(), c_index.value(), total_qubits, total_classical_bits);
        } else {
            spdlog::error("Invalid indices in measurement: qubit='{}', classical='{}'", qubit_index_str, classical_index_str);
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Parse if-else operation from string
 *
 * @param str The if-else string (e.g., "if(c[0]==1) {CX q[0] q[1]}")
 * @param qcir Reference to the QCir object to add if-else gates
 * @param total_qubits Total number of qubits available
 * @param total_classical_bits Total number of classical bits available
 * @return true if parsing was successful, false otherwise
 */
bool parse_if_else_operation(const std::string& str, QCir& qcir,
                            size_t total_qubits, size_t total_classical_bits) {
    using dvlab::str::str_get_token;
    
    // Parse if-else: if(c[bit]==value) {operation qubits}
    std::string if_else_str = str;
    
    // Remove semicolon from the end if present
    if (!if_else_str.empty() && if_else_str.back() == ';') {
        if_else_str.pop_back();
    }

    // Find the condition part (c[bit]==value)
    size_t paren_start = if_else_str.find('(');
    size_t paren_end = if_else_str.find(')');
    if (paren_start == std::string::npos || paren_end == std::string::npos) {
        spdlog::error("Invalid if-else syntax: must contain condition in parentheses, got '{}'", str);
        return false;
    }
    
    std::string condition = if_else_str.substr(paren_start + 1, paren_end - paren_start - 1);
    condition = dvlab::str::trim_spaces(condition);
    
    // Parse condition: c[bit]==value
    size_t eq_pos = condition.find("==");
    if (eq_pos == std::string::npos) {
        spdlog::error("Invalid condition syntax: must contain '==', got '{}'", condition);
        return false;
    }
    
    std::string classical_part = condition.substr(0, eq_pos);
    std::string value_part = condition.substr(eq_pos + 2);
    classical_part = dvlab::str::trim_spaces(classical_part);
    value_part = dvlab::str::trim_spaces(value_part);
    
    // Parse classical bit index - check if it's single bit or all bits
    bool check_all_bits = false;
    ClassicalBitIdType classical_bit = 0;
    
    if (classical_part == "c") {
        // All classical bits: if(c==value)
        check_all_bits = true;
    } else if (classical_part.find('[') != std::string::npos) {
        // Single classical bit: if(c[bit]==value)
        size_t bracket_start = classical_part.find('[');
        size_t bracket_end = classical_part.find(']');
        if (bracket_start != std::string::npos && bracket_end != std::string::npos) {
            std::string bit_str = classical_part.substr(bracket_start + 1, bracket_end - bracket_start - 1);
            bit_str = dvlab::str::trim_spaces(bit_str);
            
            auto bit_index = dvlab::str::from_string<size_t>(bit_str);
            if (bit_index.has_value()) {
                if (bit_index.value() >= total_classical_bits) {
                    spdlog::error("Classical bit index {} out of range (max: {})", bit_index.value(), total_classical_bits - 1);
                    return false;
                }
                classical_bit = bit_index.value();
            } else {
                spdlog::error("Invalid classical bit index: '{}'", bit_str);
                return false;
            }
        } else {
            spdlog::error("Invalid classical bit format: '{}'", classical_part);
            return false;
        }
    } else {
        spdlog::error("Invalid classical bit format: '{}'", classical_part);
        return false;
    }
    
    // Parse target value
    auto value = dvlab::str::from_string<size_t>(value_part);
    if (!value.has_value()) {
        spdlog::error("Invalid target value: '{}'", value_part);
        return false;
    }
    size_t classical_value = value.value();
    
    // Extract operation and qubits from the rest of the string
    std::string operation_part = if_else_str.substr(paren_end + 1);
    operation_part = dvlab::str::trim_spaces(operation_part);
    
    // Parse operation type and qubits
    std::string op_type;
    size_t op_pos = str_get_token(operation_part, op_type, 0);
    
    // Parse qubit indices
    QubitIdList qubit_ids;
    std::string token;
    size_t n = str_get_token(operation_part, token, op_pos, ',');
    while (!token.empty()) {            
        // Extract qubit index from q[index] format
        size_t bracket_start = token.find('[');
        size_t bracket_end = token.find(']');
        if (bracket_start != std::string::npos && bracket_end != std::string::npos) {
            std::string qubit_index_str = token.substr(bracket_start + 1, bracket_end - bracket_start - 1);
            qubit_index_str = dvlab::str::trim_spaces(qubit_index_str);
            
            auto qubit_id_num = dvlab::str::from_string<unsigned>(qubit_index_str);
            if (!qubit_id_num.has_value() || qubit_id_num.value() >= total_qubits) {
                spdlog::error("Invalid qubit id in if-else operation: '{}'", qubit_index_str);
                return false;
            }
            qubit_ids.emplace_back(qubit_id_num.value());
        } else {
            spdlog::error("Invalid qubit format in if-else operation: '{}'", token);
            return false;
        }
        n = str_get_token(operation_part, token, n, ',');
    }
    
    if (!QCirGate::qubit_id_is_unique(qubit_ids)) {
        spdlog::error("Duplicate qubit id in if-else operation: '{}'", str);
        return false;
    }
    
    // Create the operation
    auto op = str_to_operation(op_type);
    if (!op.has_value()) {
        spdlog::error("Unsupported operation in if-else: '{}'", op_type);
        return false;
    }
    
    // Add the if-else gate
    if (check_all_bits) {
        // All classical bits: if(c==value)
        qcir.append(*op, qubit_ids, classical_value);
    } else {
        // Single classical bit: if(c[bit]==value)
        qcir.append(*op, qubit_ids, classical_bit, classical_value);
    }
    return true;
}

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
    // Parse first 6 lines to find qreg and creg declarations
    size_t total_qubits = 0;
    size_t total_classical_bits = 0;
    
    // Read first 6 lines to find register declarations
    std::string str;
    for (int i = 0; i < 5; i++) {
        if (!getline(qasm_file, str)) break;
        str = dvlab::str::trim_spaces(dvlab::str::trim_comments(str));
        if (str.empty()) continue;
        
        std::string type;
        auto const type_end = str_get_token(str, type);
        
        if (type == "qreg") {
            // Parse qubit register: qreg q[3];
            std::string remaining = str.substr(type_end);
            remaining = dvlab::str::trim_spaces(remaining);
            
            size_t bracket_start = remaining.find('[');
            size_t bracket_end = remaining.find(']');
            if (bracket_start != std::string::npos && bracket_end != std::string::npos) {
                std::string size_str = remaining.substr(bracket_start + 1, bracket_end - bracket_start - 1);
                auto size = dvlab::str::from_string<size_t>(size_str);
                if (size.has_value()) {
                    total_qubits += size.value();
                    // spdlog::info("Found qubit register with {} qubits", size.value());
                }
            }
        } else if (type == "creg") {
            // Parse classical register: creg c[3];
            std::string remaining = str.substr(type_end);
            remaining = dvlab::str::trim_spaces(remaining);
            
            size_t bracket_start = remaining.find('[');
            size_t bracket_end = remaining.find(']');
            if (bracket_start != std::string::npos && bracket_end != std::string::npos) {
                std::string size_str = remaining.substr(bracket_start + 1, bracket_end - bracket_start - 1);
                auto size = dvlab::str::from_string<size_t>(size_str);
                if (size.has_value()) {
                    total_classical_bits += size.value();
                    // spdlog::info("Found classical register with {} bits", size.value());
                }
            }
        }
    }
    
    // Initialize QCir with correct number of qubits and classical bits
    QCir qcir{total_qubits, total_classical_bits};
    // spdlog::info("Initialized QCir with {} qubits and {} classical bits", total_qubits, total_classical_bits);

    // Continue reading the rest of the file
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
        // Skip register declarations since we already parsed them in the first 6 lines
        if (type == "creg" || type == "qreg" || type.empty()) {
            continue;
        }

        // Handle measurement operations first
        if (type == "measure") {
            if (!parse_measurement_operation(str, qcir, total_qubits, total_classical_bits)) {
                return std::nullopt;
            }
            continue;
        }
        
        // Handle if-else operations
        if (type == "if") {
            if (!parse_if_else_operation(str, qcir, total_qubits, total_classical_bits)) {
                return std::nullopt;
            }
            continue;
        }

        std::string phase_gate_type;
        str_get_token(str, phase_gate_type, 0, '(');
        if (phase_gate_type == "U3" || phase_gate_type == "U" || phase_gate_type == "u" || phase_gate_type == "u3") {
            std::string theta_str, phi_str, lambda_str;
            size_t pos = str.find('(');
            if (pos != std::string::npos) {
                size_t end = str.find(')', pos);

                std::string params = str.substr(pos + 1, end - pos - 1);
                std::string qubit_id_str;
                size_t qubit_pos = end + 1;
                str_get_token(str, qubit_id_str, str_get_token(str, qubit_id_str, qubit_pos, '[') + 1, ']');
                auto qubit_id_num = dvlab::str::from_string<unsigned>(qubit_id_str);
                if (!qubit_id_num.has_value() || qubit_id_num >= total_qubits) {
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
                if (!qubit_id_num.has_value() || qubit_id_num >= total_qubits) {
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
