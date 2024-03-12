/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define optimization of gates ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cassert>

#include "../gate_type.hpp"
#include "../qcir_gate.hpp"
#include "./optimizer.hpp"

namespace qsyn::qcir {

void Optimizer::_permute_gates(QCirGate* gate) {
    auto const reverse_map = _permutation | std::views::transform([](auto const& p) { return std::pair(p.second, p.first); }) | tl::to<std::unordered_map>();
    auto const qubits      = gate->get_qubits();
    gate->set_qubits(qubits | std::views::transform([&reverse_map](auto const& operand) { return reverse_map.at(operand); }) | tl::to<QubitIdList>());
}

void Optimizer::_match_hadamards(QCirGate* gate) {
    assert(gate->get_operation() == HGate{});
    auto qubit = gate->get_qubit(0);

    if (_xs.contains(qubit) && !_zs.contains(qubit)) {
        spdlog::trace("Transform X gate into Z gate");

        _xs.erase(qubit);
        _zs.emplace(qubit);
    } else if (!_xs.contains(qubit) && _zs.contains(qubit)) {
        spdlog::trace("Transform Z gate into X gate");
        _zs.erase(qubit);
        _xs.emplace(qubit);
    }
    // NOTE - H-S-H to Sdg-H-Sdg
    if (_gates[qubit].size() > 1 &&
        _gates[qubit][_gates[qubit].size() - 2]->get_operation() == HGate{}) {
        auto g2 = _gates[qubit].back();
        if (g2->get_operation() == SGate{}) {
            _statistics.HS_EXCHANGE++;
            spdlog::trace("Transform H-S-H into Sdg-H-Sdg");
            auto zp = _store_sdg(qubit);
            g2->set_operation(LegacyGateType(std::make_tuple(GateRotationCategory::pz, 1, Phase(-1, 2))));  // NOTE - S to Sdg
            _gates[qubit].insert(_gates[qubit].end() - 2, zp);
            return;
        } else if (g2->get_operation() == SdgGate{}) {
            _statistics.HS_EXCHANGE++;
            spdlog::trace("Transform H-S-H into Sdg-H-Sdg");
            auto zp = _store_s(qubit);
            g2->set_operation(LegacyGateType(std::make_tuple(GateRotationCategory::pz, 1, Phase(1, 2))));  // NOTE - Sdg to S
            _gates[qubit].insert(_gates[qubit].end() - 2, zp);
            return;
        }
    }
    _toggle_element(ElementType::h, qubit);
}

void Optimizer::_match_xs(QCirGate* gate) {
    assert(gate->get_operation() == XGate{});
    auto const qubit = gate->get_qubit(0);
    if (_xs.contains(qubit)) {
        spdlog::trace("Cancel X-X into Id");
        _statistics.X_CANCEL++;
    }
    _toggle_element(ElementType::x, qubit);
}

void Optimizer::_match_z_rotations(QCirGate* gate) {
    assert(is_single_z_rotation(gate));
    auto const qubit = gate->get_qubit(0);
    if (_zs.contains(qubit)) {
        _statistics.FUSE_PHASE++;
        _zs.erase(qubit);
        gate->set_operation(LegacyGateType{std::make_tuple(gate->get_rotation_category(), gate->get_num_qubits(), gate->get_phase() + dvlab::Phase(1))});
    }
    if (gate->get_phase() == dvlab::Phase(0)) {
        spdlog::trace("Cancel with previous RZ");
        return;
    }

    if (_xs.contains(qubit)) {
        gate->set_operation(LegacyGateType{std::make_tuple(GateRotationCategory::px, 1, -1 * gate->get_phase())});
    }
    if (gate->get_phase() == dvlab::Phase(1)) {
        _toggle_element(ElementType::z, qubit);
        return;
    }
    // REVIEW - Neglect adjoint due to S and Sdg is separated
    if (_hadamards.contains(qubit)) {
        _add_hadamard(qubit, true);
    }
    QCirGate* available = get_available_z_rotation(qubit);
    if (!_availty[qubit] && available) {
        std::erase(_available[qubit], available);
        std::erase(_gates[qubit], available);
        auto const phase = available->get_phase() + gate->get_phase();
        _statistics.FUSE_PHASE++;
        if (phase == dvlab::Phase(1)) {
            _toggle_element(ElementType::z, qubit);
            return;
        }
        if (phase != dvlab::Phase(0)) {
            _add_rotation_gate(qubit, phase, GateRotationCategory::pz);
        }
    } else {
        if (_availty[qubit]) {
            _availty[qubit] = false;
            _available[qubit].clear();
        }
        _add_rotation_gate(qubit, gate->get_phase(), GateRotationCategory::pz);
    }
}

void Optimizer::_match_czs(QCirGate* gate, bool do_swap, bool do_minimize_czs) {
    assert(gate->get_operation() == CZGate{});
    auto control_qubit = gate->get_qubit(0);
    auto target_qubit  = gate->get_qubit(1);
    if (control_qubit > target_qubit) {  // NOTE - Symmetric, let ctrl smaller than targ
        gate->set_qubits({target_qubit, control_qubit});
    }
    // NOTE - Push NOT gates trough the CZ
    // REVIEW - Seems strange
    if (_xs.contains(control_qubit))
        _toggle_element(ElementType::z, target_qubit);
    if (_xs.contains(target_qubit))
        _toggle_element(ElementType::z, control_qubit);
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
    assert(gate->get_operation() == CXGate{});
    auto control_qubit = gate->get_qubit(0);
    auto target_qubit  = gate->get_qubit(1);

    if (_xs.contains(control_qubit))
        _toggle_element(ElementType::x, target_qubit);
    if (_zs.contains(target_qubit))
        _toggle_element(ElementType::z, control_qubit);
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

}  // namespace qsyn::qcir
