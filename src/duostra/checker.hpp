/****************************************************************************
  FileName     [ checker.hpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Checker structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "device/device.hpp"

class CircuitTopo;
class Gate;

class Checker {
public:
    Checker(CircuitTopo&, Device&, std::vector<Operation> const& ops, const std::vector<size_t>&, bool = true);

    size_t getCycle(GateType);

    void applyGate(const Operation&, PhysicalQubit&);
    void applyGate(const Operation&, PhysicalQubit&, PhysicalQubit&);
    void applySwap(const Operation&);
    bool applyCX(const Operation&, const Gate&);
    bool applySingle(const Operation&, const Gate&);

    bool testOperations();

private:
    CircuitTopo* _topo;
    Device* _device;
    std::vector<Operation> const& _ops;
    bool _tqdm;
};
