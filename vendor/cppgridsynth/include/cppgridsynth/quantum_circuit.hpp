// SPDX-License-Identifier: MIT
//
// QuantumCircuit: an ordered sequence of `OperationPtr` plus a global
// phase.  Mirrors pygridsynth's `QuantumCircuit` and is intentionally
// compatible with the qsyn-style polymorphic gate hierarchy.
#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cppgridsynth/mpfloat.hpp"
#include "cppgridsynth/quantum_gate.hpp"

namespace cppgridsynth {

class QuantumCircuit {
public:
    using container = std::vector<OperationPtr>;
    using iterator = container::iterator;
    using const_iterator = container::const_iterator;

    QuantumCircuit() : phase_() {}
    explicit QuantumCircuit(MPFloat phase) : phase_(std::move(phase)) { normalize_phase(); }
    QuantumCircuit(MPFloat phase, container gates)
        : phase_(std::move(phase)), gates_(std::move(gates)) { normalize_phase(); }

    static QuantumCircuit from_list(container gates) { return QuantumCircuit(MPFloat(), std::move(gates)); }

    // ---- phase ----------------------------------------------------------
    const MPFloat& phase() const noexcept { return phase_; }
    void set_phase(MPFloat phase) { phase_ = std::move(phase); normalize_phase(); }

    // ---- gate list (vector-like interface) ------------------------------
    void          append(OperationPtr g) { gates_.push_back(std::move(g)); }
    size_t        size() const noexcept { return gates_.size(); }
    bool          empty() const noexcept { return gates_.empty(); }
    OperationPtr& operator[](size_t i) { return gates_[i]; }
    const OperationPtr& operator[](size_t i) const { return gates_[i]; }
    iterator       begin()       { return gates_.begin(); }
    iterator       end()         { return gates_.end();   }
    const_iterator begin() const { return gates_.begin(); }
    const_iterator end()   const { return gates_.end();   }
    const container& gates() const noexcept { return gates_; }

    // ---- composition ----------------------------------------------------
    QuantumCircuit& operator+=(const QuantumCircuit& o);
    QuantumCircuit& operator+=(const container& o);
    friend QuantumCircuit operator+(QuantumCircuit a, const QuantumCircuit& b) { a += b; return a; }
    friend QuantumCircuit operator+(QuantumCircuit a, const container& b)      { a += b; return a; }

    // ---- conversions ----------------------------------------------------
    std::string to_simple_str() const;   // concatenates per-gate single chars
    std::string to_string() const;

    // Decompose accumulated global phase into stacked W gates (each W = e^{iπ/4}).
    void decompose_phase_gate();

private:
    void normalize_phase();

    MPFloat phase_;
    container gates_;
};

}  // namespace cppgridsynth
