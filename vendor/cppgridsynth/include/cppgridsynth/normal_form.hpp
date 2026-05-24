// SPDX-License-Identifier: MIT
//
// Matsumoto-Amano normal form for single-qubit Clifford+T circuits.
// Faithful port of pygridsynth/normal_form.py.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "cppgridsynth/quantum_circuit.hpp"
#include "cppgridsynth/quantum_gate.hpp"

namespace cppgridsynth {

enum class Axis : uint8_t { I = 0, H = 1, SH = 2 };
enum class Syllable : uint8_t { I = 0, T = 1, HT = 2, SHT = 3 };

class Clifford {
public:
    Clifford(int a = 0, int b = 0, int c = 0, int d = 0);

    int a() const noexcept { return a_; }
    int b() const noexcept { return b_; }
    int c() const noexcept { return c_; }
    int d() const noexcept { return d_; }

    static Clifford from_str(const std::string& g);
    static Clifford from_gate(const Operation& g);

    bool operator==(const Clifford& o) const {
        return a_ == o.a_ && b_ == o.b_ && c_ == o.c_ && d_ == o.d_;
    }

    Clifford operator*(const Clifford& o) const;
    Clifford inv() const;

    // Decomposition helpers used by the normal-form construction.
    std::pair<Axis, Clifford> decompose_coset() const;
    std::pair<Axis, Clifford> decompose_tconj() const;

    QuantumCircuit to_circuit(const std::vector<int>& wires) const;

    std::string to_string() const;

private:
    int a_, b_, c_, d_;
};

class NormalForm {
public:
    NormalForm() : c_() {}
    NormalForm(std::vector<Syllable> syl, Clifford c, MPFloat phase = MPFloat());

    const std::vector<Syllable>& syllables() const noexcept { return syllables_; }
    const Clifford& c() const noexcept { return c_; }
    const MPFloat& phase() const noexcept { return phase_; }

    static NormalForm from_circuit(const QuantumCircuit& circuit);
    QuantumCircuit to_circuit(const std::vector<int>& wires) const;

private:
    void append_gate(const Operation& g);

    std::vector<Syllable> syllables_;
    Clifford c_;
    MPFloat phase_;
};

// Pre-defined Clifford constants.
extern const Clifford CLIFFORD_I;
extern const Clifford CLIFFORD_X;
extern const Clifford CLIFFORD_H;
extern const Clifford CLIFFORD_S;
extern const Clifford CLIFFORD_W;
extern const Clifford CLIFFORD_SH;
extern const Clifford CLIFFORD_HS;
extern const Clifford CLIFFORD_SHS;

}  // namespace cppgridsynth
