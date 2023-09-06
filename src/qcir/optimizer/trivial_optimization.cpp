/****************************************************************************
  FileName     [ trivial_optimization.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Implement the trivial optimization ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>

#include "../qcir.hpp"
#include "../qcirGate.hpp"
#include "./optimizer.hpp"
#include "util/logger.hpp"

extern dvlab::utils::Logger logger;
extern bool stop_requested();

/**
 * @brief Trivial optimization
 *
 * @return QCir*
 */
std::optional<QCir> Optimizer::trivial_optimization(QCir const& qcir) {
    logger.info("Start trivial optimization");

    reset(qcir);
    QCir result;
    result.setFileName(qcir.getFileName());
    result.addProcedures(qcir.getProcedures());
    result.addQubit(qcir.getNQubit());

    std::vector<QCirGate*> gateList = qcir.getTopoOrderedGates();
    for (auto gate : gateList) {
        if (stop_requested()) {
            logger.warning("optimization interrupted");
            return std::nullopt;
        }
        std::vector<QCirGate*> LastLayer = getFirstLayerGates(result, true);
        size_t qubit = gate->getTarget()._qubit;
        if (LastLayer[qubit] == nullptr) {
            Optimizer::_addGate2Circuit(result, gate, false);
            continue;
        }
        QCirGate* previousGate = LastLayer[qubit];
        if (isDoubleQubitGate(gate)) {
            size_t q2 = gate->getTarget()._qubit;
            if (previousGate->getId() != LastLayer[q2]->getId()) {
                // 2-qubit gate do not match up
                Optimizer::_addGate2Circuit(result, gate, false);
                continue;
            }
            cancelDoubleGate(result, previousGate, gate);
        } else if (isSingleRotateZ(gate) && isSingleRotateZ(previousGate)) {
            FuseZPhase(result, previousGate, gate);
        } else if (gate->getType() == previousGate->getType()) {
            result.removeGate(previousGate->getId());
        } else {
            Optimizer::_addGate2Circuit(result, gate, false);
        }
    }
    logger.info("Finished trivial optimization");
    return result;
}

/**
 * @brief Get the first layer of the circuit (nullptr if the qubit is empty at the layer)
 * @param QC: the input circuit
 * @param fromLast: Get the last layer
 *
 * @return vector<QCirGate*> with size = circuit->getNqubit()
 */
std::vector<QCirGate*> Optimizer::getFirstLayerGates(QCir& qcir, bool fromLast) {
    std::vector<QCirGate*> gateList = qcir.updateTopoOrder();
    if (fromLast) reverse(gateList.begin(), gateList.end());
    std::vector<QCirGate*> result;
    std::vector<bool> blocked;
    for (size_t i = 0; i < qcir.getNQubit(); i++) {
        result.emplace_back(nullptr);
        blocked.emplace_back(false);
    }

    for (auto gate : gateList) {
        std::vector<BitInfo> qubits = gate->getQubits();
        bool gateIsNotBlocked = all_of(qubits.begin(), qubits.end(), [&](BitInfo qubit) { return blocked[qubit._qubit] == false; });
        for (auto q : qubits) {
            size_t qubit = q._qubit;
            if (gateIsNotBlocked) result[qubit] = gate;
            blocked[qubit] = true;
        }
        if (all_of(blocked.begin(), blocked.end(), [](bool block) { return block; })) break;
    }

    return result;
}

/**
 * @brief Fuse the incoming ZPhase gate with the last layer in circuit
 * @param QC: the circuit
 * @param previousGate: previous gate
 * @param gate: the incoming gate
 *
 * @return modified circuit
 */
void Optimizer::FuseZPhase(QCir& qcir, QCirGate* previousGate, QCirGate* gate) {
    Phase p = previousGate->getPhase() + gate->getPhase();
    if (p == Phase(0)) {
        qcir.removeGate(previousGate->getId());
        return;
    }
    if (previousGate->getType() == GateType::P)
        previousGate->setRotatePhase(p);
    else {
        std::vector<size_t> qubit_list;
        qubit_list.emplace_back(previousGate->getTarget()._qubit);
        qcir.removeGate(previousGate->getId());
        qcir.addGate("p", qubit_list, p, true);
    }
}

/**
 * @brief Cancel if CX-CX / CZ-CZ, otherwise append it.
 * @param QC: the circuit
 * @param previousGate: previous gate
 * @param gate: the incoming gate
 *
 * @return modified circuit
 */
void Optimizer::cancelDoubleGate(QCir& qcir, QCirGate* previousGate, QCirGate* gate) {
    if (previousGate->getType() != gate->getType()) {
        Optimizer::_addGate2Circuit(qcir, gate, false);
        return;
    }
    if (previousGate->getType() == GateType::CZ || previousGate->getControl()._qubit == gate->getControl()._qubit)
        qcir.removeGate(previousGate->getId());
    else
        Optimizer::_addGate2Circuit(qcir, gate, false);
}
