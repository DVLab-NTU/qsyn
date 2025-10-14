/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <tl/to.hpp>

#include "qsyn/qsyn_type.hpp"
namespace qsyn::qcir {

class Operation;

class QCirGate {  // NOLINT(cppcoreguidelines-special-member-functions)
                  // : copy-swap idiom
public:
    QCirGate(size_t id, Operation const& op, QubitIdList qubits);
    QCirGate(Operation const& op, QubitIdList qubits)
        : QCirGate(0, op, std::move(qubits)) {}
    
    // Constructor for gates with classical bits (e.g., measurement)
    QCirGate(size_t id, Operation const& op, QubitIdList qubits, ClassicalBitIdList classical_bits);
    QCirGate(Operation const& op, QubitIdList qubits, ClassicalBitIdList classical_bits)
        : QCirGate(0, op, std::move(qubits), std::move(classical_bits)) {}
    
    // Constructor for if-else gates (single classical bit with value)
    QCirGate(size_t id, Operation const& op, QubitIdList qubits, ClassicalBitIdType classical_bit, size_t classical_value);
    QCirGate(Operation const& op, QubitIdList qubits, ClassicalBitIdType classical_bit, size_t classical_value)
        : QCirGate(0, op, std::move(qubits), classical_bit, classical_value) {}
    
    // Constructor for if-else gates (all classical bits with value)
    QCirGate(size_t id, Operation const& op, QubitIdList qubits, size_t classical_value);
    QCirGate(Operation const& op, QubitIdList qubits, size_t classical_value)
        : QCirGate(0, op, std::move(qubits), classical_value) {}

    ~QCirGate();

    QCirGate(QCirGate const& other);
    QCirGate(QCirGate&& other) noexcept;

    // copy-swap idiom
    void swap(QCirGate& other) noexcept;

    friend void swap(QCirGate& lhs, QCirGate& rhs) noexcept {
        lhs.swap(rhs);
    }

    QCirGate& operator=(QCirGate copy) {
        copy.swap(*this);
        return *this;
    }

    // Basic access method
    std::string get_type_str() const;
    Operation const& get_operation() const;
    void set_operation(Operation const& op);
    size_t get_id() const { return _id; }
    QubitIdList get_qubits() const { return _qubits; }
    QubitIdType get_qubit(size_t pin_id) const { return _qubits[pin_id]; }
    void set_qubits(QubitIdList qubits);
    std::optional<size_t> get_pin_by_qubit(QubitIdType qubit) const;

    size_t get_num_qubits() const { return _qubits.size(); }
    
    // Classical bit access methods
    bool has_classical_bits() const { return _has_classical_bits; }
    ClassicalBitIdList get_classical_bits() const { return _classical_bits; }
    ClassicalBitIdType get_classical_bit(size_t pin_id) const { return _classical_bits[pin_id]; }
    size_t get_num_classical_bits() const { return _classical_bits.size(); }
    std::optional<size_t> get_pin_by_classical_bit(ClassicalBitIdType classical_bit) const;
    
    // If-else gate access methods
    std::optional<size_t> get_classical_value() const { return _classical_value; }

    bool operator==(QCirGate const& rhs) const;
    bool operator!=(QCirGate const& rhs) const { return !(*this == rhs); }

    static bool qubit_id_is_unique(QubitIdList const& qubits);

protected:
    size_t _id;
    Operation* _operation;  // owning; but raw pointer to avoid having to
                            // include operation.hpp in this header. This is
                            // a necessary evil to realize `to_basic_gates`.
    std::vector<QubitIdType> _qubits;
    ClassicalBitIdList _classical_bits;   // Classical bits this gate operates on
    bool _has_classical_bits = false;     // Flag to identify classical operations
    std::optional<size_t> _classical_value;  // Classical value for if-else gates
};

}  // namespace qsyn::qcir
