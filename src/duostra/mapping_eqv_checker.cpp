/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class MappingEquivalenceChecker member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./mapping_eqv_checker.hpp"

#include <tl/enumerate.hpp>

#include "./placer.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "qcir/qcir_qubit.hpp"
#include "qsyn/qsyn_type.hpp"

using namespace qsyn::qcir;

namespace qsyn::duostra {

/**
 * @brief Construct a new Ext Checker:: Ext Checker object
 *
 * @param phy
 * @param log
 * @param dev
 * @param init
 * @param reverse check reversily if true
 */
MappingEquivalenceChecker::MappingEquivalenceChecker(QCir* phy, QCir* log, Device dev, PlacerType placer_type, std::vector<QubitIdType> init, bool reverse) : _physical(phy), _logical(log), _device(std::move(dev)), _reverse(reverse) {
    if (init.empty()) {
        auto placer = get_placer(placer_type);
        init        = placer->place_and_assign(_device);
    } else
        _device.place(init);
    for (auto const& [i, qubit] : tl::views::enumerate(_logical->get_qubits())) {
        _dependency[i] = _reverse ? qubit.get_last_gate() : qubit.get_first_gate();
    }
}

/**
 * @brief Check physical circuit
 *
 * @return true
 * @return false
 */
bool MappingEquivalenceChecker::check() {
    auto execute_order = _physical->get_gates();
    if (_reverse) std::ranges::reverse(execute_order);  // NOTE - Now the order starts from back
    // NOTE - Traverse all physical gates, should match dependency of logical gate
    std::unordered_set<QCirGate*> swaps;
    for (auto const& phys_gate : execute_order) {
        if (swaps.contains(phys_gate)) {
            continue;
        }
        if (phys_gate->get_num_qubits() == 2) {
            if (is_swap(phys_gate)) {
                if (!execute_swap(phys_gate, swaps)) return false;
            } else {
                if (!execute_double(phys_gate)) return false;
            }
        } else if (phys_gate->get_num_qubits() > 1) {
            return false;
        } else if (!execute_single(phys_gate))
            return false;
    }
    // REVIEW - check remaining gates in logical are swaps
    check_remaining();
    // Should end with all qubits in _dependency = nullptr;
    return true;
}

/**
 * @brief Check the gate and its previous two gates constitute a swap
 *
 * @param candidate
 * @return true
 * @return false
 */
bool MappingEquivalenceChecker::is_swap(QCirGate* candidate) {
    if (candidate->get_operation() != CXGate()) return false;
    QCirGate* q0_gate = get_next(*_physical, candidate->get_id(), 0);
    QCirGate* q1_gate = get_next(*_physical, candidate->get_id(), 1);

    if (q0_gate != q1_gate || q0_gate == nullptr || q1_gate == nullptr) return false;
    if (q0_gate->get_operation() != CXGate()) return false;
    // q1gate == q0 gate
    if (candidate->get_qubit(0) != q1_gate->get_qubit(1) ||
        candidate->get_qubit(1) != q0_gate->get_qubit(0)) return false;

    candidate = q0_gate;
    q0_gate   = get_next(*_physical, candidate->get_id(), 0);
    q1_gate   = get_next(*_physical, candidate->get_id(), 1);

    if (q0_gate != q1_gate || q0_gate == nullptr || q1_gate == nullptr) return false;
    if (q0_gate->get_operation() != CXGate()) return false;
    // q1 gate == q0 gate
    if (candidate->get_qubit(0) != q1_gate->get_qubit(1) ||
        candidate->get_qubit(1) != q0_gate->get_qubit(0)) return false;

    // NOTE - If it is actually a gate in dependency, it can not be changed into swap
    auto logical_gate_ctrl_id = _device.get_physical_qubit(candidate->get_qubit(0)).get_logical_qubit();
    auto logical_gate_targ_id = _device.get_physical_qubit(candidate->get_qubit(1)).get_logical_qubit();

    assert(logical_gate_ctrl_id.has_value());
    assert(logical_gate_targ_id.has_value());

    QCirGate* log_gate0 = _dependency[logical_gate_ctrl_id.value()];
    QCirGate* log_gate1 = _dependency[logical_gate_targ_id.value()];

    return log_gate0 != log_gate1 || log_gate0 == nullptr || log_gate0->get_operation() != CXGate();
}

/**
 * @brief Execute swap gate
 *
 * @param first first gate of swap
 * @param swaps container to mark
 * @return true
 * @return false
 */
bool MappingEquivalenceChecker::execute_swap(QCirGate* first, std::unordered_set<QCirGate*>& swaps) {
    if (!_device.get_physical_qubit(first->get_qubit(0)).is_adjacency(_device.get_physical_qubit(first->get_qubit(1)))) return false;

    swaps.emplace(first);
    auto next_gate = get_next(*_physical, first->get_id(), 0);
    swaps.emplace(next_gate);
    swaps.emplace(get_next(*_physical, next_gate->get_id(), 0));
    auto& q0        = _device.get_physical_qubit(first->get_qubit(0));
    auto& q1        = _device.get_physical_qubit(first->get_qubit(1));
    auto const temp = q0.get_logical_qubit();
    q0.set_logical_qubit(q1.get_logical_qubit());
    q1.set_logical_qubit(temp);
    return true;
}

/**
 * @brief Execute single-qubit gate
 *
 * @param gate
 * @return true
 * @return false
 */
bool MappingEquivalenceChecker::execute_single(QCirGate* gate) {
    auto const& logical_qubit = _device.get_physical_qubit(gate->get_qubit(0)).get_logical_qubit();

    assert(logical_qubit.has_value());

    QCirGate* logical = _dependency[logical_qubit.value()];
    if (logical == nullptr) {
        spdlog::error("Corresponding logical gate of gate {} is nullptr!!", gate->get_id());
        return false;
    }

    if (logical->get_operation() != gate->get_operation()) {
        spdlog::error("Type of gate {} mismatches!!", gate->get_id());
        return false;
    }

    if (logical->get_qubit(0) != logical_qubit.value()) {
        spdlog::error("Target qubit of gate {} mismatches!!", gate->get_id());
        return false;
    }

    _dependency[logical->get_qubit(0)] = get_next(*_logical, logical->get_id(), 0);
    return true;
}

/**
 * @brief Execute double-qubit gate
 *
 * @param gate
 * @return true
 * @return false
 */
bool MappingEquivalenceChecker::execute_double(QCirGate* gate) {
    auto logical_ctrl_id = _device.get_physical_qubit(gate->get_qubit(0)).get_logical_qubit();
    auto logical_targ_id = _device.get_physical_qubit(gate->get_qubit(1)).get_logical_qubit();

    assert(logical_ctrl_id.has_value());
    assert(logical_targ_id.has_value());

    if (_dependency[logical_ctrl_id.value()] != _dependency[logical_targ_id.value()]) {
        spdlog::error("Gate {} violates dependency graph!!", gate->get_id());
        return false;
    }
    QCirGate* logical_gate = _dependency[logical_targ_id.value()];
    if (logical_gate == nullptr) {
        spdlog::error("Corresponding logical gate of gate {} is nullptr!!", gate->get_id());
        return false;
    }

    if (logical_gate->get_operation() != gate->get_operation()) {
        spdlog::error("Type of gate {} mismatches!!", gate->get_id());
        return false;
    }
    if (logical_gate->get_qubit(0) != logical_ctrl_id.value()) {
        spdlog::error("Control qubit of gate {} mismatches!!", gate->get_id());
        return false;
    }
    if (logical_gate->get_qubit(1) != logical_targ_id.value()) {
        spdlog::error("Target qubit of gate {} mismatches!!", gate->get_id());
        return false;
    }

    if (!_device.get_physical_qubit(gate->get_qubit(0)).is_adjacency(_device.get_physical_qubit(gate->get_qubit(1)))) return false;

    _dependency[logical_gate->get_qubit(0)] = get_next(*_logical, logical_gate->get_id(), 0);
    _dependency[logical_gate->get_qubit(1)] = get_next(*_logical, logical_gate->get_id(), 1);

    return true;
}

/**
 * @brief Check gates in remaining logical circuit are all swaps
 *
 * @return true
 * @return false
 */
void MappingEquivalenceChecker::check_remaining() {
    for (auto const& [q, g] : _dependency) {
        if (g != nullptr) {
            spdlog::warn("Note: qubit {} has gates remaining", q);
        }
    }
}

/**
 * @brief Get next gate
 *
 * @param info
 * @return QCirGate*
 */
QCirGate* MappingEquivalenceChecker::get_next(qcir::QCir const& qcir, size_t gate_id, size_t pin) const {
    return qcir.get_gate(_reverse ? qcir.get_predecessor(gate_id, pin) : qcir.get_successor(gate_id, pin));
}

}  // namespace qsyn::duostra
