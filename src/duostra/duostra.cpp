/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./duostra.hpp"

#include "./checker.hpp"
#include "./placer.hpp"
#include "qcir/qcir.hpp"

extern size_t VERBOSE;

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
    if (VERBOSE > 3) std::cout << "Creating dependency of quantum circuit..." << std::endl;
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
    if (VERBOSE > 3) std::cout << "Creating dependency of quantum circuit..." << std::endl;
    make_dependency(cir, n_qubit);
}

/**
 * @brief Make dependency graph from QCir*
 *
 */
void Duostra::make_dependency() {
    std::vector<Gate> all_gates;
    for (auto const& g : _logical_circuit->get_gates()) {
        size_t id = g->get_id();

        GateType type = g->get_type();

        size_t q2 = SIZE_MAX;
        QubitInfo first = g->get_qubits()[0];
        QubitInfo second = {};
        if (g->get_qubits().size() > 1) {
            second = g->get_qubits()[1];
            q2 = second._qubit;
        }

        std::tuple<size_t, size_t> temp{first._qubit, q2};
        Gate temp_gate{id, type, g->get_phase(), temp};
        if (first._parent != nullptr) temp_gate.add_prev(first._parent->get_id());
        if (first._child != nullptr) temp_gate.add_next(first._child->get_id());
        if (g->get_qubits().size() > 1) {
            if (second._parent != nullptr) temp_gate.add_prev(second._parent->get_id());
            if (second._child != nullptr) temp_gate.add_next(second._child->get_id());
        }
        all_gates.emplace_back(std::move(temp_gate));
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
        size_t q0_gate = last_gate[get<0>(ops[i].get_qubits())];
        size_t q1_gate = last_gate[get<1>(ops[i].get_qubits())];
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
    topo = make_unique<CircuitTopology>(_dependency);
    auto check_topo = topo->clone();
    auto check_device(_device);

    if (VERBOSE > 3) std::cout << "Creating device..." << std::endl;
    if (topo->get_num_qubits() > _device.get_num_qubits()) {
        std::cerr << "Error: number of logical qubits are larger than the device!!" << std::endl;
        return false;
    }
    std::vector<size_t> assign;
    if (!use_device_as_placement) {
        if (VERBOSE > 3) std::cout << "Initial placing..." << std::endl;
        auto placer = get_placer();
        assign = placer->place_and_assign(_device);
    }
    // scheduler
    if (VERBOSE > 3) std::cout << "Creating Scheduler..." << std::endl;
    auto sched = get_scheduler(std::move(topo), _tqdm);

    // router
    if (VERBOSE > 3) std::cout << "Creating Router..." << std::endl;
    std::string cost = (DuostraConfig::SCHEDULER_TYPE == SchedulerType::greedy) ? "end" : "start";
    auto router = make_unique<Router>(std::move(_device), cost, DuostraConfig::TIE_BREAKING_STRATEGY);

    // routing
    if (!_silent) std::cout << "Routing..." << std::endl;
    _device = sched->assign_gates_and_sort(std::move(router));

    if (stop_requested()) {
        std::cerr << "Warning: mapping interrupted" << std::endl;
        return false;
    }

    if (_check) {
        if (!_silent) std::cout << "Checking..." << std::endl;
        Checker checker(*check_topo, check_device, sched->get_operations(), assign, _tqdm);
        if (!checker.test_operations()) {
            return false;
        }
    }
    if (!_silent) {
        std::cout << "Duostra Result: " << std::endl;
        std::cout << std::endl;
        std::cout << "Scheduler:      " << get_scheduler_type_str(DuostraConfig::SCHEDULER_TYPE) << std::endl;
        std::cout << "Router:         " << get_router_type_str(DuostraConfig::ROUTER_TYPE) << std::endl;
        std::cout << "Placer:         " << get_placer_type_str(DuostraConfig::PLACER_TYPE) << std::endl;
        std::cout << std::endl;
        std::cout << "Mapping Depth:  " << sched->get_final_cost() << "\n";
        std::cout << "Total Time:     " << sched->get_total_time() << "\n";
        std::cout << "#SWAP:          " << sched->get_num_swaps() << "\n";
        std::cout << std::endl;
    }
    assert(sched->is_sorted());
    assert(sched->get_order().size() == _dependency->get_gates().size());
    _result = sched->get_operations();
    store_order_info(sched->get_order());
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
        Gate const& g = _dependency->get_gate(gate_id);
        std::tuple<size_t, size_t> qubits = g.get_qubits();
        if (g.is_swapped())
            qubits = std::make_tuple(get<1>(qubits), get<0>(qubits));
        Operation op(g.get_type(), g.get_phase(), qubits, {});
        op.set_id(g.get_id());
        _order.emplace_back(op);
    }
}

/**
 * @brief Print as qasm form
 *
 */
void Duostra::print_assembly() const {
    std::cout << "Mapping Result: " << std::endl;
    std::cout << std::endl;
    for (auto const& op : _result) {
        std::string gate_name{GATE_TYPE_TO_STR[op.get_type()]};
        std::cout << std::left << std::setw(5) << gate_name << " ";  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        std::tuple<size_t, size_t> qubits = op.get_qubits();
        std::string res = "q[" + std::to_string(get<0>(qubits)) + "]";
        if (get<1>(qubits) != SIZE_MAX) {
            res += ",q[" + std::to_string(get<1>(qubits)) + "]";
        }
        res += ";";
        std::cout << std::left << std::setw(20) << res;  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        std::cout << " // (" << op.get_time_begin() << "," << op.get_time_end() << ")   Origin gate: " << op.get_id() << "\n";
    }
}

/**
 * @brief Construct physical QCir by operation
 *
 */
void Duostra::build_circuit_by_result() {
    _physical_circuit->add_qubits(_device.get_num_qubits());
    for (auto const& operation : _result) {
        std::string gate_name{GATE_TYPE_TO_STR[operation.get_type()]};
        std::tuple<size_t, size_t> qubits = operation.get_qubits();
        QubitIdList qu;
        qu.emplace_back(get<0>(qubits));
        if (get<1>(qubits) != SIZE_MAX) {
            qu.emplace_back(get<1>(qubits));
        }
        if (operation.get_type() == GateType::swap) {
            // NOTE - Decompose SWAP into three CX
            QubitIdList qu_reverse;
            qu_reverse.emplace_back(get<1>(qubits));
            qu_reverse.emplace_back(get<0>(qubits));
            _physical_circuit->add_gate("CX", qu, dvlab::Phase(0), true);
            _physical_circuit->add_gate("CX", qu_reverse, dvlab::Phase(0), true);
            _physical_circuit->add_gate("CX", qu, dvlab::Phase(0), true);
        } else
            _physical_circuit->add_gate(gate_name, qu, operation.get_phase(), true);
    }
}

}  // namespace qsyn::duostra