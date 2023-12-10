/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./duostra.hpp"

#include <spdlog/spdlog.h>

#include "./checker.hpp"
#include "./placer.hpp"
#include "qcir/qcir.hpp"
#include "qsyn/qsyn_type.hpp"

extern bool stop_requested();

using namespace qsyn::qcir;

namespace qsyn::duostra {

/**
 * @brief Construct a new Duostra Mapper object
 *
 * @param cir
 * @param dev
 * @param check
 * @param tqdm
 * @param silent
 */
Duostra::Duostra(QCir* cir, Device dev, DuostraExecutionOptions const& config)
    : _logical_circuit(cir), _device(std::move(dev)), _check(config.verify_result),
      _tqdm{!config.silent && config.use_tqdm}, _silent{config.silent} {
    make_dependency();
}

/**
 * @brief Construct a new Duostra Mapper object
 *
 * @param cir
 * @param dev
 * @param check
 * @param tqdm
 * @param silent
 */
Duostra::Duostra(std::vector<Operation> const& cir, size_t n_qubit, Device dev, DuostraExecutionOptions const& config)
    : _logical_circuit(nullptr), _device(std::move(dev)), _check(config.verify_result),
      _tqdm{!config.silent && config.use_tqdm}, _silent{config.silent} {
    make_dependency(cir, n_qubit);
}

/**
 * @brief Make dependency graph from QCir*
 *
 */
void Duostra::make_dependency() {
    spdlog::info("Creating dependency of quantum circuit...");
    std::vector<Gate> all_gates;
    for (auto const& g : _logical_circuit->get_gates()) {
        auto rotation_category = g->get_rotation_category();

        auto q2          = max_qubit_id;
        auto const first = g->get_qubits()[0];
        auto second      = QubitInfo{};
        if (g->get_qubits().size() > 1) {
            second = g->get_qubits()[1];
            q2     = second._qubit;
        }

        auto const temp = std::make_tuple(first._qubit, q2);
        Gate temp_gate{g->get_id(), rotation_category, g->get_phase(), temp};
        if (first._prev != nullptr) temp_gate.add_prev(first._prev->get_id());
        if (first._next != nullptr) temp_gate.add_next(first._next->get_id());
        if (g->get_qubits().size() > 1) {
            if (second._prev != nullptr) temp_gate.add_prev(second._prev->get_id());
            if (second._next != nullptr) temp_gate.add_next(second._next->get_id());
        }
        all_gates.emplace_back(std::move(temp_gate));
    }
    std::unordered_map<size_t, size_t> reordered_map;
    for (size_t i = 0; i < all_gates.size(); i++) {
        reordered_map[all_gates[i].get_id()] = i;
    }
    for (size_t i = 0; i < all_gates.size(); i++) {
        all_gates[i].set_id(reordered_map[all_gates[i].get_id()]);
        all_gates[i].set_prevs(reordered_map);
        all_gates[i].set_nexts(reordered_map);
    }
    _dependency = make_shared<DependencyGraph>(_logical_circuit->get_num_qubits(), std::move(all_gates));
}

/**
 * @brief Make dependency graph from vector<Operation>
 *
 * @param oper in topological order
 */
void Duostra::make_dependency(std::vector<Operation> const& ops, size_t n_qubits) {
    std::vector<size_t> last_gate;  // idx:qubit value: Gate id
    std::vector<Gate> all_gates;
    last_gate.resize(n_qubits, SIZE_MAX);
    for (size_t i = 0; i < ops.size(); i++) {
        Gate temp_gate{i, ops[i].get_type(), ops[i].get_phase(), ops[i].get_qubits()};
        auto const q0_gate = last_gate[get<0>(ops[i].get_qubits())];
        auto const q1_gate = last_gate[get<1>(ops[i].get_qubits())];
        temp_gate.add_prev(q0_gate);
        if (q0_gate != q1_gate)
            temp_gate.add_prev(q1_gate);
        if (q0_gate != SIZE_MAX)
            all_gates[q0_gate].add_next(i);
        if (q1_gate != SIZE_MAX && q1_gate != q0_gate) {
            all_gates[q1_gate].add_next(i);
        }
        last_gate[get<0>(ops[i].get_qubits())] = i;
        last_gate[get<1>(ops[i].get_qubits())] = i;
        all_gates.emplace_back(std::move(temp_gate));
    }
    _dependency = make_shared<DependencyGraph>(n_qubits, std::move(all_gates));
}

/**
 * @brief Main flow of Duostra mapper
 *
 * @return size_t
 */
bool Duostra::map(bool use_device_as_placement) {
    std::unique_ptr<CircuitTopology> topo;
    topo            = make_unique<CircuitTopology>(_dependency);
    auto check_topo = topo->clone();
    auto check_device(_device);

    spdlog::info("Creating device...");
    if (topo->get_num_qubits() > _device.get_num_qubits()) {
        spdlog::error("Number of logical qubits are larger than the device!!");
        return false;
    }

    std::vector<QubitIdType> assign;
    if (!use_device_as_placement) {
        spdlog::info("Calculating Initial Placement...");
        auto placer = get_placer();
        assign      = placer->place_and_assign(_device);
    }
    // scheduler
    spdlog::info("Creating Scheduler...");
    auto scheduler = get_scheduler(std::move(topo), _tqdm);

    // router
    spdlog::info("Creating Router...");
    auto cost_strategy = (DuostraConfig::SCHEDULER_TYPE == SchedulerType::greedy) ? Router::CostStrategyType::end
                                                                                  : Router::CostStrategyType::start;
    auto router        = std::make_unique<Router>(std::move(_device), cost_strategy, DuostraConfig::TIE_BREAKING_STRATEGY);

    // routing
    if (!_silent) {
        fmt::println("Routing...");
    }
    _device = scheduler->assign_gates_and_sort(std::move(router));

    if (stop_requested()) {
        spdlog::warn("Warning: mapping interrupted");
        return false;
    }

    if (_check) {
        if (!_silent) {
            fmt::println("Checking...");
        }
        Checker checker(*check_topo, check_device, scheduler->get_operations(), assign, _tqdm);
        if (!checker.test_operations()) {
            return false;
        }
    }
    if (!_silent) {
        fmt::println("Duostra Result: ");
        fmt::println("");
        fmt::println("Scheduler:      {}", get_scheduler_type_str(DuostraConfig::SCHEDULER_TYPE));
        fmt::println("Router:         {}", get_router_type_str(DuostraConfig::ROUTER_TYPE));
        fmt::println("Placer:         {}", get_placer_type_str(DuostraConfig::PLACER_TYPE));
        fmt::println("");
        fmt::println("Mapping Depth:  {}", scheduler->get_final_cost());
        fmt::println("Total Time:     {}", scheduler->get_total_time());
        fmt::println("#SWAP:          {}", scheduler->get_num_swaps());
        fmt::println("");
    }
    assert(scheduler->is_sorted());
    assert(scheduler->get_order().size() == _dependency->get_gates().size());
    _result = scheduler->get_operations();
    store_order_info(scheduler->get_order());
    build_circuit_by_result();

    return true;
}

/**
 * @brief Convert index to full information of gate
 *
 * @param order
 */
void Duostra::store_order_info(std::vector<size_t> const& order) {
    for (auto const& gate_id : order) {
        Gate const& g                     = _dependency->get_gate(gate_id);
        std::tuple<size_t, size_t> qubits = g.get_qubits();
        if (g.is_swapped())
            qubits = std::make_tuple(get<1>(qubits), get<0>(qubits));
        Operation op(g.get_type(), g.get_phase(), qubits, {});
        op.set_id(g.get_id());
        _order.emplace_back(op);
    }
}

/**
 * @brief Construct physical QCir by operation
 *
 */
void Duostra::build_circuit_by_result() {
    _physical_circuit->add_qubits(_device.get_num_qubits());
    for (auto const& operation : _result) {
        auto qubits = operation.get_qubits();
        QubitIdList qu;
        qu.emplace_back(get<0>(qubits));
        if (get<1>(qubits) != max_qubit_id) {
            qu.emplace_back(get<1>(qubits));
        }
        if (operation.is_swap()) {
            // NOTE - Decompose SWAP into three CX
            QubitIdList qu_reverse;
            qu_reverse.emplace_back(get<1>(qubits));
            qu_reverse.emplace_back(get<0>(qubits));
            _physical_circuit->add_gate("CX", qu, dvlab::Phase(1), true);
            _physical_circuit->add_gate("CX", qu_reverse, dvlab::Phase(1), true);
            _physical_circuit->add_gate("CX", qu, dvlab::Phase(1), true);
        } else if (operation.get_phase() != dvlab::Phase(0)) {
            _physical_circuit->add_gate(operation.get_type_str(), qu, operation.get_phase(), true);
        }
    }
}

}  // namespace qsyn::duostra
