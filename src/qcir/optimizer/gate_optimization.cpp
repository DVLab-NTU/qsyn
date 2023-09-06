/****************************************************************************
  PackageName  [ optimizer ]
  Synopsis     [ Define optimization of gates ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>

#include <cassert>

#include "../gate_type.hpp"
#include "../qcir_gate.hpp"
#include "./optimizer.hpp"
#include "util/logger.hpp"

extern dvlab::Logger LOGGER;

void Optimizer::_permute_gates(QCirGate* gate) {
    auto qubits = gate->get_qubits();

    for (size_t i = 0; i < qubits.size(); i++) {
        for (auto& [j, k] : _permutation) {
            if (k == qubits[i]._qubit) {
                qubits[i]._qubit = j;
                break;
            }
        }
    }

    gate->set_qubits(qubits);
}

void Optimizer::_match_hadamards(QCirGate* gate) {
    assert(gate->get_type() == GateType::h);
    size_t qubit = gate->get_targets()._qubit;
    if (_xs.contains(qubit) && !_zs.contains(qubit)) {
        LOGGER.trace("Transform X gate into Z gate");

        _xs.erase(qubit);
        _zs.emplace(qubit);
    } else if (!_xs.contains(qubit) && _zs.contains(qubit)) {
        LOGGER.trace("Transform Z gate into X gate");
        _zs.erase(qubit);
        _xs.emplace(qubit);
    }
    // NOTE - H-S-H to Sdg-H-Sdg
    if (_gates[qubit].size() > 1 && _gates[qubit][_gates[qubit].size() - 2]->get_type() == GateType::h && is_single_z_rotation(_gates[qubit][_gates[qubit].size() - 1])) {
        QCirGate* g2 = _gates[qubit][_gates[qubit].size() - 1];
        if (g2->get_phase().denominator() == 2) {
            _statistics.HS_EXCHANGE++;
            LOGGER.trace("Transform H-S-H into Sdg-H-Sdg");
            QCirGate* zp = new PGate(_gate_count);
            zp->add_qubit(qubit, true);
            _gate_count++;
            zp->set_phase(-1 * g2->get_phase());  // NOTE - S to Sdg
            g2->set_phase(zp->get_phase());       // NOTE - S to Sdg
            _gates[qubit].insert(_gates[qubit].end() - 2, zp);
            return;
        }
    }
    _toggle_element(GateType::h, qubit);
}

void Optimizer::_match_xs(QCirGate* gate) {
    assert(gate->get_type() == GateType::x);
    size_t qubit = gate->get_targets()._qubit;
    if (_xs.contains(qubit)) {
        LOGGER.trace("Cancel X-X into Id");
        _statistics.X_CANCEL++;
    }
    _toggle_element(GateType::x, qubit);
}

void Optimizer::_match_z_rotations(QCirGate* gate) {
    assert(is_single_z_rotation(gate));
    size_t qubit = gate->get_targets()._qubit;
    if (_zs.contains(qubit)) {
        _statistics.FUSE_PHASE++;
        _zs.erase(qubit);
        if (gate->get_type() == GateType::rz || gate->get_type() == GateType::p) {
            gate->set_phase(gate->get_phase() + Phase(1));
        } else if (gate->get_type() == GateType::z) {
            return;
        } else {
            // NOTE - Trans S/S*/T/T* into PGate
            QCirGate* temp = new PGate(_gate_count);
            _gate_count++;
            temp->add_qubit(qubit, true);
            temp->set_phase(gate->get_phase() + Phase(1));
            gate = temp;
        }
    }
    if (gate->get_phase() == Phase(0)) {
        LOGGER.trace("Cancel with previous RZ");
        return;
    }

    if (_xs.contains(qubit)) {
        gate->set_phase(-1 * (gate->get_phase()));
    }
    if (gate->get_phase() == Phase(1) || gate->get_type() == GateType::z) {
        _toggle_element(GateType::z, qubit);
        return;
    }
    // REVIEW - Neglect adjoint due to S and Sdg is separated
    if (_hadamards.contains(qubit)) {
        _add_hadamard(qubit, true);
    }
    QCirGate* available = get_available_z_rotation(qubit);
    if (_availty[qubit] == false && available != nullptr) {
        std::erase(_available[qubit], available);
        std::erase(_gates[qubit], available);
        Phase ph = available->get_phase() + gate->get_phase();
        _statistics.FUSE_PHASE++;
        if (ph == Phase(1)) {
            _toggle_element(GateType::z, qubit);
            return;
        }
        if (ph != Phase(0)) {
            _add_rotation_gate(qubit, ph, GateType::p);
        }
    } else {
        if (_availty[qubit] == true) {
            _availty[qubit] = false;
            _available[qubit].clear();
        }
        _add_rotation_gate(qubit, gate->get_phase(), GateType::p);
    }
}

void Optimizer::_match_czs(QCirGate* gate, bool do_swap, bool do_minimize_czs) {
    assert(gate->get_type() == GateType::cz);
    size_t control_qubit = gate->get_control()._qubit;
    size_t target_qubit = gate->get_targets()._qubit;
    if (control_qubit > target_qubit) {  // NOTE - Symmetric, let ctrl smaller than targ
        size_t tmp = control_qubit;
        gate->set_target_qubit(target_qubit);
        gate->set_control_qubit(tmp);
    }
    // NOTE - Push NOT gates trough the CZ
    // REVIEW - Seems strange
    if (_xs.contains(control_qubit))
        _toggle_element(GateType::z, target_qubit);
    if (_xs.contains(target_qubit))
        _toggle_element(GateType::z, control_qubit);
    if (_hadamards.contains(control_qubit) && _hadamards.contains(target_qubit)) {
        _add_hadamard(control_qubit, true);
        _add_hadamard(target_qubit, true);
    }
    if (!_hadamards.contains(control_qubit) && !_hadamards.contains(target_qubit)) {
        _add_cz(control_qubit, target_qubit, do_minimize_czs);
    } else if (_hadamards.contains(control_qubit)) {
        _statistics.CZ2CX++;
        _add_cx(target_qubit, control_qubit, do_swap);
    } else {
        _statistics.CZ2CX++;
        _add_cx(control_qubit, target_qubit, do_swap);
    }
}

void Optimizer::_match_cxs(QCirGate* gate, bool do_swap, bool do_minimize_czs) {
    assert(gate->get_type() == GateType::cx);
    size_t control_qubit = gate->get_control()._qubit;
    size_t target_qubit = gate->get_targets()._qubit;

    if (_xs.contains(control_qubit))
        _toggle_element(GateType::x, target_qubit);
    if (_zs.contains(target_qubit))
        _toggle_element(GateType::z, control_qubit);
    if (_hadamards.contains(control_qubit) && _hadamards.contains(target_qubit)) {
        _add_cx(target_qubit, control_qubit, do_swap);
    } else if (!_hadamards.contains(control_qubit) && !_hadamards.contains(target_qubit)) {
        _add_cx(control_qubit, target_qubit, do_swap);
    } else if (_hadamards.contains(target_qubit)) {
        _statistics.CX2CZ++;
        if (control_qubit > target_qubit)
            _add_cz(target_qubit, control_qubit, do_minimize_czs);
        else
            _add_cz(control_qubit, target_qubit, do_minimize_czs);
    } else {
        _add_hadamard(control_qubit, true);
        _add_cx(control_qubit, target_qubit, do_swap);
    }
}
