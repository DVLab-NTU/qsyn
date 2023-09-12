/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./duostra.hpp"

#include <iomanip>

#include "./checker.hpp"
#include "./placer.hpp"
#include "./variables.hpp"
#include "qcir/qcir.hpp"

using namespace std;
extern size_t VERBOSE;

extern bool stop_requested();

// SECTION - Global settings for Duostra mapper

size_t DUOSTRA_SCHEDULER = 4;            // 0:base 1:static 2:random 3:greedy 4:search
size_t DUOSTRA_ROUTER = 1;               // 0:apsp 1:duostra
size_t DUOSTRA_PLACER = 2;               // 0:static 1:random 2:dfs
bool DUOSTRA_ORIENT = 1;                 // t/f smaller logical qubit index with little priority
size_t DUOSTRA_CANDIDATES = (size_t)-1;  // top k candidates, -1: all
size_t DUOSTRA_APSP_COEFF = 1;           // coefficient of apsp cost
bool DUOSTRA_AVAILABLE = 1;              // 0:min 1:max, available time of double-qubit gate is set to min or max of occupied time
bool DUOSTRA_COST = 0;                   // 0:min 1:max, select min or max cost from the waitlist
size_t DUOSTRA_DEPTH = 4;                // depth of searching region
bool DUOSTRA_NEVER_CACHE = 1;            // never cache any children unless children() is called
bool DUOSTRA_EXECUTE_SINGLE = 0;         // execute the single gates when they are available

extern size_t VERBOSE;

/**
 * @brief Get the Scheduler Type Str object
 *
 * @return string
 */
string get_scheduler_type_str() {
    // 0:base 1:static 2:random 3:greedy 4:search
    if (DUOSTRA_SCHEDULER == 0) return "base";
    if (DUOSTRA_SCHEDULER == 1) return "static";
    if (DUOSTRA_SCHEDULER == 2) return "random";
    if (DUOSTRA_SCHEDULER == 3) return "greedy";
    if (DUOSTRA_SCHEDULER == 4)
        return "search";
    else
        return "Error";
}

/**
 * @brief Get the Router Type Str object
 *
 * @return string
 */
string get_router_type_str() {
    // 0:apsp 1:duostra
    if (DUOSTRA_ROUTER == 0) return "apsp";
    if (DUOSTRA_ROUTER == 1)
        return "duostra";
    else
        return "Error";
}

/**
 * @brief Get the Placer Type Str object
 *
 * @return string
 */
string get_placer_type_str() {
    // 0:static 1:random 2:dfs
    if (DUOSTRA_PLACER == 0) return "static";
    if (DUOSTRA_PLACER == 1) return "random";
    if (DUOSTRA_PLACER == 2)
        return "dfs";
    else
        return "Error";
}

/**
 * @brief Get the Scheduler object
 *
 * @param str
 * @return size_t
 */
size_t get_scheduler_type(string const& str) {
    // 0:base 1:static 2:random 3:greedy 4:search
    if (str == "base") return 0;
    if (str == "static") return 1;
    if (str == "random") return 2;
    if (str == "greedy") return 3;
    if (str == "search")
        return 4;
    else
        return (size_t)-1;
}

/**
 * @brief Get the Router object
 *
 * @param str
 * @return size_t
 */
size_t get_router_type(string const& str) {
    // 0:apsp 1:duostra
    if (str == "apsp") return 0;
    if (str == "duostra")
        return 1;
    else
        return (size_t)-1;
}

/**
 * @brief Get the Placer object
 *
 * @param str
 * @return size_t
 */
size_t get_placer_type(string const& str) {
    // 0:static 1:random 2:dfs
    if (str == "static") return 0;
    if (str == "random") return 1;
    if (str == "dfs")
        return 2;
    else
        return (size_t)-1;
}

/**
 * @brief Construct a new Duostra:: Duostra object
 *
 * @param cir
 * @param dev
 * @param check
 * @param tqdm
 * @param silent
 */
Duostra::Duostra(QCir* cir, Device dev, DuostraConfig const& config)
    : _logical_circuit(cir), _device(std::move(dev)), _check(config.verifyResult),
      _tqdm{!config.silent && config.useTqdm}, _silent{config.silent} {
    if (VERBOSE > 3) cout << "Creating dependency of quantum circuit..." << endl;
    make_dependency();
}

/**
 * @brief Construct a new Duostra:: Duostra object
 *
 * @param cir
 * @param dev
 * @param check
 * @param tqdm
 * @param silent
 */
Duostra::Duostra(vector<Operation> const& cir, size_t n_qubit, Device dev, DuostraConfig const& config)
    : _logical_circuit(nullptr), _device(std::move(dev)), _check(config.verifyResult),
      _tqdm{!config.silent && config.useTqdm}, _silent{config.silent} {
    if (VERBOSE > 3) cout << "Creating dependency of quantum circuit..." << endl;
    make_dependency(cir, n_qubit);
}

/**
 * @brief Make dependency graph from QCir*
 *
 */
