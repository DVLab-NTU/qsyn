/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define optimization of gates ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cassert>
#include <tl/enumerate.hpp>
#include <unordered_map>

#include "../basic_gate_type.hpp"
#include "../qcir_gate.hpp"
#include "./optimizer.hpp"

namespace qsyn::qcir {

void Optimizer::_permute_gates(QCirGate& gate) {
    std::unordered_map<QubitIdType, QubitIdType> reverse_table;
    for (auto const& [a, b] : tl::views::enumerate(_permutation)) {
        reverse_table[b] = a;
    }
    auto const qubits = gate.get_qubits();
    gate.set_qubits(qubits | std::views::transform([&reverse_table](auto const& operand) { return reverse_table.at(operand); }) | tl::to<QubitIdList>());
}

void Optimizer::_match_hadamards(QCirGate const& gate) {
    assert(gate.get_operation() == HGate());
    auto qubit = gate.get_qubit(0);

    if (_xs[qubit] && !_zs[qubit]) {
        spdlog::trace("Transform X gate into Z gate");
        _xs[qubit] = false;
        _zs[qubit] = true;
    } else if (!_xs[qubit] && _zs[qubit]) {
        spdlog::trace("Transform Z gate into X gate");
        _zs[qubit] = false;
        _xs[qubit] = true;
    }
    // NOTE - H-S-H to Sdg-H-Sdg
    if (_gates[qubit].size() > 1 &&
        _storage[_gates[qubit][_gates[qubit].size() - 2]].get_operation() == HGate()) {
        auto g2 = _gates[qubit].back();
        if (_storage[g2].get_operation() == SGate()) {
            _statistics.HS_EXCHANGE++;
            spdlog::trace("Transform H-S-H into Sdg-H-Sdg");
            auto zp = _store_sdg(qubit);
            _storage[g2].set_operation(SdgGate());  // NOTE - S to Sdg
            _gates[qubit].insert(_gates[qubit].end() - 2, zp);
            return;
        } else if (_storage[g2].get_operation() == SdgGate()) {
            _statistics.HS_EXCHANGE++;
            spdlog::trace("Transform H-S-H into Sdg-H-Sdg");
            auto zp = _store_s(qubit);
            _storage[g2].set_operation(SGate());  // NOTE - Sdg to S
            _gates[qubit].insert(_gates[qubit].end() - 2, zp);
            return;
        }
    }
    _toggle_element(ElementType::h, qubit);
}

void Optimizer::_match_xs(QCirGate const& gate) {
    assert(gate.get_operation() == XGate());
    auto const qubit = gate.get_qubit(0);
    if (_xs[qubit]) {
        spdlog::trace("Cancel X-X into Id");
        _statistics.X_CANCEL++;
    }
    _toggle_element(ElementType::x, qubit);
}

void Optimizer::_match_z_rotations(QCirGate& gate) {
    assert(is_single_z_rotation(gate));
    auto const qubit = gate.get_qubit(0);
    if (_zs[qubit]) {
        _statistics.FUSE_PHASE++;
        _zs[qubit] = false;
        auto op    = gate.get_operation().get_underlying<PZGate>();
        op.set_phase(op.get_phase() + dvlab::Phase(1));
        gate.set_operation(op);
    }
    if (gate.get_operation() == IdGate()) {
        spdlog::trace("Cancel with previous RZ");
        return;
    }

    if (_xs[qubit]) {
        // since we know that the gate is a single-qubit z-rotation, commuting it with an x gate is equivalent to taking the adjoint of the z-rotation
        gate.set_operation(adjoint(gate.get_operation()));
    }
    if (gate.get_operation() == ZGate()) {
        _toggle_element(ElementType::z, qubit);
        return;
    }
    // REVIEW - Neglect adjoint due to S and Sdg is separated
    if (_hadamards[qubit]) {
        _add_hadamard(qubit, true);
    }
    auto const gate_op = gate.get_operation().get_underlying<PZGate>();
    auto available     = get_available_z_rotation(qubit);
    if (!_qubit_available[qubit] && available.has_value()) {
        std::erase(_available_gates[qubit], available);
        std::erase(_gates[qubit], available);
        auto const available_op = _storage[*available].get_operation().get_underlying<PZGate>();
        auto const phase        = available_op.get_phase() + gate_op.get_phase();
        _statistics.FUSE_PHASE++;
        if (phase == dvlab::Phase(1)) {
            _toggle_element(ElementType::z, qubit);
            return;
        }
        if (phase != dvlab::Phase(0)) {
            _add_single_z_rotation_gate(qubit, phase);
        }
    } else {
        if (_qubit_available[qubit]) {
            _qubit_available[qubit] = false;
            _available_gates[qubit].clear();
        }
        _add_single_z_rotation_gate(qubit, gate_op.get_phase());
    }
}

void Optimizer::_match_czs(QCirGate& gate, bool do_swap, bool do_minimize_czs) {
    assert(gate.get_operation() == CZGate());
    auto control_qubit = gate.get_qubit(0);
    auto target_qubit  = gate.get_qubit(1);
    if (control_qubit > target_qubit) {  // NOTE - Symmetric, let ctrl smaller than targ
        gate.set_qubits({target_qubit, control_qubit});
    }
    // NOTE - Push NOT gates trough the CZ
    // REVIEW - Seems strange
    if (_xs[control_qubit])
        _toggle_element(ElementType::z, target_qubit);
    if (_xs[target_qubit])
        _toggle_element(ElementType::z, control_qubit);
    if (_hadamards[control_qubit] && _hadamards[target_qubit]) {
        _add_hadamard(control_qubit, true);
        _add_hadamard(target_qubit, true);
    }
    if (!_hadamards[control_qubit] && !_hadamards[target_qubit]) {
        _add_cz(control_qubit, target_qubit, do_minimize_czs);
    } else if (_hadamards[control_qubit]) {
        _statistics.CZ2CX++;
        _add_cx(target_qubit, control_qubit, do_swap);
    } else {
        _statistics.CZ2CX++;
        _add_cx(control_qubit, target_qubit, do_swap);
    }
}

void Optimizer::_match_cxs(QCirGate const& gate, bool do_swap, bool do_minimize_czs) {
    assert(gate.get_operation() == CXGate());
    auto control_qubit = gate.get_qubit(0);
    auto target_qubit  = gate.get_qubit(1);

    if (_xs[control_qubit])
        _toggle_element(ElementType::x, target_qubit);
    if (_zs[target_qubit])
        _toggle_element(ElementType::z, control_qubit);
    if (_hadamards[control_qubit] && _hadamards[target_qubit]) {
        _add_cx(target_qubit, control_qubit, do_swap);
    } else if (!_hadamards[control_qubit] && !_hadamards[target_qubit]) {
        _add_cx(control_qubit, target_qubit, do_swap);
    } else if (_hadamards[target_qubit]) {
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

}  // namespace qsyn::qcir
