/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Implementation of LatticeSurgeryGate class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "latticesurgery/latticesurgery_gate.hpp"

#include <algorithm>
#include <sstream>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

namespace qsyn::latticesurgery {

LatticeSurgeryGate::LatticeSurgeryGate(size_t id, LatticeSurgeryOpType op_type, QubitIdList qubits)
    : _id(id), _op_type(op_type), _qubits(std::move(qubits)) {
    if (!qubit_id_is_unique(_qubits)) {
        throw std::runtime_error("Duplicate qubit IDs in gate");
    }
};

std::string LatticeSurgeryGate::get_type_str() const {
    std::ostringstream measure_op;
    switch (_op_type) {
        case LatticeSurgeryOpType::merge:
            return "merge";
        case LatticeSurgeryOpType::split:
            return "split";
        case LatticeSurgeryOpType::hadamard_l:
            return "hadamard_l";
        case LatticeSurgeryOpType::measure:
            assert(!_measure.empty());
            measure_op << "M";
            for (auto measure_type : _measure) {
                switch (measure_type) {
                    case MeasureType::x:
                        measure_op << "X"; // Append "X" for X measurement
                        break;
                    case MeasureType::y:
                        measure_op << "Y"; // Append "Y" for Y measurement
                        break;
                    case MeasureType::z:
                        measure_op << "Z"; // Append "Z" for Z measurement
                        break;
                    default:
                        measure_op << "-"; // Handle unexpected measure_type
                }
            }
            return measure_op.str();
        case LatticeSurgeryOpType::measure_c:
            assert(!_measure.empty());
            measure_op << "(c) M";
            for (auto measure_type : _measure) {
                switch (measure_type) {
                    case MeasureType::x:
                        measure_op << "X"; // Append "X" for X measurement
                        break;
                    case MeasureType::y:
                        measure_op << "Y"; // Append "Y" for Y measurement
                        break;
                    case MeasureType::z:
                        measure_op << "Z"; // Append "Z" for Z measurement
                        break;
                    default:
                        measure_op << "-"; // Handle unexpected measure_type
                }
            }
            return measure_op.str();
        case LatticeSurgeryOpType::flip:
            assert(_measure.size() == 1);
            if(_measure[0] == MeasureType::x) return "Xflip";
            else return "Zflip";
        case LatticeSurgeryOpType::s:
            assert(_measure.size() == 1);
            if(_measure[0] == MeasureType::x) return "SX";
            else return "SZ";
        case LatticeSurgeryOpType::t:
            assert(_measure.size() == 1);
            if(_measure[0] == MeasureType::x) return "TX";
            else return "TZ";
        default:
            return "unknown";
    }
}

void LatticeSurgeryGate::set_qubits(QubitIdList qubits) {
    if (!qubit_id_is_unique(qubits)) {
        throw std::runtime_error("Duplicate qubit IDs in gate");
    }
    _qubits = std::move(qubits);
}

std::optional<size_t> LatticeSurgeryGate::get_pin_by_qubit(QubitIdType qubit) const {
    auto it = std::find(_qubits.begin(), _qubits.end(), qubit);
    if (it == _qubits.end()) {
        return std::nullopt;
    }
    return std::distance(_qubits.begin(), it);
}

bool LatticeSurgeryGate::operator==(LatticeSurgeryGate const& rhs) const {
    return _id == rhs._id && _op_type == rhs._op_type && _qubits == rhs._qubits;
}

bool LatticeSurgeryGate::qubit_id_is_unique(QubitIdList const& qubits) {
    std::vector<QubitIdType> sorted_qubits = qubits;
    std::sort(sorted_qubits.begin(), sorted_qubits.end());
    return std::adjacent_find(sorted_qubits.begin(), sorted_qubits.end()) == sorted_qubits.end();
}

} // namespace qsyn::latticesurgery 