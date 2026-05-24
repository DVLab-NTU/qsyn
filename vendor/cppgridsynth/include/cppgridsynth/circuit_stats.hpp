// SPDX-License-Identifier: MIT
//
// Clifford+T circuit statistics: gate counts and circuit depth, both
// overall and per gate kind (S, H, T, X, W).
#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

namespace cppgridsynth {

class QuantumCircuit;

struct GateKindStats {
    size_t count = 0;
    size_t depth = 0;
};

struct CircuitStats {
    GateKindStats total;
    GateKindStats s;
    GateKindStats h;
    GateKindStats t;
    GateKindStats x;
    GateKindStats w;
};

// Analyze a gate string (characters S, H, T, X, W only).
CircuitStats analyze_clifford_t_gates(const std::string& gates);

// Analyze a QuantumCircuit (Clifford+T kinds only; other kinds are ignored).
CircuitStats analyze_clifford_t_circuit(const QuantumCircuit& circuit);

// Human-readable summary on `os` (lines prefixed with "[stats]").
void print_circuit_stats(std::ostream& os, const CircuitStats& stats);

}  // namespace cppgridsynth
