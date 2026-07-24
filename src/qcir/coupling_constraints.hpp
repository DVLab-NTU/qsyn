/****************************************************************************
  PackageName  [ qcir / coupling_constraints ]
  Synopsis     [ Allowed CX edges for topology-aware gate deletion. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <set>
#include <utility>
#include <vector>

#include "qcir/qcir.hpp"

namespace qsyn::device {
class Device;
}

namespace qsyn::qcir {

struct CouplingConstraints {
    bool all_to_all = true;
    // Directed CX edges (control, target) in logical qubit indices.
    std::set<std::pair<QubitIdType, QubitIdType>> allowed_cx;

    [[nodiscard]] bool allows_cx(QubitIdType ctrl, QubitIdType targ) const;
};

// All-to-all on n logical qubits.
[[nodiscard]] CouplingConstraints all_to_all_coupling(size_t n_qubits);

// Build from device physical adjacency (logical i maps to physical i for i < n).
[[nodiscard]] CouplingConstraints coupling_from_device(device::Device const& dev,
                                                       size_t n_logical_qubits);

[[nodiscard]] bool circuit_respects_coupling(QCir const& circ, CouplingConstraints const& cx);

}  // namespace qsyn::qcir
