/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class MappingEQChecker member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./mapping_eqv_checker.hpp"

#include "./placer.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "qcir/qcir_qubit.hpp"

using namespace std;

/**
 * @brief Construct a new Ext Checker:: Ext Checker object
 *
 * @param phy
 * @param log
 * @param dev
 * @param init
 * @param reverse check reversily if true
 */
MappingEQChecker::MappingEQChecker(QCir* phy, QCir* log, Device dev, std::vector<size_t> init, bool reverse) : _physical(phy), _logical(log), _device(dev), _reverse(reverse) {
    if (init.empty()) {
        auto placer = get_placer();
        init = placer->place_and_assign(_device);
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
bool MappingEQChecker::check() {
    vector<QCirGate*> execute_order = _physical->get_topologically_ordered_gates();
    if (_reverse) reverse(execute_order.begin(), execute_order.end());  // NOTE - Now the order starts from back
    // NOTE - Traverse all physical gates, should match dependency of logical gate
    unordered_set<QCirGate*> swaps;
    for (auto const& phy_gate : execute_order) {
        if (swaps.contains(phy_gate)) {
            continue;
        }
        GateType gate_type = phy_gate->get_type();
        if (gate_type == GateType::cx || gate_type == GateType::cz) {
            if (is_swap(phy_gate)) {
                bool correct = execute_swap(phy_gate, swaps);
                if (!correct) return false;
            } else {
                bool correct = execute_double(phy_gate);
                if (!correct) return false;
            }
        } else if (phy_gate->get_num_qubits() > 1) {
            return false;
        } else {
            bool correct = execute_single(phy_gate);
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
bool MappingEQChecker::is_swap(QCirGate* candidate) {
    if (candidate->get_type() != GateType::cx) return false;
    QCirGate* q0_gate = get_next(candidate->get_qubits()[0]);
    QCirGate* q1_gate = get_next(candidate->get_qubits()[1]);

    if (q0_gate != q1_gate || q0_gate == nullptr || q1_gate == nullptr) return false;
    if (q0_gate->get_type() != GateType::cx) return false;
    // q1gate == q0 gate
    if (candidate->get_qubits()[0]._qubit != q1_gate->get_qubits()[1]._qubit ||
        candidate->get_qubits()[1]._qubit != q0_gate->get_qubits()[0]._qubit) return false;

    candidate = q0_gate;
    q0_gate = get_next(candidate->get_qubits()[0]);
    q1_gate = get_next(candidate->get_qubits()[1]);

    if (q0_gate != q1_gate || q0_gate == nullptr || q1_gate == nullptr) return false;
    if (q0_gate->get_type() != GateType::cx) return false;
    // q1gate == q0 gate
    if (candidate->get_qubits()[0]._qubit != q1_gate->get_qubits()[1]._qubit ||
        candidate->get_qubits()[1]._qubit != q0_gate->get_qubits()[0]._qubit) return false;

    // NOTE - If it is actually a gate in dependency, it can not be changed into swap
    size_t phy_ctrl_id = candidate->get_qubits()[0]._qubit;
    size_t phy_targ_id = candidate->get_qubits()[1]._qubit;

    QCirGate* log_gate0 = _dependency[_device.get_physical_qubit(phy_ctrl_id).get_logical_qubit()];
    QCirGate* log_gate1 = _dependency[_device.get_physical_qubit(phy_targ_id).get_logical_qubit()];
    if (log_gate0 != log_gate1 || log_gate0 == nullptr)
        return true;
    else if (log_gate0->get_type() != GateType::cx)
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
bool MappingEQChecker::execute_swap(QCirGate* first, unordered_set<QCirGate*>& swaps) {
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
bool MappingEQChecker::execute_single(QCirGate* gate) {
    assert(gate->get_qubits()[0]._isTarget == true);
    size_t phy_id = gate->get_qubits()[0]._qubit;

    QCirGate* logical = _dependency[_device.get_physical_qubit(phy_id).get_logical_qubit()];
    if (logical == nullptr) {
        cout << "Error: corresponding logical gate of gate " << gate->get_id() << " is nullptr!!" << endl;
        return false;
    }

    if (logical->get_type() != gate->get_type()) {
        cout << logical->get_type_str() << "(" << logical->get_id() << ") " << gate->get_type_str() << endl;
        cout << "Error: type of gate " << gate->get_id() << " mismatches!!" << endl;
        return false;
    }
    if (logical->get_phase() != gate->get_phase()) {
        cout << "Error: phase of gate " << gate->get_id() << " mismatches!!" << endl;
        return false;
    }
    if (logical->get_qubits()[0]._qubit != _device.get_physical_qubit(phy_id).get_logical_qubit()) {
        cout << "Error: target of gate " << gate->get_id() << " mismatches!!" << endl;
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
bool MappingEQChecker::execute_double(QCirGate* gate) {
    assert(gate->get_qubits()[0]._isTarget == false);
    assert(gate->get_qubits()[1]._isTarget == true);
    size_t phy_ctrl_id = gate->get_qubits()[0]._qubit;
    size_t phy_targ_id = gate->get_qubits()[1]._qubit;

    if (_dependency[_device.get_physical_qubit(phy_ctrl_id).get_logical_qubit()] != _dependency[_device.get_physical_qubit(phy_targ_id).get_logical_qubit()]) {
        cout << "Error: gate " << gate->get_id() << " violates dependency graph!!" << endl;
        return false;
    }
    QCirGate* logical = _dependency[_device.get_physical_qubit(phy_targ_id).get_logical_qubit()];
    if (logical == nullptr) {
        cout << "Error: corresponding logical gate of gate " << gate->get_id() << " is nullptr!!" << endl;
        return false;
    }

    if (logical->get_type() != gate->get_type()) {
        cout << "Error: type of gate " << gate->get_id() << " mismatches!!" << endl;
        return false;
    }
    if (logical->get_phase() != gate->get_phase()) {
        cout << "Error: phase of gate " << gate->get_id() << " mismatches!!" << endl;
        return false;
    }
    if (logical->get_qubits()[0]._qubit != _device.get_physical_qubit(phy_ctrl_id).get_logical_qubit()) {
        cout << "Error: control of gate " << gate->get_id() << " mismatches!!" << endl;
        return false;
    }
    if (logical->get_qubits()[1]._qubit != _device.get_physical_qubit(phy_targ_id).get_logical_qubit()) {
        cout << "Error: target of gate " << gate->get_id() << " mismatches!!" << endl;
        return false;
    }

    bool connect = _device.get_physical_qubit(phy_ctrl_id).is_adjacency(_device.get_physical_qubit(phy_targ_id));
    if (!connect) return false;

    _dependency[logical->get_qubits()[0]._qubit] = get_next(logical->get_qubits()[0]);
    _dependency[logical->get_qubits()[1]._qubit] = get_next(logical->get_qubits()[1]);

    return true;
}

/**
 * @brief Check gates in remaining logical circuit are all swaps
 *
 * @return true
 * @return false
 */
bool MappingEQChecker::check_remaining() {
    for (auto const& [q, g] : _dependency) {
        if (g != nullptr) {
            cout << "Note: qubit " << q << " has gates remaining" << endl;
        }
    }
    return true;
}

/**
 * @brief Get next gate
 *
 * @param info
 * @return QCirGate*
 */
QCirGate* MappingEQChecker::get_next(QubitInfo const& info) {
    if (_reverse)
        return info._parent;
    else
        return info._child;
}