/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Implement the trivial optimization ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cassert>

#include "../qcir.hpp"
#include "../qcir_gate.hpp"
#include "./optimizer.hpp"

extern bool stop_requested();

namespace qsyn::qcir {

/**
 * @brief Trivial optimization
 *
 * @return QCir*
 */
std::optional<QCir> Optimizer::trivial_optimization(QCir const& qcir) {
    spdlog::info("Start trivial optimization");

    reset(qcir);
    QCir result;
    result.set_filename(qcir.get_filename());
    result.add_procedures(qcir.get_procedures());
    result.add_qubits(qcir.get_num_qubits());

    auto const gate_list = qcir.get_topologically_ordered_gates();
    for (auto gate : gate_list) {
        if (stop_requested()) {
            spdlog::warn("optimization interrupted");
            return std::nullopt;
        }
        auto const last_layer = _get_first_layer_gates(result, true);
        auto const qubit      = gate->get_targets()._qubit;
        if (last_layer[qubit] == nullptr) {
            Optimizer::_add_gate_to_circuit(result, gate, false);
            continue;
        }
        QCirGate* previous_gate = last_layer[qubit];
        if (is_double_qubit_gate(gate)) {
            auto const q2 = gate->get_targets()._qubit;
            if (previous_gate->get_id() != last_layer[q2]->get_id()) {
                // 2-qubit gate do not match up
                Optimizer::_add_gate_to_circuit(result, gate, false);
                continue;
            }
            _cancel_double_gate(result, previous_gate, gate);
        } else if (is_single_z_rotation(gate) && is_single_z_rotation(previous_gate)) {
            _fuse_z_phase(result, previous_gate, gate);
        } else if (gate->get_rotation_category() == previous_gate->get_rotation_category()) {
            result.remove_gate(previous_gate->get_id());
        } else {
            Optimizer::_add_gate_to_circuit(result, gate, false);
        }
    }
    spdlog::info("Finished trivial optimization");
    return result;
}

/**
 * @brief Get the first layer of the circuit (nullptr if the qubit is empty at the layer)
 * @param QC: the input circuit
 * @param fromLast: Get the last layer
 *
 * @return vector<QCirGate*> with size = circuit->getNqubit()
 */
std::vector<QCirGate*> Optimizer::_get_first_layer_gates(QCir& qcir, bool from_last) {
    std::vector<QCirGate*> gate_list = qcir.update_topological_order();
    if (from_last) reverse(gate_list.begin(), gate_list.end());
    std::vector<QCirGate*> result;
    std::vector<bool> blocked;
    for (size_t i = 0; i < qcir.get_num_qubits(); i++) {
        result.emplace_back(nullptr);
        blocked.emplace_back(false);
    }

    for (auto gate : gate_list) {
        auto const qubits              = gate->get_qubits();
        auto const gate_is_not_blocked = all_of(qubits.begin(), qubits.end(), [&](QubitInfo qubit) { return blocked[qubit._qubit] == false; });
        for (auto q : qubits) {
            if (gate_is_not_blocked) result[q._qubit] = gate;
            blocked[q._qubit] = true;
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
void Optimizer::_fuse_z_phase(QCir& qcir, QCirGate* prev_gate, QCirGate* gate) {
    auto const phase = prev_gate->get_phase() + gate->get_phase();
    if (phase == dvlab::Phase(0)) {
        qcir.remove_gate(prev_gate->get_id());
        return;
    }
    if (prev_gate->get_rotation_category() == GateRotationCategory::pz)
        prev_gate->set_phase(phase);
    else {
        QubitIdList qubit_list;
        qubit_list.emplace_back(prev_gate->get_targets()._qubit);
        qcir.remove_gate(prev_gate->get_id());
        qcir.add_gate("p", qubit_list, phase, true);
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
void Optimizer::_cancel_double_gate(QCir& qcir, QCirGate* prev_gate, QCirGate* gate) {
    if (!is_double_qubit_gate(prev_gate) || !is_double_qubit_gate(gate)) {
        Optimizer::_add_gate_to_circuit(qcir, gate, false);
        return;
    }

    auto const prev_qubits = prev_gate->get_qubits();
    auto const gate_qubits = gate->get_qubits();
    if (!(
            prev_qubits[0]._qubit == gate_qubits[0]._qubit && prev_qubits[1]._qubit == gate_qubits[1]._qubit) &&
        !(prev_qubits[0]._qubit == gate_qubits[1]._qubit && prev_qubits[1]._qubit == gate_qubits[0]._qubit)) {
        Optimizer::_add_gate_to_circuit(qcir, gate, false);
        return;
    }

    if (prev_gate->get_rotation_category() != gate->get_rotation_category()) {
        Optimizer::_add_gate_to_circuit(qcir, gate, false);
        return;
    }
    if (prev_gate->is_cz() || prev_gate->get_control()._qubit == gate->get_control()._qubit)
        qcir.remove_gate(prev_gate->get_id());
    else
        Optimizer::_add_gate_to_circuit(qcir, gate, false);
}

}  // namespace qsyn::qcir
