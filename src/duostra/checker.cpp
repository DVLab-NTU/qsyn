/****************************************************************************
  FileName     [ checker.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Checker member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "checker.h"

using namespace std;
extern size_t verbose;

/**
 * @brief Construct a new Checker:: Checker object
 *
 * @param topo
 * @param device
 * @param ops
 * @param assign
 * @param tqdm
 */
Checker::Checker(CircuitTopo& topo,
                 Device& device,
                 const vector<Operation>& ops,
                 const vector<size_t>& assign, bool tqdm)
    : _topo(topo), _device(device), _ops(ops), _tqdm(tqdm) {
    _device.place(assign);
}

/**
 * @brief Get Cycle
 *
 * @param type
 * @return size_t
 */
size_t Checker::getCycle(GateType type) {
    switch (type) {
        case GateType::SWAP:
            return SWAP_DELAY;
        case GateType::CX:
            return DOUBLE_DELAY;
        case GateType::CZ:
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
void Checker::applyGate(const Operation& op, PhysicalQubit& q0) {
    size_t start = get<0>(op.getDuration());
    size_t end = get<1>(op.getDuration());

    if (!(start >= q0.getOccupiedTime())) {
        cerr << op << "\n"
             << "Q" << q0.getId() << " occu: " << q0.getOccupiedTime()
             << endl;
        abort();
    }
    if (!(end == start + getCycle(op.getType()))) {
        cerr << op << endl;
        abort();
    }
    q0.setOccupiedTime(end);
}

/**
 * @brief Apply Gate
 *
 * @param op
 * @param q0
 * @param q1
 */
void Checker::applyGate(const Operation& op,
                        PhysicalQubit& q0,
                        PhysicalQubit& q1) {
    size_t start = get<0>(op.getDuration());
    size_t end = get<1>(op.getDuration());

    if (!(start >= q0.getOccupiedTime() && start >= q1.getOccupiedTime())) {
        cerr << op << "\n"
             << "Q" << q0.getId() << " occu: " << q0.getOccupiedTime()
             << "\n"
             << "Q" << q1.getId() << " occu: " << q1.getOccupiedTime()
             << endl;
        abort();
    }
    if (!(end == start + getCycle(op.getType()))) {
        cerr << op << endl;
        abort();
    }
    q0.setOccupiedTime(end);
    q1.setOccupiedTime(end);
}

/**
 * @brief Apply Swap
 *
 * @param op
 */
void Checker::applySwap(const Operation& op) {
    if (op.getType() != GateType::SWAP) {
        cerr << gateType2Str[op.getType()] << " in applySwap"
             << endl;
        abort();
    }
    size_t q0_idx = get<0>(op.getQubits());
    size_t q1_idx = get<1>(op.getQubits());
    auto& q0 = _device.getPhysicalQubit(q0_idx);
    auto& q1 = _device.getPhysicalQubit(q1_idx);
    applyGate(op, q0, q1);

    // swap
    size_t temp = q0.getLogicalQubit();
    q0.setLogicalQubit(q1.getLogicalQubit());
    q1.setLogicalQubit(temp);
}

/**
 * @brief Apply CX
 *
 * @param op
 * @param gate
 * @return true
 * @return false
 */
bool Checker::applyCX(const Operation& op, const Gate& gate) {
    if (!(op.getType() == GateType::CX || op.getType() == GateType::CZ)) {
        cerr << gateType2Str[op.getType()] << " in applyCX" << endl;
        abort();
    }
    size_t q0_idx = get<0>(op.getQubits());
    size_t q1_idx = get<1>(op.getQubits());
    auto& q0 = _device.getPhysicalQubit(q0_idx);
    auto& q1 = _device.getPhysicalQubit(q1_idx);

    size_t topo_0 = q0.getLogicalQubit();
    if (topo_0 == ERROR_CODE) {
        cerr << "topo_0 is ERROR CODE" << endl;
        abort();
    }
    size_t topo_1 = q1.getLogicalQubit();
    if (topo_1 == ERROR_CODE) {
        cerr << "topo_1 is ERRORCODE" << endl;
        abort();
    }

    if (topo_0 > topo_1) {
        swap(topo_0, topo_1);
    } else if (topo_0 == topo_1) {
        cerr << "topo_0 == topo_1: " << topo_0 << endl;
        abort();
    }
    if (topo_0 != get<0>(gate.getQubits()) ||
        topo_1 != get<1>(gate.getQubits())) {
        return false;
    }

    applyGate(op, q0, q1);
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
bool Checker::applySingle(const Operation& op, const Gate& gate) {
    if ((op.getType() == GateType::SWAP) || (op.getType() == GateType::CX) || (op.getType() == GateType::CZ)) {
        cerr << gateType2Str[op.getType()] << " in applySingle"
             << endl;
        abort();
    }
    size_t q0_idx = get<0>(op.getQubits());
    if (get<1>(op.getQubits()) != ERROR_CODE) {
        cerr << "Single gate " << gate.getId()
             << " has no null second qubit" << endl;
        abort();
    }
    auto& q0 = _device.getPhysicalQubit(q0_idx);

    size_t topo_0 = q0.getLogicalQubit();
    if (topo_0 == ERROR_CODE) {
        cerr << "topo_0 is ERROR CODE" << endl;
        abort();
    }

    if (topo_0 != get<0>(gate.getQubits())) {
        return false;
    }

    applyGate(op, q0);
    return true;
}

/**
 * @brief Test Operation
 *
 */
bool Checker::testOperations() {
    vector<size_t> finishedGates;

    // cout << "Checking..." << endl;
    TqdmWrapper bar{_ops.size(), _tqdm};
    for (const auto& op : _ops) {
        if (op.getType() == GateType::SWAP) {
            applySwap(op);
        } else {
            auto& availableGates = _topo.getAvailableGates();
            bool passCondition = false;
            if (op.getType() == GateType::CX || op.getType() == GateType::CZ) {
                for (auto gate : availableGates) {
                    if (applyCX(op, _topo.getGate(gate))) {
                        passCondition = true;
                        _topo.updateAvailableGates(gate);
                        finishedGates.push_back(gate);
                        break;
                    }
                }
            } else {
                for (auto gate : availableGates) {
                    if (applySingle(op, _topo.getGate(gate))) {
                        passCondition = true;
                        _topo.updateAvailableGates(gate);
                        finishedGates.push_back(gate);
                        break;
                    }
                }
            }
            if (!passCondition) {
                cerr << "Executed gates:\n";
                for (auto gate : finishedGates) {
                    cerr << gate << "\n";
                }
                cerr << "Available gates:\n";
                for (auto gate : availableGates) {
                    cerr << gate << "\n";
                }
                cerr << "Failed Operation: " << op;
                return false;
            }
        }
        ++bar;
    }
    if (verbose > 3) {
        cout << "\nNum gates: " << finishedGates.size() << "\n"
             << "Num operations:" << _ops.size() << "\n";
    }

    if (finishedGates.size() != _topo.getNumGates()) {
        cerr << "Number of finished gates " << finishedGates.size()
             << " different from number of gates "
             << _topo.getNumGates() << endl;
        return false;
    }
    return true;
}