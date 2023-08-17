/****************************************************************************
  FileName     [ gate_optimization.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define optimization of gates ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>

#include <cassert>

#include "./optimizer.hpp"
#include "../gateType.hpp"
#include "../qcirGate.hpp"
#include "util/logger.hpp"

extern dvlab_utils::Logger logger;

void Optimizer::permuteGate(QCirGate* gate) {
    auto qubits = gate->getQubits();

    for (size_t i = 0; i < qubits.size(); i++) {
        for (auto& [j, k] : _permutation) {
            if (k == qubits[i]._qubit) {
                qubits[i]._qubit = j;
                break;
            }
        }
    }

    gate->setQubits(qubits);
}

void Optimizer::matchHadamard(QCirGate* gate) {
    assert(gate->getType() == GateType::H);
    size_t qubit = gate->getTarget()._qubit;
    if (_xs.contains(qubit) && !_zs.contains(qubit)) {
        logger.trace("Transform X gate into Z gate");

        _xs.erase(qubit);
        _zs.emplace(qubit);
    } else if (!_xs.contains(qubit) && _zs.contains(qubit)) {
        logger.trace("Transform Z gate into X gate");
        _zs.erase(qubit);
        _xs.emplace(qubit);
    }
    // NOTE - H-S-H to Sdg-H-Sdg
    if (_gates[qubit].size() > 1 && _gates[qubit][_gates[qubit].size() - 2]->getType() == GateType::H && isSingleRotateZ(_gates[qubit][_gates[qubit].size() - 1])) {
        QCirGate* g2 = _gates[qubit][_gates[qubit].size() - 1];
        if (g2->getPhase().denominator() == 2) {
            _statistics.HS_EXCHANGE++;
            logger.trace("Transform H-S-H into Sdg-H-Sdg");
            QCirGate* zp = new PGate(_gateCnt);
            zp->addQubit(qubit, true);
            _gateCnt++;
            zp->setRotatePhase(-1 * g2->getPhase());  // NOTE - S to Sdg
            g2->setRotatePhase(zp->getPhase());       // NOTE - S to Sdg
            _gates[qubit].insert(_gates[qubit].end() - 2, zp);
            return;
        }
    }
    toggleElement(GateType::H, qubit);
}

void Optimizer::matchX(QCirGate* gate) {
    assert(gate->getType() == GateType::X);
    size_t qubit = gate->getTarget()._qubit;
    if (_xs.contains(qubit)) {
        logger.trace("Cancel X-X into Id");
        _statistics.X_CANCEL++;
    }
    toggleElement(GateType::X, qubit);
}

void Optimizer::matchRotateZ(QCirGate* gate) {
    assert(isSingleRotateZ(gate));
    size_t qubit = gate->getTarget()._qubit;
    if (_zs.contains(qubit)) {
        _statistics.FUSE_PHASE++;
        _zs.erase(qubit);
        if (gate->getType() == GateType::RZ || gate->getType() == GateType::P) {
            gate->setRotatePhase(gate->getPhase() + Phase(1));
        } else if (gate->getType() == GateType::Z) {
            return;
        } else {
            // NOTE - Trans S/S*/T/T* into PGate
            QCirGate* temp = new PGate(_gateCnt);
            _gateCnt++;
            temp->addQubit(qubit, true);
            temp->setRotatePhase(gate->getPhase() + Phase(1));
            gate = temp;
        }
    }
    if (gate->getPhase() == Phase(0)) {
        logger.trace("Cancel with previous RZ");
        return;
    }

    if (_xs.contains(qubit)) {
        gate->setRotatePhase(-1 * (gate->getPhase()));
    }
    if (gate->getPhase() == Phase(1) || gate->getType() == GateType::Z) {
        toggleElement(GateType::Z, qubit);
        return;
    }
    // REVIEW - Neglect adjoint due to S and Sdg is separated
    if (_hadamards.contains(qubit)) {
        addHadamard(qubit, true);
    }
    QCirGate* available = getAvailableRotateZ(qubit);
    if (_availty[qubit] == false && available != nullptr) {
        std::erase(_available[qubit], available);
        std::erase(_gates[qubit], available);
        Phase ph = available->getPhase() + gate->getPhase();
        _statistics.FUSE_PHASE++;
        if (ph == Phase(1)) {
            toggleElement(GateType::Z, qubit);
            return;
        }
        if (ph != Phase(0)) {
            addRotateGate(qubit, ph, GateType::P);
        }
    } else {
        if (_availty[qubit] == true) {
            _availty[qubit] = false;
            _available[qubit].clear();
        }
        addRotateGate(qubit, gate->getPhase(), GateType::P);
    }
}

void Optimizer::matchCZ(QCirGate* gate, bool doSwap, bool minimizeCZ) {
    assert(gate->getType() == GateType::CZ);
    size_t controlQubit = gate->getControl()._qubit;
    size_t targetQubit = gate->getTarget()._qubit;
    if (controlQubit > targetQubit) {  // NOTE - Symmetric, let ctrl smaller than targ
        size_t tmp = controlQubit;
        gate->setTargetBit(targetQubit);
        gate->setControlBit(tmp);
    }
    // NOTE - Push NOT gates trough the CZ
    // REVIEW - Seems strange
    if (_xs.contains(controlQubit))
        toggleElement(GateType::Z, targetQubit);
    if (_xs.contains(targetQubit))
        toggleElement(GateType::Z, controlQubit);
    if (_hadamards.contains(controlQubit) && _hadamards.contains(targetQubit)) {
        addHadamard(controlQubit, true);
        addHadamard(targetQubit, true);
    }
    if (!_hadamards.contains(controlQubit) && !_hadamards.contains(targetQubit)) {
        addCZ(controlQubit, targetQubit, minimizeCZ);
    } else if (_hadamards.contains(controlQubit)) {
        _statistics.CZ2CX++;
        addCX(targetQubit, controlQubit, doSwap);
    } else {
        _statistics.CZ2CX++;
        addCX(controlQubit, targetQubit, doSwap);
    }
}

void Optimizer::matchCX(QCirGate* gate, bool doSwap, bool minimizeCZ) {
    assert(gate->getType() == GateType::CX);
    size_t controlQubit = gate->getControl()._qubit;
    size_t targetQubit = gate->getTarget()._qubit;

    if (_xs.contains(controlQubit))
        toggleElement(GateType::X, targetQubit);
    if (_zs.contains(targetQubit))
        toggleElement(GateType::Z, controlQubit);
    if (_hadamards.contains(controlQubit) && _hadamards.contains(targetQubit)) {
        addCX(targetQubit, controlQubit, doSwap);
    } else if (!_hadamards.contains(controlQubit) && !_hadamards.contains(targetQubit)) {
        addCX(controlQubit, targetQubit, doSwap);
    } else if (_hadamards.contains(targetQubit)) {
        _statistics.CX2CZ++;
        if (controlQubit > targetQubit)
            addCZ(targetQubit, controlQubit, minimizeCZ);
        else
            addCZ(controlQubit, targetQubit, minimizeCZ);
    } else {
        addHadamard(controlQubit, true);
        addCX(controlQubit, targetQubit, doSwap);
    }
}
