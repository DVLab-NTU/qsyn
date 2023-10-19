/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Checker member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./checker.hpp"

#include <spdlog/spdlog.h>

#include "./circuit_topology.hpp"
#include "qcir/gate_type.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/util.hpp"

using namespace qsyn::qcir;

namespace qsyn::duostra {

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
                 Checker::Device& device,
                 std::vector<Checker::Operation> const& ops,
                 std::vector<QubitIdType> const& assign, bool tqdm)
    : _topo(&topo), _device(&device), _ops(ops), _tqdm(tqdm) {
    _device->place(assign);
}

/**
 * @brief Get Cycle
 *
 * @param type
 * @return size_t
 */
size_t Checker::get_cycle(Operation const& op) {
    if (op.is_swap()) {
        return SWAP_DELAY;
    } else if (op.is_cx() || op.is_cz()) {
        return DOUBLE_DELAY;
    } else {
        return SINGLE_DELAY;
    }
}

/**
 * @brief Apply Gate
 *
 * @param op
 * @param q0
 */
void Checker::apply_gate(Checker::Operation const& op, Checker::PhysicalQubit& q0) {
    size_t start = get<0>(op.get_time_range());
    size_t end   = get<1>(op.get_time_range());

    DVLAB_ASSERT(
        start >= q0.get_occupied_time(),
        fmt::format("The starting time of Operation {} is less than Qubit {}'s occupied time of ({}) !!", op.get_id(), q0.get_id(), q0.get_occupied_time()));

    DVLAB_ASSERT(
        end == start + get_cycle(op),
        fmt::format("The ending time of Operation {} is not equal to start time + cycle!!", op.get_id()));

    q0.set_occupied_time(end);
}

/**
 * @brief Apply Gate
 *
 * @param op
 * @param q0
 * @param q1
 */
void Checker::apply_gate(Checker::Operation const& op,
                         Checker::PhysicalQubit& q0,
                         Checker::PhysicalQubit& q1) {
    size_t start = get<0>(op.get_time_range());
    size_t end   = get<1>(op.get_time_range());

    DVLAB_ASSERT(
        start >= q0.get_occupied_time(),
        fmt::format("The starting time of Operation {} is less than Qubit {}'s occupied time of ({}) !!", op.get_id(), q0.get_id(), q0.get_occupied_time()));
    DVLAB_ASSERT(
        start >= q1.get_occupied_time(),
        fmt::format("The starting time of Operation {} is less than Qubit {}'s occupied time of ({}) !!", op.get_id(), q1.get_id(), q1.get_occupied_time()));
    DVLAB_ASSERT(end == start + get_cycle(op), fmt::format("The ending time of Operation {} is not equal to start time + cycle!!", op.get_id()));

    q0.set_occupied_time(end);
    q1.set_occupied_time(end);
}

/**
 * @brief Apply Swap
 *
 * @param op
 */
void Checker::apply_swap(Checker::Operation const& op) {
    DVLAB_ASSERT(op.is_swap(), fmt::format("Gate type {} in apply_swap is not allowed!!", op.get_type_str()));

    auto q0_idx = get<0>(op.get_qubits());
    auto q1_idx = get<1>(op.get_qubits());
    auto& q0    = _device->get_physical_qubit(q0_idx);
    auto& q1    = _device->get_physical_qubit(q1_idx);
    apply_gate(op, q0, q1);

    // swap
    auto temp = q0.get_logical_qubit();
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
    DVLAB_ASSERT(op.is_cx() || op.is_cz(), fmt::format("Gate type {} in apply_cx is not allowed!!", op.get_type_str()));
    auto q0_idx = get<0>(op.get_qubits());
    auto q1_idx = get<1>(op.get_qubits());
    auto& q0    = _device->get_physical_qubit(q0_idx);
    auto& q1    = _device->get_physical_qubit(q1_idx);

    auto topo_0 = q0.get_logical_qubit();
    auto topo_1 = q1.get_logical_qubit();

    DVLAB_ASSERT(topo_0.has_value(), "topo_0 does not have value");
    DVLAB_ASSERT(topo_1.has_value(), "topo_1 does not have value");
    DVLAB_ASSERT(topo_0 != topo_1, "topo_0 should not be equal to topo_1");

    if (topo_0 > topo_1) {
        std::swap(topo_0, topo_1);
    }
    if (topo_0 != std::get<0>(gate.get_qubits()) ||
        topo_1 != std::get<1>(gate.get_qubits())) {
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
    DVLAB_ASSERT(
        !op.is_swap() && !op.is_cx() && !op.is_cz(),
        fmt::format("Gate type {} in apply_single is not allowed!!", op.get_type_str()));

    auto q0_idx = get<0>(op.get_qubits());

    DVLAB_ASSERT(
        !op.is_double_qubit_gate(),
        fmt::format("Single gate {} has no null second qubit", gate.get_id()));

    auto& q0 = _device->get_physical_qubit(q0_idx);

    auto topo_0 = q0.get_logical_qubit();

    DVLAB_ASSERT(topo_0.has_value(), "topo_0 does not have value");

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
    std::vector<size_t> finished_gates;
    for (dvlab::TqdmWrapper bar{_ops.size(), _tqdm}; !bar.done(); ++bar) {
        auto& op = _ops[bar.idx()];
        if (op.is_swap()) {
            apply_swap(op);
        } else {
            auto& available_gates = _topo->get_available_gates();
            bool pass_condition   = false;
            if (op.is_cx() || op.is_cz()) {
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
                spdlog::error("Could not match operation {} to any logical gates!!", op.get_id());
                spdlog::error("  Executed gates:   {}", fmt::join(finished_gates, " "));
                spdlog::error("  Available gates:  {}", fmt::join(available_gates, " "));
                spdlog::error("  Failed Operation: {}, type: ", op.get_id(), op.get_type_str());
                return false;
            }
        }
    }
    spdlog::info("");
    spdlog::info("#gates: {}", finished_gates.size());
    spdlog::info("#operations: {}", _ops.size());

    if (finished_gates.size() != _topo->get_num_gates()) {
        spdlog::error("Number of finished gates ({}) is different from number of gates ({})!!", finished_gates.size(), _topo->get_num_gates());
        return false;
    }
    return true;
}

}  // namespace qsyn::duostra
