/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define measurement handling for QASM files ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace qsyn::qcir {
    class QCir;  // Forward declaration
}

namespace qsyn::qcir {

//------------------------------------------------------------------------
//   Measurement Types and Structures
//------------------------------------------------------------------------

enum class MeasurementType {
    bulk,      // measure q -> c (all qubits to corresponding classical bits)
    individual // measure q[i] -> c[i] (individual qubits to specific classical bits)
};

struct MeasurementInfo {
    MeasurementType type;
    std::vector<size_t> qubit_ids;
    std::vector<size_t> classical_bit_ids;
    std::string qubit_register;
    std::string classical_register;
};

class MeasurementHandler {
public:
    // Parse measurement statement from QASM
    static std::optional<MeasurementInfo> parse_measurement(const std::string& line);
    
    // Validate measurement statement
    static bool validate_measurement(const MeasurementInfo& info, 
                                   size_t num_qubits, 
                                   size_t num_classical_bits);
    
    // Execute measurement (add gates and create classical bits)
    static bool execute_measurement(QCir& qcir, const MeasurementInfo& info);
    
    // Helper functions
    static std::vector<size_t> parse_register_indices(const std::string& register_str);
    static std::string extract_register_name(const std::string& register_str);
    static bool is_register_reference(const std::string& str);
};

}  // namespace qsyn::qcir
