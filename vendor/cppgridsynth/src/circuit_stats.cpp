// SPDX-License-Identifier: MIT
#include "cppgridsynth/circuit_stats.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "cppgridsynth/quantum_circuit.hpp"
#include "cppgridsynth/quantum_gate.hpp"

namespace cppgridsynth {
namespace {

using QubitLayers = std::map<int, size_t>;

size_t schedule_on_qubits(QubitLayers& layers, const std::vector<int>& qubits) {
    if (qubits.empty()) {
        size_t max_layer = 0;
        for (const auto& [_, layer] : layers) {
            max_layer = std::max(max_layer, layer);
        }
        return max_layer + 1;
    }

    size_t layer = 1;
    for (int q : qubits) {
        auto it = layers.find(q);
        const size_t prev = (it == layers.end()) ? 0 : it->second;
        layer = std::max(layer, prev + 1);
    }
    for (int q : qubits) {
        layers[q] = layer;
    }
    return layer;
}

void record(GateKindStats& stats, QubitLayers& layers, const std::vector<int>& qubits) {
    ++stats.count;
    const size_t layer = schedule_on_qubits(layers, qubits);
    stats.depth = std::max(stats.depth, layer);
}

bool is_clifford_t_kind(GateKind kind) {
    switch (kind) {
        case GateKind::S:
        case GateKind::H:
        case GateKind::T:
        case GateKind::X:
        case GateKind::W:
            return true;
        default:
            return false;
    }
}

GateKindStats& stats_for_kind(CircuitStats& out, GateKind kind) {
    switch (kind) {
        case GateKind::S: return out.s;
        case GateKind::H: return out.h;
        case GateKind::T: return out.t;
        case GateKind::X: return out.x;
        case GateKind::W: return out.w;
        default:
            throw std::logic_error("stats_for_kind: not a Clifford+T gate");
    }
}

GateKind char_to_kind(char c) {
    switch (c) {
        case 'S': return GateKind::S;
        case 'H': return GateKind::H;
        case 'T': return GateKind::T;
        case 'X': return GateKind::X;
        case 'W': return GateKind::W;
        default:
            throw std::invalid_argument(
                std::string("analyze_clifford_t_gates: unknown gate '") + c + "'");
    }
}

void analyze_gate(CircuitStats& out,
                  QubitLayers& total_layers,
                  std::unordered_map<int, QubitLayers>& kind_layers,
                  GateKind kind,
                  const std::vector<int>& qubits) {
    record(out.total, total_layers, qubits);
    record(stats_for_kind(out, kind), kind_layers[static_cast<int>(kind)], qubits);
}

}  // namespace

CircuitStats analyze_clifford_t_gates(const std::string& gates) {
    CircuitStats out;
    QubitLayers total_layers;
    std::unordered_map<int, QubitLayers> kind_layers;

    for (char c : gates) {
        GateKind kind = char_to_kind(c);
        std::vector<int> qubits;
        if (kind != GateKind::W) {
            qubits.push_back(0);
        }
        analyze_gate(out, total_layers, kind_layers, kind, qubits);
    }
    return out;
}

CircuitStats analyze_clifford_t_circuit(const QuantumCircuit& circuit) {
    CircuitStats out;
    QubitLayers total_layers;
    std::unordered_map<int, QubitLayers> kind_layers;

    for (const auto& op : circuit.gates()) {
        if (!is_clifford_t_kind(op->kind())) {
            continue;
        }
        analyze_gate(out, total_layers, kind_layers, op->kind(), op->qubits());
    }
    return out;
}

void print_circuit_stats(std::ostream& os, const CircuitStats& stats) {
    auto line = [&](const char* label, const GateKindStats& s) {
        os << "[stats] " << label << ": count=" << s.count << " depth=" << s.depth << "\n";
    };
    line("total", stats.total);
    line("S", stats.s);
    line("H", stats.h);
    line("T", stats.t);
    line("X", stats.x);
    line("W", stats.w);
}

}  // namespace cppgridsynth
