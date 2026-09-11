// SPDX-License-Identifier: MIT
#include "cppgridsynth/quantum_circuit.hpp"

#include <cmath>
#include <sstream>

#include "cppgridsynth/mymath.hpp"

namespace cppgridsynth {

void QuantumCircuit::normalize_phase() {
    MPFloat two_pi = MPFloat(2) * MPFloat::pi();
    MPFloat q = phase_ / two_pi;
    mpz_class qf = floor(q);
    phase_ = phase_ - MPFloat(qf) * two_pi;
}

QuantumCircuit& QuantumCircuit::operator+=(const QuantumCircuit& o) {
    phase_ += o.phase_;
    normalize_phase();
    gates_.insert(gates_.end(), o.gates_.begin(), o.gates_.end());
    return *this;
}

QuantumCircuit& QuantumCircuit::operator+=(const container& o) {
    gates_.insert(gates_.end(), o.begin(), o.end());
    return *this;
}

std::string QuantumCircuit::to_simple_str() const {
    std::string out;
    for (const auto& g : gates_) out += g->to_simple_str();
    return out;
}

std::string QuantumCircuit::to_string() const {
    std::ostringstream os;
    os << "exp(i·" << phase_.to_string() << ") · ";
    bool first = true;
    for (const auto& g : gates_) {
        if (!first) os << " · ";
        os << g->to_string();
        first = false;
    }
    return os.str();
}

void QuantumCircuit::decompose_phase_gate() {
    MPFloat two_pi = MPFloat(2) * MPFloat::pi();
    MPFloat phase_mod = phase_ - MPFloat(floor(phase_ / two_pi)) * two_pi;
    phase_ = phase_mod;

    MPFloat one_w = MPFloat::pi() / MPFloat(4);
    MPFloat ratio = phase_ / one_w;
    long n = static_cast<long>(std::round(ratio.to_double()));
    for (long i = 0; i < n; ++i) gates_.push_back(std::make_shared<WGate>());
    phase_ = MPFloat();
}

}  // namespace cppgridsynth
