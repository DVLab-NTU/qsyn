/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define measurement handling for QASM files ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./measurement_handler.hpp"
#include "./qcir.hpp"
#include "./basic_gate_type.hpp"
#include <spdlog/spdlog.h>
#include <regex>
#include <sstream>

namespace qsyn::qcir {

std::optional<MeasurementInfo> MeasurementHandler::parse_measurement(const std::string& line) {
    // Remove leading/trailing whitespace
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    
    // Check if it's a measurement line
    if (trimmed.substr(0, 7) != "measure") {
        return std::nullopt;
    }
    
    // Remove "measure" keyword
    trimmed = trimmed.substr(7);
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    
    // Find the arrow "->"
    size_t arrow_pos = trimmed.find("->");
    if (arrow_pos == std::string::npos) {
        spdlog::error("Invalid measurement syntax: missing '->' in '{}'", line);
        return std::nullopt;
    }
    
    std::string qubit_part = trimmed.substr(0, arrow_pos);
    std::string classical_part = trimmed.substr(arrow_pos + 2);
    
    // Remove whitespace
    qubit_part.erase(0, qubit_part.find_first_not_of(" \t"));
    qubit_part.erase(qubit_part.find_last_not_of(" \t") + 1);
    classical_part.erase(0, classical_part.find_first_not_of(" \t"));
    classical_part.erase(classical_part.find_last_not_of(" \t") + 1);
    
    MeasurementInfo info;
    
    // Determine measurement type and parse
    if (!is_register_reference(qubit_part) && !is_register_reference(classical_part)) {
        // Bulk measurement: measure q -> c (no brackets)
        info.type = MeasurementType::bulk;
        info.qubit_register = qubit_part;
        info.classical_register = classical_part;
        
        spdlog::info("Bulk measurement detected: {} -> {}", info.qubit_register, info.classical_register);
    } else {
        // Individual measurement: measure q[i] -> c[j]
        info.type = MeasurementType::individual;
        
        // Parse qubit indices
        info.qubit_ids = parse_register_indices(qubit_part);
        info.classical_bit_ids = parse_register_indices(classical_part);
        
        if (info.qubit_ids.size() != info.classical_bit_ids.size()) {
            spdlog::error("Mismatch in number of qubits and classical bits in measurement: '{}'", line);
            return std::nullopt;
        }
        
        spdlog::info("Individual measurement detected: {} qubits to {} classical bits", 
                    info.qubit_ids.size(), info.classical_bit_ids.size());
    }
    
    return info;
}

bool MeasurementHandler::validate_measurement(const MeasurementInfo& info, 
                                            size_t num_qubits, 
                                            size_t num_classical_bits) {
    if (info.type == MeasurementType::bulk) {
        // For bulk measurement, we need to ensure classical register is large enough
        // This validation will be done when we know the register sizes
        return true;
    } else {
        // For individual measurement, validate indices
        for (size_t qubit_id : info.qubit_ids) {
            if (qubit_id >= num_qubits) {
                spdlog::error("Qubit index {} out of range (max: {})", qubit_id, num_qubits - 1);
                return false;
            }
        }
        
        for (size_t classical_id : info.classical_bit_ids) {
            if (classical_id >= num_classical_bits) {
                spdlog::error("Classical bit index {} out of range (max: {})", classical_id, num_classical_bits - 1);
                return false;
            }
        }
        
        return true;
    }
}

bool MeasurementHandler::execute_measurement(QCir& qcir, const MeasurementInfo& info) {
    if (info.type == MeasurementType::bulk) {
        // For bulk measurement, measure all qubits to corresponding classical bits
        size_t num_qubits = qcir.get_num_qubits();
        
        // Ensure we have enough classical bits for all qubits
        while (qcir.get_num_classical_bits() < num_qubits) {
            qcir.add_classical_bit();
        }
        
        // Add measurement gates for all qubits
        for (size_t i = 0; i < num_qubits; ++i) {
            qcir.append(MeasurementGate(), {i});
            spdlog::info("Added bulk measurement: qubit {} -> classical bit {}", i, i);
        }
        
        spdlog::info("Executed bulk measurement: {} qubits -> {} classical bits", num_qubits, num_qubits);
        return true;
    } else {
        // For individual measurement, add measurement gates and create classical bits
        for (size_t i = 0; i < info.qubit_ids.size(); ++i) {
            size_t qubit_id = info.qubit_ids[i];
            size_t classical_id = info.classical_bit_ids[i];
            
            // Ensure we have enough classical bits
            while (qcir.get_num_classical_bits() <= classical_id) {
                qcir.add_classical_bit();
            }
            
            // Add measurement gate
            qcir.append(MeasurementGate(), {qubit_id});
            
            // The measurement gate will automatically create a classical bit
            // We can optionally set the classical bit ID mapping here
            spdlog::info("Added measurement: qubit {} -> classical bit {}", qubit_id, classical_id);
        }
        return true;
    }
}

std::vector<size_t> MeasurementHandler::parse_register_indices(const std::string& register_str) {
    std::vector<size_t> indices;
    
    // Handle both single index (q[0]) and multiple indices (q[0], q[1], q[2])
    std::regex index_regex(R"(\[(\d+)\])");
    std::sregex_iterator iter(register_str.begin(), register_str.end(), index_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::smatch match = *iter;
        size_t index = std::stoul(match[1].str());
        indices.push_back(index);
    }
    
    return indices;
}

std::string MeasurementHandler::extract_register_name(const std::string& register_str) {
    // Extract register name from q[0] -> q
    size_t bracket_pos = register_str.find('[');
    if (bracket_pos != std::string::npos) {
        return register_str.substr(0, bracket_pos);
    }
    return register_str;
}

bool MeasurementHandler::is_register_reference(const std::string& str) {
    // Check if string is a register reference (contains [])
    return str.find('[') != std::string::npos;
}

}  // namespace qsyn::qcir
