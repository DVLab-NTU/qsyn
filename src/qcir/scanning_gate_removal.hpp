/****************************************************************************
  PackageName  [ qcir / scanning_gate_removal ]
  Synopsis     [ BQSKit ScanningGateRemovalPass-style redundant gate deletion. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/coupling_constraints.hpp"
#include "qcir/qcir.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn::qcir {

struct ScanningGateRemovalOptions {
    double synthesis_epsilon = 1e-8;
    int max_passes           = 8;
    bool remove_u3           = true;
    bool remove_cx           = true;
    CouplingConstraints coupling{};
};

// Hilbert–Schmidt residual 1 - |<target|trial>| (same as qfactor).
[[nodiscard]] double unitary_residual(QCir const& trial, tensor::QTensor<double> const& target);

// One forward scan; removes gates whose deletion keeps residual <= epsilon.
[[nodiscard]] QCir scanning_gate_removal_pass(QCir const& compiled,
                                              tensor::QTensor<double> const& target_unitary,
                                              ScanningGateRemovalOptions const& opt = {});

// BQSKit optimization_level>=2 workflow: repeat passes until stable.
[[nodiscard]] QCir scanning_gate_removal_workflow(QCir const& compiled,
                                                  tensor::QTensor<double> const& target_unitary,
                                                  ScanningGateRemovalOptions const& opt = {});

// BQSKit retarget + gate-deletion weave: forward scan, optional CX flip for coupling.
[[nodiscard]] QCir gate_deletion_optimization_workflow(
    QCir const& compiled, tensor::QTensor<double> const& target_unitary,
    ScanningGateRemovalOptions const& opt = {});

}  // namespace qsyn::qcir
