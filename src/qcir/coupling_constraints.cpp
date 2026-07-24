/****************************************************************************
  PackageName  [ qcir / coupling_constraints ]
  Synopsis     [ CouplingConstraints helpers. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./coupling_constraints.hpp"

#include <spdlog/spdlog.h>

#include "device/device.hpp"
#include "qcir/basic_gate_type.hpp"

namespace qsyn::qcir {

bool CouplingConstraints::allows_cx(QubitIdType ctrl, QubitIdType targ) const {
    if (all_to_all) return true;
    return allowed_cx.contains({ctrl, targ});
}

CouplingConstraints all_to_all_coupling(size_t /*n_qubits*/) {
    return CouplingConstraints{.all_to_all = true};
}

CouplingConstraints coupling_from_device(device::Device const& dev, size_t n_logical_qubits) {
    CouplingConstraints out;
    out.all_to_all = false;

    auto const& qubits = dev.get_physical_qubit_list();
    for (size_t a = 0; a < qubits.size(); ++a) {
        for (auto const b : qubits[a].get_adjacencies()) {
            if (static_cast<size_t>(a) < n_logical_qubits && static_cast<size_t>(b) < n_logical_qubits) {
                out.allowed_cx.emplace(static_cast<QubitIdType>(a), static_cast<QubitIdType>(b));
                out.allowed_cx.emplace(static_cast<QubitIdType>(b), static_cast<QubitIdType>(a));
            }
        }
    }

    if (out.allowed_cx.empty()) {
        spdlog::warn("coupling_from_device: no edges for {} logical qubits; using all-to-all.",
                     n_logical_qubits);
        out.all_to_all = true;
    }
    return out;
}

bool circuit_respects_coupling(QCir const& circ, CouplingConstraints const& cx) {
    if (cx.all_to_all) return true;
    for (auto const* g : circ.get_gates()) {
        if (g->get_num_qubits() < 2) continue;
        auto const t = g->get_operation().get_type();
        if (t.size() < 2 || t[0] != 'c' || t[1] != 'x') continue;
        auto qs = g->get_qubits();
        if (qs.size() < 2) continue;
        if (!cx.allows_cx(qs[0], qs[1])) return false;
    }
    return true;
}

}  // namespace qsyn::qcir
