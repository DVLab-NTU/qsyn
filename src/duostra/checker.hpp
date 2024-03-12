/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Checker structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "device/device.hpp"
#include "qcir/qcir_gate.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::duostra {

class CircuitTopology;

class Checker {
public:
    using Device        = qsyn::device::Device;
    using PhysicalQubit = qsyn::device::PhysicalQubit;
    Checker(CircuitTopology& topo,
            Checker::Device& device,
            std::span<device::Operation const> ops,
            std::vector<QubitIdType> const& assign, bool tqdm = true);

    size_t get_cycle(device::Operation const& op);

    void apply_gate(device::Operation const& op, PhysicalQubit& q0);
    void apply_gate(device::Operation const& op, PhysicalQubit& q0, PhysicalQubit& q1);
    void apply_swap(device::Operation const& op);
    bool apply_cx(device::Operation const& op, qcir::QCirGate const& gate);
    bool apply_single(device::Operation const& op, qcir::QCirGate const& gate);

    bool test_operations();

private:
    CircuitTopology* _topo;
    Device* _device;
    std::span<device::Operation const> _ops;
    bool _tqdm;
};

}  // namespace qsyn::duostra
