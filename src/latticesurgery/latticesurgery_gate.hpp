/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define class LatticeSurgeryGate structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <tl/to.hpp>
#include <utility>
#include <vector>

#include "qsyn/qsyn_type.hpp"

namespace qsyn::latticesurgery {

enum class LatticeSurgeryOpType {
    merge, 
    split,
    measure, // single qubit : {X,Y,Z} double qubits: {X,Y,Z} * {X,Y,Z}
    initialize,


};

enum class MeasureType{
    x,
    y, 
    z
};

class LatticeSurgeryGate {
public:
    LatticeSurgeryGate(size_t id, LatticeSurgeryOpType op_type, QubitIdList qubits);
    LatticeSurgeryGate(size_t id, LatticeSurgeryOpType op_type, QubitIdList qubits, size_t depth)
        : _id(id), _op_type(op_type), _qubits(std::move(qubits)), _depth(depth){};
    LatticeSurgeryGate(size_t id, LatticeSurgeryOpType op_type, QubitIdList qubits, std::vector<MeasureType> measure_list, size_t depth)
        : _id(id), _op_type(op_type), _qubits(std::move(qubits)), _measure(std::move(measure_list)), _depth(depth){};
    LatticeSurgeryGate(LatticeSurgeryOpType op_type, QubitIdList qubits)
        : LatticeSurgeryGate(0, op_type, std::move(qubits)) {};
    LatticeSurgeryGate(LatticeSurgeryOpType op_type, QubitIdList qubits, size_t depth)
        : LatticeSurgeryGate(0, op_type, std::move(qubits), depth){};
    LatticeSurgeryGate(LatticeSurgeryOpType op_type, QubitIdList qubits, std::vector<MeasureType> measure_list, size_t depth)
        : LatticeSurgeryGate(0, op_type, std::move(qubits), std::move(measure_list), depth) {};

    ~LatticeSurgeryGate() = default;
    LatticeSurgeryGate(LatticeSurgeryGate const& other) = default;
    LatticeSurgeryGate(LatticeSurgeryGate&& other) noexcept = default;

    void swap(LatticeSurgeryGate& other) noexcept {
        std::swap(_id, other._id);
        std::swap(_op_type, other._op_type);
        std::swap(_qubits, other._qubits);
    }

    friend void swap(LatticeSurgeryGate& lhs, LatticeSurgeryGate& rhs) noexcept {
        lhs.swap(rhs);
    }

    LatticeSurgeryGate& operator=(LatticeSurgeryGate copy) {
        copy.swap(*this);
        return *this;
    }

    // Basic access methods
    std::string get_type_str() const;
    LatticeSurgeryOpType get_operation_type() const { return _op_type; }
    size_t get_id() const { return _id; }
    QubitIdList get_qubits() const { return _qubits; }
    QubitIdType get_qubit(size_t pin_id) const { return _qubits[pin_id]; }
    void set_qubits(QubitIdList qubits);
    std::optional<size_t> get_pin_by_qubit(QubitIdType qubit) const;
    size_t get_num_qubits() const { return _qubits.size(); }
    std::vector<MeasureType> get_measure_types() const { return _measure; };
    void set_depth(size_t t){ _depth = t; }
    size_t get_depth() const { return _depth; }

    bool operator==(LatticeSurgeryGate const& rhs) const;
    bool operator!=(LatticeSurgeryGate const& rhs) const { return !(*this == rhs); }

    static bool qubit_id_is_unique(QubitIdList const& qubits);

protected:
    size_t _id;
    LatticeSurgeryOpType _op_type;
    std::vector<QubitIdType> _qubits;
    std::vector<MeasureType> _measure;
    size_t _depth=0;
    
};

} // namespace qsyn::latticesurgery 