void Duostra::make_dependency() {
    vector<Gate> all_gates;
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

        tuple<size_t, size_t> temp{first._qubit, q2};
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
void Duostra::make_dependency(vector<Operation> const& oper, size_t n_qubit) {
    vector<size_t> last_gate;  // idx:qubit value: Gate id
    vector<Gate> all_gates;
    last_gate.resize(n_qubit, SIZE_MAX);
    for (size_t i = 0; i < oper.size(); i++) {
        Gate temp_gate{i, oper[i].get_type(), oper[i].get_phase(), oper[i].get_qubits()};
        size_t q0_gate = last_gate[get<0>(oper[i].get_qubits())];
        size_t q1_gate = last_gate[get<1>(oper[i].get_qubits())];
        temp_gate.add_prev(q0_gate);
        if (q0_gate != q1_gate)
            temp_gate.add_prev(q1_gate);
        if (q0_gate != SIZE_MAX)
            all_gates[q0_gate].add_next(i);
        if (q1_gate != SIZE_MAX && q1_gate != q0_gate) {
            all_gates[q1_gate].add_next(i);
        }
        last_gate[get<0>(oper[i].get_qubits())] = i;
        last_gate[get<1>(oper[i].get_qubits())] = i;
        all_gates.emplace_back(std::move(temp_gate));
    }
    _dependency = make_shared<DependencyGraph>(n_qubit, std::move(all_gates));
}

/**
 * @brief Main flow of Duostra mapper
 *
 * @return size_t
 */
size_t Duostra::flow(bool use_device_as_placement) {
    unique_ptr<CircuitTopology> topo;
    topo = make_unique<CircuitTopology>(_dependency);
    auto check_topo = topo->clone();
    auto check_device(_device);

    if (VERBOSE > 3) cout << "Creating device..." << endl;
    if (topo->get_num_qubits() > _device.get_num_qubits()) {
        cerr << "Error: number of logical qubits are larger than the device!!" << endl;
        return SIZE_MAX;
    }
    vector<size_t> assign;
    if (!use_device_as_placement) {
        if (VERBOSE > 3) cout << "Initial placing..." << endl;
        auto placer = get_placer();
        assign = placer->place_and_assign(_device);
    }
    // scheduler
    if (VERBOSE > 3) cout << "Creating Scheduler..." << endl;
    auto sched = get_scheduler(std::move(topo), _tqdm);

    // router
    if (VERBOSE > 3) cout << "Creating Router..." << endl;
    string cost = (DUOSTRA_SCHEDULER == 3) ? "end" : "start";
    auto router = make_unique<Router>(std::move(_device), cost, DUOSTRA_ORIENT);

    // routing
    if (!_silent) cout << "Routing..." << endl;
    _device = sched->assign_gates_and_sort(std::move(router));

    if (stop_requested()) {
        cerr << "Warning: mapping interrupted" << endl;
        return SIZE_MAX;
    }

    if (_check) {
        if (!_silent) cout << "Checking..." << endl;
        Checker checker(*check_topo, check_device, sched->get_operations(), assign, _tqdm);
        if (!checker.test_operations()) {
            return SIZE_MAX;
        }
    }
    if (!_silent) {
        cout << "Duostra Result: " << endl;
        cout << endl;
        cout << "Scheduler:      " << get_scheduler_type_str() << endl;
        cout << "Router:         " << get_router_type_str() << endl;
        cout << "Placer:         " << get_placer_type_str() << endl;
        cout << endl;
        cout << "Mapping Depth:  " << sched->get_final_cost() << "\n";
        cout << "Total Time:     " << sched->get_total_time() << "\n";
        cout << "#SWAP:          " << sched->get_num_swaps() << "\n";
        cout << endl;
    }
    assert(sched->is_sorted());
    assert(sched->get_order().size() == _dependency->get_gates().size());
    _result = sched->get_operations();
    store_order_info(sched->get_order());
    build_circuit_by_result();
    cout.clear();
    return sched->get_final_cost();
}

/**
 * @brief Convert index to full information of gate
 *
 * @param order
 */
void Duostra::store_order_info(std::vector<size_t> const& order) {
    for (auto const& gate_id : order) {
        Gate const& g = _dependency->get_gate(gate_id);
        tuple<size_t, size_t> qubits = g.get_qubits();
        if (g.is_swapped())
            qubits = make_tuple(get<1>(qubits), get<0>(qubits));
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
    cout << "Mapping Result: " << endl;
    cout << endl;
    for (size_t i = 0; i < _result.size(); ++i) {
        auto const& op = _result.at(i);
        string gate_name{GATE_TYPE_TO_STR[op.get_type()]};
        cout << left << setw(5) << gate_name << " ";  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        tuple<size_t, size_t> qubits = op.get_qubits();
        string res = "q[" + to_string(get<0>(qubits)) + "]";
        if (get<1>(qubits) != SIZE_MAX) {
            res += ",q[" + to_string(get<1>(qubits)) + "]";
        }
        res += ";";
        cout << left << setw(20) << res;  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        cout << " // (" << op.get_operation_time() << "," << op.get_cost() << ")   Origin gate: " << op.get_id() << "\n";
    }
}

/**
 * @brief Construct physical QCir by operation
 *
 */
void Duostra::build_circuit_by_result() {
    _physical_circuit->add_qubits(_device.get_num_qubits());
    for (auto const& operation : _result) {
        string gate_name{GATE_TYPE_TO_STR[operation.get_type()]};
        tuple<size_t, size_t> qubits = operation.get_qubits();
        vector<size_t> qu;
        qu.emplace_back(get<0>(qubits));
        if (get<1>(qubits) != SIZE_MAX) {
            qu.emplace_back(get<1>(qubits));
        }
        if (operation.get_type() == GateType::swap) {
            // NOTE - Decompose SWAP into three CX
            vector<size_t> qu_reverse;
            qu_reverse.emplace_back(get<1>(qubits));
            qu_reverse.emplace_back(get<0>(qubits));
            _physical_circuit->add_gate("CX", qu, Phase(0), true);
            _physical_circuit->add_gate("CX", qu_reverse, Phase(0), true);
            _physical_circuit->add_gate("CX", qu, Phase(0), true);
        } else
            _physical_circuit->add_gate(gate_name, qu, operation.get_phase(), true);
    }
}
