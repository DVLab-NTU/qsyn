/****************************************************************************
  FileName     [ checker.h ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Checker structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef CHECKER_H
#define CHECKER_H

#include "circuitTopology.h"
#include "device.h"

class Checker {
public:
    Checker(CircuitTopo&, Device&, const std::vector<Operation>&, const std::vector<size_t>&, bool = true);

    size_t getCycle(GateType);

    void applyGate(const Operation&, PhysicalQubit&);
    void applyGate(const Operation&, PhysicalQubit&, PhysicalQubit&);
    void applySwap(const Operation&);
    bool applyCX(const Operation&, const Gate&);
    bool applySingle(const Operation&, const Gate&);

    bool testOperations();

private:
    CircuitTopo& _topo;
    Device& _device;
    const std::vector<Operation>& _ops;
    bool _tqdm;
};

#endif