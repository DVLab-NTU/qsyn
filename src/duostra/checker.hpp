/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Checker structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "device/device.hpp"

namespace qsyn::duostra {

class CircuitTopology;
class Gate;

class Checker {
public:
    using Device = qsyn::device::Device;
    using Operation = qsyn::device::Operation;
    using PhysicalQubit = qsyn::device::PhysicalQubit;
    Checker(CircuitTopology&, Device&, std::vector<Operation> const& ops, std::vector<size_t> const&, bool = true);

    size_t get_cycle(qcir::GateType);

    void apply_gate(Operation const&, PhysicalQubit&);
    void apply_gate(Operation const&, PhysicalQubit&, PhysicalQubit&);
    void apply_swap(Operation const&);
    bool apply_cx(Operation const&, Gate const&);
    bool apply_single(Operation const&, Gate const&);

    bool test_operations();

private:
    CircuitTopology* _topo;
    Device* _device;
    std::vector<Operation> const& _ops;
    bool _tqdm;
};

}  // namespace qsyn::duostra
