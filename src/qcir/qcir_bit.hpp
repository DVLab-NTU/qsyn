/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirBit structure for classical bits ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <algorithm>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace qsyn::qcir {

// Forward declaration - we only need pointers to QCirGate
class QCirGate;

//------------------------------------------------------------------------
//   Define enums and classes
//------------------------------------------------------------------------

class QCirBit {
public:
    // Constructor
    QCirBit() = default;
    
    // Initialize with a specific value (0 or 1)
    QCirBit(bool bit_value) 
        : _has_value(true), _bit_value(bit_value), _measured(false), _measurement_gate(nullptr) {}
    
    // Value management
    void set_value(bool bit_value) {
        _has_value = true;
        _bit_value = bit_value;
    }
    
    void clear_value() {
        _has_value = false;
        _bit_value = false;  // Reset to default
    }
    
    bool has_value() const { 
        return _has_value; 
    }
    
    bool get_value() const { 
        return _bit_value; 
    }
    
    // Measurement management
    void set_measured(QCirGate* measurement_gate = nullptr) {
        _measured = true;
        _measurement_gate = measurement_gate;
    }
    
    bool is_measured() const { 
        return _measured; 
    }
    
    QCirGate* get_measurement_gate() const { 
        return _measurement_gate; 
    }
    
    // Combined state checks
    bool is_determined() const {
        return _has_value || _measured;
    }
    
    bool is_zero() const { 
        return _has_value && !_bit_value; 
    }
    
    bool is_one() const { 
        return _has_value && _bit_value; 
    }
    
    bool is_unknown() const { 
        return !_has_value && !_measured; 
    }
    
    // State transition methods
    void measure_to_classical(bool result, QCirGate* measurement_gate = nullptr) {
        _has_value = true;
        _bit_value = result;
        _measured = true;
        _measurement_gate = measurement_gate;
    }
    
    // Just mark as measured without setting a value
    void mark_as_measured(QCirGate* measurement_gate = nullptr) {
        _measured = true;
        _measurement_gate = measurement_gate;
        // Note: _has_value remains false until measurement result is known
    }
    
    // Utility methods
    std::string get_state_string() const {
        if (is_zero()) return "0";
        if (is_one()) return "1";
        if (is_measured() && !_has_value) return "measured(unknown)";
        if (is_unknown()) return "unknown";
        return "unknown";
    }
    
    // Get the actual bit value (0 or 1) if known, otherwise return -1
    int get_bit_value() const {
        if (_has_value) return _bit_value ? 1 : 0;
        return -1;  // Unknown
    }

private:
    bool _has_value = false;        // Whether the bit has been given a value (0 or 1)
    bool _bit_value = false;        // The actual bit value (0 or 1) when _has_value is true
    bool _measured = false;         // Whether the bit has been measured
    QCirGate* _measurement_gate = nullptr;  // Pointer to the gate that measured this bit
};

}  // namespace qsyn::qcir