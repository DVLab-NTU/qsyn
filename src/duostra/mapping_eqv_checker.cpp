/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class MappingEquivalenceChecker member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./mapping_eqv_checker.hpp"

#include "./placer.hpp"
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
MappingEquivalenceChecker::MappingEquivalenceChecker(QCir* phy, QCir* log, Device dev, std::vector<QubitIdType> init, bool reverse) : _physical(phy), _logical(log), _device(std::move(dev)), _reverse(reverse) {
    if (init.empty()) {
        auto placer = get_placer();
        init        = placer->place_and_assign(_device);
    } else
        _device.place(init);
    _physical->update_topological_order();
    for (auto const& qubit : _logical->get_qubits()) {
        _dependency[qubit->get_id()] = _reverse ? qubit->get_last() : qubit->get_first();
    }
}

/**
 * @brief Check physical circuit
 *
 * @return true
 * @return false
 */
bool MappingEquivalenceChecker::check() {
    std::vector<QCirGate*> execute_order = _physical->get_topologically_ordered_gates();
    if (_reverse) reverse(execute_order.begin(), execute_order.end());  // NOTE - Now the order starts from back
    // NOTE - Traverse all physical gates, should match dependency of logical gate
    std::unordered_set<QCirGate*> swaps;
    for (auto const& phys_gate : execute_order) {
        if (swaps.contains(phys_gate)) {
            continue;
        }
        if (phys_gate->is_cx() || phys_gate->is_cz()) {
            if (is_swap(phys_gate)) {
                bool correct = execute_swap(phys_gate, swaps);
                if (!correct) return false;
            } else {
                bool correct = execute_double(phys_gate);
                if (!correct) return false;
            }
        } else if (phys_gate->get_num_qubits() > 1) {
            return false;
        } else {
            bool correct = execute_single(phys_gate);
            if (!correct) return false;
        }
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
    if (!candidate->is_cx()) return false;
    QCirGate* q0_gate = get_next(candidate->get_qubits()[0]);
    QCirGate* q1_gate = get_next(candidate->get_qubits()[1]);

    if (q0_gate != q1_gate || q0_gate == nullptr || q1_gate == nullptr) return false;
    if (!q0_gate->is_cx()) return false;
    // q1gate == q0 gate
    if (candidate->get_qubits()[0]._qubit != q1_gate->get_qubits()[1]._qubit ||
        candidate->get_qubits()[1]._qubit != q0_gate->get_qubits()[0]._qubit) return false;

    candidate = q0_gate;
    q0_gate   = get_next(candidate->get_qubits()[0]);
    q1_gate   = get_next(candidate->get_qubits()[1]);

    if (q0_gate != q1_gate || q0_gate == nullptr || q1_gate == nullptr) return false;
    if (!q0_gate->is_cx()) return false;
    // q1gate == q0 gate
    if (candidate->get_qubits()[0]._qubit != q1_gate->get_qubits()[1]._qubit ||
        candidate->get_qubits()[1]._qubit != q0_gate->get_qubits()[0]._qubit) return false;

    // NOTE - If it is actually a gate in dependency, it can not be changed into swap
    auto logical_gate_ctrl_id = _device.get_physical_qubit(candidate->get_qubits()[0]._qubit).get_logical_qubit();
    auto logical_gate_targ_id = _device.get_physical_qubit(candidate->get_qubits()[1]._qubit).get_logical_qubit();

    assert(logical_gate_ctrl_id.has_value());
    assert(logical_gate_targ_id.has_value());

    QCirGate* log_gate0 = _dependency[logical_gate_ctrl_id.value()];
    QCirGate* log_gate1 = _dependency[logical_gate_targ_id.value()];
    if (log_gate0 != log_gate1 || log_gate0 == nullptr)
        return true;
    else if (!log_gate0->is_cx())
        return true;
    else
        return false;
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
    bool connect = _device.get_physical_qubit(first->get_qubits()[0]._qubit).is_adjacency(_device.get_physical_qubit(first->get_qubits()[1]._qubit));
    if (!connect) return false;

    swaps.emplace(first);
    QCirGate* next = get_next(first->get_qubits()[0]);
    swaps.emplace(next);
    next = get_next(next->get_qubits()[0]);
    swaps.emplace(next);
    _device.apply_swap_check(first->get_qubits()[0]._qubit, first->get_qubits()[1]._qubit);
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
    assert(gate->get_qubits()[0]._isTarget == true);
    auto const& logical_qubit = _device.get_physical_qubit(gate->get_qubits()[0]._qubit).get_logical_qubit();

    assert(logical_qubit.has_value());

    QCirGate* logical = _dependency[logical_qubit.value()];
    if (logical == nullptr) {
        std::cout << "Error: corresponding logical gate of gate " << gate->get_id() << " is nullptr!!" << std::endl;
        return false;
    }

    if (logical->get_type() != gate->get_type()) {
        std::cout << logical->get_type_str() << "(" << logical->get_id() << ") " << gate->get_type_str() << std::endl;
        std::cout << "Error: type of gate " << gate->get_id() << " mismatches!!" << std::endl;
        return false;
    }
    if (logical->get_phase() != gate->get_phase()) {
        std::cout << "Error: phase of gate " << gate->get_id() << " mismatches!!" << std::endl;
        return false;
    }
    if (logical->get_qubits()[0]._qubit != logical_qubit.value()) {
        std::cout << "Error: target of gate " << gate->get_id() << " mismatches!!" << std::endl;
        return false;
    }

    _dependency[logical->get_qubits()[0]._qubit] = get_next(logical->get_qubits()[0]);
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
    assert(gate->get_qubits()[0]._isTarget == false);
    assert(gate->get_qubits()[1]._isTarget == true);
    auto logical_ctrl_id = _device.get_physical_qubit(gate->get_qubits()[0]._qubit).get_logical_qubit();
    auto logical_targ_id = _device.get_physical_qubit(gate->get_qubits()[1]._qubit).get_logical_qubit();

    assert(logical_ctrl_id.has_value());
    assert(logical_targ_id.has_value());

    if (_dependency[logical_ctrl_id.value()] != _dependency[logical_targ_id.value()]) {
        std::cout << "Error: gate " << gate->get_id() << " violates dependency graph!!" << std::endl;
        return false;
    }
    QCirGate* logical_gate = _dependency[logical_targ_id.value()];
    if (logical_gate == nullptr) {
        std::cout << "Error: corresponding logical gate of gate " << gate->get_id() << " is nullptr!!" << std::endl;
        return false;
    }

    if (logical_gate->get_type() != gate->get_type()) {
        std::cout << "Error: type of gate " << gate->get_id() << " mismatches!!" << std::endl;
        return false;
    }
    if (logical_gate->get_phase() != gate->get_phase()) {
        std::cout << "Error: phase of gate " << gate->get_id() << " mismatches!!" << std::endl;
        return false;
    }
    if (logical_gate->get_qubits()[0]._qubit != logical_ctrl_id.value()) {
        std::cout << "Error: control of gate " << gate->get_id() << " mismatches!!" << std::endl;
        return false;
    }
    if (logical_gate->get_qubits()[1]._qubit != logical_targ_id.value()) {
        std::cout << "Error: target of gate " << gate->get_id() << " mismatches!!" << std::endl;
        return false;
    }

    bool connect = _device.get_physical_qubit(gate->get_qubits()[0]._qubit).is_adjacency(_device.get_physical_qubit(gate->get_qubits()[1]._qubit));
    if (!connect) return false;

    _dependency[logical_gate->get_qubits()[0]._qubit] = get_next(logical_gate->get_qubits()[0]);
    _dependency[logical_gate->get_qubits()[1]._qubit] = get_next(logical_gate->get_qubits()[1]);

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
            std::cout << "Note: qubit " << q << " has gates remaining" << std::endl;
        }
    }
}

/**
 * @brief Get next gate
 *
 * @param info
 * @return QCirGate*
 */
QCirGate* MappingEquivalenceChecker::get_next(QubitInfo const& info) {
    if (_reverse)
        return info._prev;
    else
        return info._next;
}

}  // namespace qsyn::duostra
