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
    Optimizer(QCir* = nullptr);
    ~Optimizer() {}
    void reset();

    // Predicate function && Utils
    bool TwoQubitGateExist(QCirGate* g, GateType gt, size_t ctrl, size_t targ);
    bool isSingleRotateZ(QCirGate*);
    bool isSingleRotateX(QCirGate*);
    bool isDoubleQubitGate(QCirGate*);
    QCirGate* getAvailableRotateZ(size_t t);

    // basic optimization
    QCir* basic_optimization(bool, bool, size_t, bool);
    QCir* parseForward();
    bool parseGate(QCirGate*);
    void addHadamard(size_t, bool erase);
    void addCZ(size_t, size_t);
    void addCX(size_t, size_t);
    void topologicalSort(QCir*);

    // trivial optimization
    QCir* trivial_optimization();
    std::vector<QCirGate*> getFirstLayerGates(QCir* QC, bool fromLast = false);
    void CheckDoubleGate(QCir* QC, QCirGate* previousGate, QCirGate* gate);
    void FuseZPhase(QCir* QC, QCirGate* previousGate, QCirGate* gate);

private:
    QCir* _circuit;
    bool _doSwap;
    bool _separateCorrection;
    bool _minimize_czs;
    bool _reversed;
    bool _statistics;
    size_t _maxIter;
    size_t _iter;
    Qubit2Gates _gates;
    Qubit2Gates _available;
    std::vector<QCirGate*> _corrections;
    std::vector<bool> _availty;

    std::unordered_map<size_t, size_t> _permutation;
    ordered_hashset<size_t> _hadamards;
    ordered_hashset<size_t> _xs;  // NOTE - nots
    ordered_hashset<size_t> _zs;
    std::vector<std::pair<size_t, size_t>> _swaps;

    size_t _gateCnt;  // NOTE - gcount

    // Utils
    void toggleElement(size_t type, size_t element);
    void swapElement(size_t type, size_t e1, size_t e2);
    std::vector<size_t> stats(QCir* circuit);

    // basic optimization
    QCirGate* addGate(size_t, Phase, size_t);
    std::vector<std::pair<size_t, size_t>> get_swap_path();
    void _addGate2Circuit(QCir* circuit, QCirGate* gate);

    // NOTE - To count how many time the rule be operated.
    size_t FUSE_PHASE;
    size_t X_CANCEL;
    size_t CNOT_CANCEL;
    size_t CZ_CANCEL;
    size_t HS_EXCHANGE;
    size_t CRZ_TRACSFORM;
    size_t DO_SWAP;
    size_t CZ2CX;
    size_t CX2CZ;

    // physical
    std::string _name;
    std::vector<std::string> _procedures;
};

#endif