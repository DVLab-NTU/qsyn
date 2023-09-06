/****************************************************************************
  FileName     [ optimizer.hpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define class Optimizer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <set>
#include <unordered_map>

#include "util/ordered_hashset.hpp"

class QCir;
class QCirGate;
class Phase;
enum class GateType;

using Qubit2Gates = std::unordered_map<size_t, std::vector<QCirGate*>>;

class Optimizer {
public:
    Optimizer() {}

    void reset(QCir const& qcir);

    // Predicate function && Utils
    bool TwoQubitGateExist(QCirGate* g, GateType gt, size_t ctrl, size_t targ);
    bool isSingleRotateZ(QCirGate*);
    bool isSingleRotateX(QCirGate*);
    bool isDoubleQubitGate(QCirGate*);
    QCirGate* getAvailableRotateZ(size_t t);

    // basic optimization
    struct BasicOptimizationConfig {
        bool doSwap;
        bool separateCorrection;
        size_t maxIter;
        bool printStatistics;
    };
    std::optional<QCir> basic_optimization(QCir const& qcir, BasicOptimizationConfig const& config);
    QCir parseForward(QCir const& qcir, bool minimizeCZ, BasicOptimizationConfig const& config);
    QCir parseBackward(QCir const& qcir, bool minimizeCZ, BasicOptimizationConfig const& config);
    bool parseGate(QCirGate*, bool doSwap, bool minimizeCZ);

    // trivial optimization
    std::optional<QCir> trivial_optimization(QCir const& qcir);

private:
    size_t _iter;
    Qubit2Gates _gates;
    Qubit2Gates _available;
    std::vector<bool> _availty;

    std::unordered_map<size_t, size_t> _permutation;
    ordered_hashset<size_t> _hadamards;
    ordered_hashset<size_t> _xs;  // NOTE - nots
    ordered_hashset<size_t> _zs;
    std::vector<std::pair<size_t, size_t>> _swaps;

    size_t _gateCnt;  // NOTE - gcount

    struct Statistics {
        size_t FUSE_PHASE = 0;
        size_t X_CANCEL = 0;
        size_t CNOT_CANCEL = 0;
        size_t CZ_CANCEL = 0;
        size_t HS_EXCHANGE = 0;
        size_t CRZ_TRACSFORM = 0;
        size_t DO_SWAP = 0;
        size_t CZ2CX = 0;
        size_t CX2CZ = 0;
    } _statistics;

    // Utils
    void toggleElement(GateType const& type, size_t element);
    void swapElement(size_t type, size_t e1, size_t e2);
    static std::vector<size_t> computeStats(QCir const& circuit);

    QCir parseOnce(QCir const& qcir, bool reversed, bool minimizeCZ, BasicOptimizationConfig const& config);

    // basic optimization subroutines

    void permuteGate(QCirGate* gate);

    void matchHadamard(QCirGate* gate);
    void matchX(QCirGate* gate);
    void matchRotateZ(QCirGate* gate);
    void matchCZ(QCirGate* gate, bool doSwap, bool minimizeCZ);
    void matchCX(QCirGate* gate, bool doSwap, bool minimizeCZ);

    void addHadamard(size_t, bool erase);
    bool replace_CX_and_CZ_with_S_and_CX(size_t t1, size_t t2);
    void addCZ(size_t t1, size_t t2, bool minimizeCZ);
    void addCX(size_t t1, size_t t2, bool doSwap);
    void addRotateGate(size_t target, Phase ph, GateType const& type);

    QCir fromStorage(size_t nQubits, bool reversed);

    std::vector<std::pair<size_t, size_t>> get_swap_path();
    void _addGate2Circuit(QCir& circuit, QCirGate* gate, bool prepend);

    // trivial optimization subroutines

    std::vector<QCirGate*> getFirstLayerGates(QCir& qcir, bool fromLast = false);
    void cancelDoubleGate(QCir& qcir, QCirGate* previousGate, QCirGate* gate);
    void FuseZPhase(QCir& qcir, QCirGate* previousGate, QCirGate* gate);
};
