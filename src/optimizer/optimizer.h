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
        _doSwap = true;
        _separateCorrection = false;
        _maxIter = 1000;
    }
    ~Optimizer() {}

    void reset();
    QCir* parseCircuit(bool, bool, size_t);
    QCir* parseForward();
    bool parseGate(QCirGate*);

    void addHadamard(size_t);
    void addCZ(size_t, size_t);
    void addCX(size_t, size_t);
    void addGate(size_t, Phase, size_t);

    void topologicalSort(QCir*);
    bool isSingleRotateZ(QCirGate*);
    QCirGate* getAvailableRotateZ(size_t t);

    // Predicate function
    bool TwoQubitGateExist(QCirGate* g, GateType gt, size_t ctrl, size_t targ);

private:
    QCir* _circuit;
    bool _doSwap;
    bool _separateCorrection;
    bool _minimize_czs;
    size_t _maxIter;
    Qubit2Gates _gates;
    Qubit2Gates _available;
    std::vector<QCirGate*> _corrections;
    std::vector<size_t> _availty;  // TODO - checkout if vector<bool> is availiable too. 

    std::unordered_map<size_t, size_t> _permutation;
    ordered_hashset<size_t> _hadamards;
    ordered_hashset<size_t> _xs;  // NOTE - nots
    ordered_hashset<size_t> _zs;

    size_t _gateCnt;  // NOTE - gcount
    void toggleElement(size_t type, size_t element);
    void swapElement(size_t type, size_t e1, size_t e2);
};

#endif