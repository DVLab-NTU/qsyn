// SPDX-License-Identifier: MIT
#include "cppgridsynth/synthesis_of_cliffordT.hpp"

#include <stdexcept>

#include "cppgridsynth/normal_form.hpp"
#include "cppgridsynth/quantum_gate.hpp"
#include "cppgridsynth/ring.hpp"

namespace cppgridsynth {

namespace {

constexpr std::array<int, 16> BIT_SHIFT = {0, 0, 1, 0, 2, 0, 1, 3, 3, 3, 0, 2, 2, 1, 0, 0};
constexpr std::array<int, 16> BIT_COUNT = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

QuantumCircuit::container t_power_and_h(int m, int target) {
    QuantumCircuit::container out;
    switch (m) {
        case 0: out.push_back(std::make_shared<HGate>(target)); break;
        case 1: out.push_back(std::make_shared<TGate>(target));
                out.push_back(std::make_shared<HGate>(target)); break;
        case 2: out.push_back(std::make_shared<SGate>(target));
                out.push_back(std::make_shared<HGate>(target)); break;
        case 3: out.push_back(std::make_shared<TGate>(target));
                out.push_back(std::make_shared<SGate>(target));
                out.push_back(std::make_shared<HGate>(target)); break;
        default: throw std::invalid_argument("t_power_and_h: bad m");
    }
    return out;
}

std::pair<QuantumCircuit::container, DOmegaUnitary>
reduce_denomexp_step(const DOmegaUnitary& unitary, const std::vector<int>& wires) {
    int residue_z = unitary.z().residue();
    int residue_w = unitary.w().residue();
    int residue_squared_z = (unitary.z().u() * unitary.z().conj().u()).residue();

    int m = BIT_SHIFT[residue_w] - BIT_SHIFT[residue_z];
    if (m < 0) m += 4;

    if (residue_squared_z == 0b0000) {
        DOmegaUnitary u2 = unitary.mul_by_H_and_T_power_from_left(0).renew_denomexp(unitary.k() - 1);
        return {t_power_and_h(0, wires[0]), u2};
    }
    if (residue_squared_z == 0b1010) {
        DOmegaUnitary u2 = unitary.mul_by_H_and_T_power_from_left(-m).renew_denomexp(unitary.k() - 1);
        return {t_power_and_h(m, wires[0]), u2};
    }
    if (residue_squared_z == 0b0001) {
        if (BIT_COUNT[residue_z] == BIT_COUNT[residue_w]) {
            DOmegaUnitary u2 = unitary.mul_by_H_and_T_power_from_left(-m).renew_denomexp(unitary.k() - 1);
            return {t_power_and_h(m, wires[0]), u2};
        }
        DOmegaUnitary u2 = unitary.mul_by_H_and_T_power_from_left(-m);
        return {t_power_and_h(m, wires[0]), u2};
    }
    throw std::runtime_error("reduce_denomexp_step: unexpected residue");
}

}  // namespace

QuantumCircuit decompose_domega_unitary(DOmegaUnitary unitary,
                                        const std::vector<int>& wires,
                                        bool up_to_phase) {
    QuantumCircuit circuit;
    while (unitary.k() > 0) {
        auto [g, u2] = reduce_denomexp_step(unitary, wires);
        unitary = u2;
        circuit += g;
    }

    if (unitary.n() & 1) {
        circuit.append(std::make_shared<TGate>(wires[0]));
        unitary = unitary.mul_by_T_inv_from_left();
    }
    if (unitary.z() == DOmega::from_int(0)) {
        circuit.append(std::make_shared<SXGate>(wires[0]));
        unitary = unitary.mul_by_X_from_left();
    }

    int m_W = 0;
    for (int m = 0; m < 8; ++m) {
        if (unitary.z() == DOmega(OMEGA_POWER()[m], 0)) {
            m_W = m;
            unitary = unitary.mul_by_W_power_from_left(-m_W);
            break;
        }
    }

    int m_S = static_cast<int>(unitary.n() >> 1);
    for (int i = 0; i < m_S; ++i) circuit.append(std::make_shared<SGate>(wires[0]));
    unitary = unitary.mul_by_S_power_from_left(-m_S);

    if (up_to_phase) {
        circuit.set_phase(MPFloat(m_W) * w_phase());
    } else {
        for (int i = 0; i < m_W; ++i) circuit.append(std::make_shared<WGate>());
    }

    if (!(unitary == DOmegaUnitary::identity())) {
        throw std::runtime_error("decompose_domega_unitary: decomposition failed");
    }

    NormalForm nf = NormalForm::from_circuit(circuit);
    return nf.to_circuit(wires);
}

}  // namespace cppgridsynth
