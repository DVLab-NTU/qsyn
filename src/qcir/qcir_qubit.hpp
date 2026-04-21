/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirQubit structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace qsyn::qcir {

class QCirGate;

//------------------------------------------------------------------------
//   Define enums and classes
//------------------------------------------------------------------------

enum class QubitType {
    data,    // Regular data qubit
    ancilla  // Ancilla qubit
};

enum class AncillaState {
    clean,   // Clean ancilla qubit (initialized to |0⟩)
    dirty    // Dirty ancilla qubit (may contain arbitrary state)
};

enum class QubitInitialState {
    zero,   // |0⟩ — default computational basis state
    one,    // |1⟩
    plus,   // |+⟩ = H|0⟩
    minus   // |−⟩ = XH|0⟩
};

class QCirQubit {
public:
    // Basic access methods
    void set_last_gate(QCirGate* l) { _last_gate = l; }
    void set_first_gate(QCirGate* f) { _first_gate = f; }
    QCirGate* get_last_gate() const { return _last_gate; }
    QCirGate* get_first_gate() const { return _first_gate; }
    
    // Qubit type management
    void set_type(QubitType type) { _type = type; }
    QubitType get_type() const { return _type; }
    bool is_ancilla() const { return _type == QubitType::ancilla; }
    bool is_data() const { return _type == QubitType::data; }
    
    // Ancilla state management (only relevant for ancilla qubits)
    void set_ancilla_state(AncillaState state) { 
        if (is_ancilla()) {
            _ancilla_state = state; 
        }
    }
    AncillaState get_ancilla_state() const { 
        return is_ancilla() ? _ancilla_state : AncillaState::clean; 
    }
    bool is_clean_ancilla() const { 
        return is_ancilla() && _ancilla_state == AncillaState::clean; 
    }
    bool is_dirty_ancilla() const { 
        return is_ancilla() && _ancilla_state == AncillaState::dirty; 
    }
    
    // Initial state management
    void set_initial_state(QubitInitialState state) { _initial_state = state; }
    QubitInitialState get_initial_state() const { return _initial_state; }
    bool is_zero_initial() const { return _initial_state == QubitInitialState::zero; }
    bool is_one_initial()  const { return _initial_state == QubitInitialState::one; }
    bool is_plus_initial() const { return _initial_state == QubitInitialState::plus; }
    bool is_minus_initial() const { return _initial_state == QubitInitialState::minus; }

    std::string get_initial_state_string() const {
        switch (_initial_state) {
            case QubitInitialState::zero:  return "|0>";
            case QubitInitialState::one:   return "|1>";
            case QubitInitialState::plus:  return "|+>";
            case QubitInitialState::minus: return "|->";
        }
        return "|0>";
    }

    // Utility methods
    std::string get_type_string() const {
        if (is_data()) return "data";
        if (is_clean_ancilla()) return "ancilla(clean)";
        if (is_dirty_ancilla()) return "ancilla(dirty)";
        return "unknown";
    }

private:
    QCirGate* _last_gate  = nullptr;
    QCirGate* _first_gate = nullptr;
    QubitType _type = QubitType::data;
    AncillaState _ancilla_state = AncillaState::clean;
    QubitInitialState _initial_state = QubitInitialState::zero;
};

}  // namespace qsyn::qcir
