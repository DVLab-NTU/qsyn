/****************************************************************************
  FileName     [ optimizer.h ]
  PackageName  [ optimizer ]
  Synopsis     [ Define class Optimizer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#include <cstddef>  // for size_t
#include <set>
#include <unordered_map>

#include "qcir.h"  // for QCir

class QCir;

class Optimizer {
public:
    Optimizer(QCir* c = nullptr) {
        _circuit = c;
        for (size_t i = 0; i < c->getQubits().size(); i++)
            _permutation[i] = c->getQubits()[i]->getId();
    }
    ~Optimizer() {}

    QCir* parseCircuit();
    QCir* parseForward();
    void parseGate();

    void addHadamard(size_t);
    void addCZ();
    void addCX();
    void addGate();

    void topologicalSort();

private:
    QCir* _circuit;
    std::unordered_map<size_t, std::vector<QCirGate*>> _gates;
    std::unordered_map<size_t, std::vector<size_t>> _available;
    std::unordered_map<size_t, size_t> _availty;  // FIXME - Consider rename. Look like something available order.

    std::unordered_map<size_t, size_t> _permutation;
    std::vector<size_t> _hadamards;
    std::vector<size_t> _xs;  // NOTE - nots
    std::vector<size_t> _zs;

    size_t _gateCnt;  // NOTE - gcount
};

#endif