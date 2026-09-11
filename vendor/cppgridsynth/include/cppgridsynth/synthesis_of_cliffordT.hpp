// SPDX-License-Identifier: MIT
//
// Decomposition of a DOmegaUnitary into the Clifford+T gate set.
// Faithful port of pygridsynth/synthesis_of_cliffordT.py.
#pragma once

#include <vector>

#include "cppgridsynth/domega_unitary.hpp"
#include "cppgridsynth/quantum_circuit.hpp"

namespace cppgridsynth {

QuantumCircuit decompose_domega_unitary(DOmegaUnitary unitary,
                                        const std::vector<int>& wires,
                                        bool up_to_phase = false);

}  // namespace cppgridsynth
