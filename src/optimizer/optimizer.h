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

#include "ordered_hashset.h"
#include "qcir.h"  // for QCir

class QCir;

using Qubit2Gates = std::unordered_map<size_t, std::vector<QCirGate*>>;

class Optimizer {
public:
    Optimizer(QCir* c = nullptr) {
        _circuit = c;
        reset();
    }
    ~Optimizer() {}

    void reset();
    QCir* parseCircuit();
    QCir* parseForward();
    bool parseGate(QCirGate*);

    void addHadamard(size_t);
    void addCZ(size_t, size_t);
    void addCX(size_t, size_t);
    void addGate(size_t, Phase, size_t);

    void topologicalSort();
    bool isSingleRotateZ(QCirGate*);
    QCirGate* getAvailableRotateZ(size_t t);

private:
    QCir* _circuit;
    Qubit2Gates _gates;
    Qubit2Gates _available;
    std::vector<QCirGate*> _corrections;
    std::unordered_map<size_t, size_t> _availty;  // FIXME - Consider rename. Look like something available order.

    std::unordered_map<size_t, size_t> _permutation;
    ordered_hashset<size_t> _hadamards;
    ordered_hashset<size_t> _xs;  // NOTE - nots
    ordered_hashset<size_t> _zs;

    size_t _gateCnt;  // NOTE - gcount
    void toggleElement(size_t type, size_t element);
};

#endif