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
    QubitType _type = QubitType::data;  // Default to data qubit
    AncillaState _ancilla_state = AncillaState::clean;  // Default to clean
};

}  // namespace qsyn::qcir
