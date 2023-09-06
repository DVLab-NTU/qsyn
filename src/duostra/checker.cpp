/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Checker member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./checker.hpp"

#include "./circuit_topology.hpp"
#include "util/logger.hpp"

using namespace std;
extern size_t VERBOSE;
extern dvlab::Logger LOGGER;

/**
 * @brief Construct a new Checker:: Checker object
 *
 * @param topo
 * @param device
 * @param ops
 * @param assign
 * @param tqdm
 */
Checker::Checker(CircuitTopology& topo,
                 Device& device,
                 std::vector<Operation> const& ops,
                 vector<size_t> const& assign, bool tqdm)
    : _topo(&topo), _device(&device), _ops(ops), _tqdm(tqdm) {
    _device->place(assign);
}

/**
 * @brief Get Cycle
 *
 * @param type
 * @return size_t
 */
size_t Checker::get_cycle(GateType type) {
    switch (type) {
        case GateType::swap:
            return SWAP_DELAY;
        case GateType::cx:
            return DOUBLE_DELAY;
        case GateType::cz:
            return DOUBLE_DELAY;
        default:
            return SINGLE_DELAY;
    }
}

/**
 * @brief Apply Gate
 *
 * @param op
 * @param q0
 */
void Checker::apply_gate(Operation const& op, PhysicalQubit& q0) {
    size_t start = get<0>(op.get_duration());
    size_t end = get<1>(op.get_duration());

    if (!(start >= q0.get_occupied_time())) {
        cerr << op << "\n"
             << "Q" << q0.get_id() << " occu: " << q0.get_occupied_time()
             << endl;
        abort();
    }
    if (!(end == start + get_cycle(op.get_type()))) {
        cerr << op << endl;
        abort();
    }
    q0.set_occupied_time(end);
}

/**
 * @brief Apply Gate
 *
 * @param op
 * @param q0
 * @param q1
 */
void Checker::apply_gate(Operation const& op,
                         PhysicalQubit& q0,
                         PhysicalQubit& q1) {
    size_t start = get<0>(op.get_duration());
    size_t end = get<1>(op.get_duration());

    if (!(start >= q0.get_occupied_time() && start >= q1.get_occupied_time())) {
        cerr << op << "\n"
             << "Q" << q0.get_id() << " occu: " << q0.get_occupied_time()
             << "\n"
             << "Q" << q1.get_id() << " occu: " << q1.get_occupied_time()
             << endl;
        abort();
    }
    if (!(end == start + get_cycle(op.get_type()))) {
        cerr << op << endl;
        abort();
    }
    q0.set_occupied_time(end);
    q1.set_occupied_time(end);
}

/**
 * @brief Apply Swap
 *
 * @param op
 */
void Checker::apply_swap(Operation const& op) {
    if (op.get_type() != GateType::swap) {
        cerr << GATE_TYPE_TO_STR[op.get_type()] << " in applySwap"
             << endl;
        abort();
    }
    size_t q0_idx = get<0>(op.get_qubits());
    size_t q1_idx = get<1>(op.get_qubits());
    auto& q0 = _device->get_physical_qubit(q0_idx);
    auto& q1 = _device->get_physical_qubit(q1_idx);
    apply_gate(op, q0, q1);

    // swap
    size_t temp = q0.get_logical_qubit();
    q0.set_logical_qubit(q1.get_logical_qubit());
    q1.set_logical_qubit(temp);
}

/**
 * @brief Apply CX
 *
 * @param op
 * @param gate
 * @return true
 * @return false
 */
bool Checker::apply_cx(Operation const& op, Gate const& gate) {
    if (!(op.get_type() == GateType::cx || op.get_type() == GateType::cz)) {
        cerr << GATE_TYPE_TO_STR[op.get_type()] << " in applyCX" << endl;
        abort();
    }
    size_t q0_idx = get<0>(op.get_qubits());
    size_t q1_idx = get<1>(op.get_qubits());
    auto& q0 = _device->get_physical_qubit(q0_idx);
    auto& q1 = _device->get_physical_qubit(q1_idx);

    size_t topo_0 = q0.get_logical_qubit();
    if (topo_0 == SIZE_MAX) {
        cerr << "topo_0 is ERROR CODE" << endl;
        abort();
    }
    size_t topo_1 = q1.get_logical_qubit();
    if (topo_1 == SIZE_MAX) {
        cerr << "topo_1 is ERRORCODE" << endl;
        abort();
    }

    if (topo_0 > topo_1) {
        swap(topo_0, topo_1);
    } else if (topo_0 == topo_1) {
        cerr << "topo_0 == topo_1: " << topo_0 << endl;
        abort();
    }
    if (topo_0 != get<0>(gate.get_qubits()) ||
        topo_1 != get<1>(gate.get_qubits())) {
        return false;
    }

    apply_gate(op, q0, q1);
    return true;
}

/**
 * @brief Apply Single
 *
 * @param op
 * @param gate
 * @return true
 * @return false
 */
bool Checker::apply_single(Operation const& op, Gate const& gate) {
    if ((op.get_type() == GateType::swap) || (op.get_type() == GateType::cx) || (op.get_type() == GateType::cz)) {
        cerr << GATE_TYPE_TO_STR[op.get_type()] << " in applySingle"
             << endl;
        abort();
    }
    size_t q0_idx = get<0>(op.get_qubits());
    if (get<1>(op.get_qubits()) != SIZE_MAX) {
        cerr << "Single gate " << gate.get_id()
             << " has no null second qubit" << endl;
        abort();
    }
    auto& q0 = _device->get_physical_qubit(q0_idx);

    size_t topo_0 = q0.get_logical_qubit();
    if (topo_0 == SIZE_MAX) {
        cerr << "topo_0 is ERROR CODE" << endl;
        abort();
    }

    if (topo_0 != get<0>(gate.get_qubits())) {
        return false;
    }

    apply_gate(op, q0);
    return true;
}

/**
 * @brief Test Operation
 *
 */
bool Checker::test_operations() {
    vector<size_t> finished_gates;

    // cout << "Checking..." << endl;
    dvlab::TqdmWrapper bar{_ops.size(), _tqdm};
    for (auto const& op : _ops) {
        if (op.get_type() == GateType::swap) {
            apply_swap(op);
        } else {
            auto& available_gates = _topo->get_available_gates();
            bool pass_condition = false;
            if (op.get_type() == GateType::cx || op.get_type() == GateType::cz) {
                for (auto gate : available_gates) {
                    if (apply_cx(op, _topo->get_gate(gate))) {
                        pass_condition = true;
                        _topo->update_available_gates(gate);
                        finished_gates.emplace_back(gate);
                        break;
                    }
                }
            } else {
                for (auto gate : available_gates) {
                    if (apply_single(op, _topo->get_gate(gate))) {
                        pass_condition = true;
                        _topo->update_available_gates(gate);
                        finished_gates.emplace_back(gate);
                        break;
                    }
                }
            }
            if (!pass_condition) {
                cerr << "Executed gates:\n";
                for (auto gate : finished_gates) {
                    cerr << gate << "\n";
                }
                cerr << "Available gates:\n";
                for (auto gate : available_gates) {
                    cerr << gate << "\n";
                }
                cerr << "Failed Operation: " << op;
                return false;
            }
        }
        ++bar;
    }
    if (VERBOSE > 3) {
        cout << "\nNum gates: " << finished_gates.size() << "\n"
             << "Num operations:" << _ops.size() << "\n";
    }

    if (finished_gates.size() != _topo->get_num_gates()) {
        cerr << "Number of finished gates " << finished_gates.size()
             << " different from number of gates "
             << _topo->get_num_gates() << endl;
        return false;
    }
    return true;
}