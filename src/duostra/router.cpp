/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Router member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./router.hpp"

#include <gsl/util>

#include "./circuit_topology.hpp"
#include "./variables.hpp"
#include "util/util.hpp"

extern size_t VERBOSE;

using namespace qsyn::qcir;

namespace qsyn::duostra {

// SECTION - Class AStarNode Member Functions

/**
 * @brief Construct a new AStarNode::AStarNode object
 *
 * @param cost
 * @param id
 * @param source
 */
AStarNode::AStarNode(size_t cost, size_t id, bool source)
    : _estimated_cost(cost), _id(id), _source(source) {}

// SECTION - Class Router Member Functions

/**
 * @brief Construct a new Router:: Router object
 *
 * @param device
 * @param cost
 * @param orient
 */
Router::Router(Device&& device, std::string const& cost, bool orient)
    : _greedy_type(false),
      _duostra(false),
      _orient(orient),
      _apsp(false),
      _device(device),
      _logical_to_physical({}) {
    _initialize(cost);
}

/**
 * @brief Clone router
 *
 * @return unique_ptr<Router>
 */
std::unique_ptr<Router> Router::clone() const {
    return std::make_unique<Router>(*this);
}

/**
 * @brief Initialize router
 *
 * @param type
 * @param cost
 */
void Router::_initialize(std::string const& cost) {
    // DUOSTRA_ROUTER 0:apsp 1:duostra
    if (DUOSTRA_ROUTER == 0) {
        _apsp = true;
        _duostra = false;
    } else if (DUOSTRA_ROUTER == 1) {
        _duostra = true;
    } else {
        std::cerr << "Error: router type not found" << std::endl;
        abort();
    }

    if (cost == "end") {
        _greedy_type = true;
        _apsp = true;
    } else if (cost == "start") {
        _greedy_type = false;
    } else {
        std::cerr << cost << " is not a cost type" << std::endl;
        abort();
    }

    if (_apsp) {
        _device.calculate_path();
    }

    size_t num_qubits = _device.get_num_qubits();
    _logical_to_physical.resize(num_qubits);
    for (size_t i = 0; i < num_qubits; ++i) {
        _logical_to_physical[_device.get_physical_qubit(i).get_logical_qubit()] = i;
    }
}

/**
 * @brief Get physical qubits of gate
 *
 * @param gate
 * @return tuple<size_t, size_t> Note: (_, SIZE_MAX) if single-qubit gate
 */
std::tuple<size_t, size_t> Router::_get_physical_qubits(Gate const& gate) const {
    // NOTE - Only for 1- or 2-qubit gates
    size_t logical_id0 = get<0>(gate.get_qubits());           // get logical qubit index of gate in topology
    size_t physical_id0 = _logical_to_physical[logical_id0];  // get physical qubit index of the gate
    size_t physical_id1 = SIZE_MAX;

    if ((gate.get_type() == GateType::cx || gate.get_type() == GateType::cz)) {
        size_t logical_id1 = get<1>(gate.get_qubits());  // get logical qubit index of gate in topology
        assert(logical_id1 != SIZE_MAX);
        physical_id1 = _logical_to_physical[logical_id1];  // get physical qubit index of the gate
    }
    return std::make_tuple(physical_id0, physical_id1);
}

/**
 * @brief Get cost of gate
 *
 * @param gate
 * @param minMax
 * @param APSPCoeff
 * @return size_t
 */
size_t Router::get_gate_cost(Gate const& gate, bool min_max, size_t apsp_coeff) {
    std::tuple<size_t, size_t> physical_qubits_ids = _get_physical_qubits(gate);

    if (!(gate.get_type() == GateType::cx || gate.get_type() == GateType::cz)) {
        assert(get<1>(physical_qubits_ids) == SIZE_MAX);
        return _device.get_physical_qubit(get<0>(physical_qubits_ids)).get_occupied_time();
    }

    size_t q0_id = get<0>(physical_qubits_ids);
    size_t q1_id = get<1>(physical_qubits_ids);
    PhysicalQubit const& q0 = _device.get_physical_qubit(q0_id);
    PhysicalQubit const& q1 = _device.get_physical_qubit(q1_id);
    auto apsp_cost = _apsp ? _device.get_path(q0_id, q1_id).size() : 0;

    size_t avail = min_max ? std::max(q0.get_occupied_time(), q1.get_occupied_time()) : std::min(q0.get_occupied_time(), q1.get_occupied_time());
    return avail + apsp_cost / apsp_coeff;
}

/**
 * @brief Is gate executable or not
 *
 * @param gate
 * @return true
 * @return false
 */
bool Router::is_executable(Gate const& gate) {
    if (!(gate.get_type() == GateType::cx || gate.get_type() == GateType::cz)) {
        assert(get<1>(gate.get_qubits()) == SIZE_MAX);
        return true;
    }

    std::tuple<size_t, size_t> physical_qubits_ids{_get_physical_qubits(gate)};
    assert(get<1>(physical_qubits_ids) != SIZE_MAX);
    PhysicalQubit const& q0 = _device.get_physical_qubit(get<0>(physical_qubits_ids));
    PhysicalQubit const& q1 = _device.get_physical_qubit(get<1>(physical_qubits_ids));
    return q0.is_adjacency(q1);
}

/**
 * @brief
 *
 * @param gate
 * @param phase
 * @param q
 * @return Operation
 */
Router::Operation Router::execute_single(GateType gate, dvlab::Phase phase, size_t q) {
    PhysicalQubit& qubit = _device.get_physical_qubit(q);
    size_t start_time = qubit.get_occupied_time();
    size_t end_time = start_time + SINGLE_DELAY;
    qubit.set_occupied_time(end_time);
    qubit.reset();
    Operation op(gate, phase, std::make_tuple(q, SIZE_MAX), std::make_tuple(start_time, end_time));
    if (VERBOSE > 3) std::cout << op << std::endl;
    return op;
}

/**
 * @brief Route gate by Duostra
 *
 * @param gate
 * @param phase
 * @param qubitPair
 * @param orient
 * @param swapped if the qubits of gate are swapped when added into Duostra
 * @return vector<Operation>
 */
std::vector<Router::Operation> Router::duostra_routing(GateType gate, size_t gate_id, dvlab::Phase phase, std::tuple<size_t, size_t> qubit_pair, bool orient, bool swapped) {
    assert(gate == GateType::cx || gate == GateType::cz);
    size_t q0_id = get<0>(qubit_pair);  // source 0
    size_t q1_id = get<1>(qubit_pair);  // source 1
    bool swap_ids = false;
    // If two sources compete for the same qubit, the one with smaller occupied time goes first
    if (_device.get_physical_qubit(q0_id).get_occupied_time() >
        _device.get_physical_qubit(q1_id).get_occupied_time()) {
        std::swap(q0_id, q1_id);
        swap_ids = true;
    } else if (_device.get_physical_qubit(q0_id).get_occupied_time() ==
               _device.get_physical_qubit(q1_id).get_occupied_time()) {
        // orientation means qubit with smaller logical idx has a little priority
        if (orient && _device.get_physical_qubit(q0_id).get_logical_qubit() >
                          _device.get_physical_qubit(q1_id).get_logical_qubit()) {
            std::swap(q0_id, q1_id);
            swap_ids = true;
        }
    }

    PhysicalQubit& t0 = _device.get_physical_qubit(q0_id);  // target 0
    PhysicalQubit& t1 = _device.get_physical_qubit(q1_id);  // target 1
    // priority queue: pop out the node with the smallest cost from both the sources
    PriorityQueue pq;

    // init conditions for the sources
    t0.mark(false, t0.get_id());
    t0.take_route(t0.get_cost(), 0);
    t1.mark(true, t1.get_id());
    t1.take_route(t1.get_cost(), 0);
    std::tuple<bool, size_t> touch0 = _touch_adjacency(t0, pq, false);
    bool is_adjacent = get<0>(touch0);
#ifdef DEBUG
    tuple<bool, size_t> touch1 = touchAdjacency(t1, pq, true);
    assert(isAdjacent == get<0>(touch1));
#else
    _touch_adjacency(t1, pq, true);
#endif

    // the two paths from the two sources propagate until the two paths meet each other
    while (!is_adjacent) {
        // each iteration gets an element from the priority queue
        AStarNode next(pq.top());
        pq.pop();
        size_t q_next_id = next.get_id();
        PhysicalQubit& q_next = _device.get_physical_qubit(q_next_id);
        // FIXME - swtch to source
        assert(q_next.get_source() == next.get_source());

        // mark the element as visited and check its neighbors
        size_t cost = next.get_cost();
        assert(cost >= SWAP_DELAY);
        size_t operation_time = cost - SWAP_DELAY;
        q_next.take_route(cost, operation_time);
        std::tuple<bool, size_t> touch = _touch_adjacency(q_next, pq, next.get_source());
        is_adjacent = get<0>(touch);
        if (is_adjacent) {
            if (next.get_source())  // 0 get true means touch 1's set
            {
                q0_id = get<1>(touch);
                q1_id = q_next_id;
            } else {
                q0_id = q_next_id;
                q1_id = get<1>(touch);
            }
        }
    }
    std::vector<Operation> operation_list =
        _traceback(gate, gate_id, phase, _device.get_physical_qubit(q0_id), _device.get_physical_qubit(q1_id), t0, t1, swap_ids, swapped);

    if (VERBOSE > 3) {
        for (size_t i = 0; i < operation_list.size(); ++i) {
            std::cout << operation_list[i] << std::endl;
        }
    }

#ifdef DEBUG
    vector<bool> checker(_device.getNQubit(), false);
#endif
    for (size_t i = 0; i < _device.get_num_qubits(); ++i) {
        PhysicalQubit& qubit = _device.get_physical_qubit(i);
        qubit.reset();
        assert(qubit.get_logical_qubit() < _device.get_num_qubits());
#ifdef DEBUG
        if (i != SIZE_MAX) {
            assert(checker[i] == false);
            checker[i] = true;
        }
#endif
    }
    return operation_list;
}

/**
 * @brief Route gate by APSP
 *
 * @param gate
 * @param phase
 * @param qs
 * @param orient
 * @param swapped if the qubits of gate are swapped when added into Duostra
 * @return vector<Operation>
 */
std::vector<Router::Operation> Router::apsp_routing(GateType gate, size_t gate_id, dvlab::Phase phase, std::tuple<size_t, size_t> qs, bool orient, bool swapped) {
    std::vector<Operation> operation_list;
    size_t s0_id = get<0>(qs);
    size_t s1_id = get<1>(qs);
    size_t q0_id = s0_id;
    size_t q1_id = s1_id;

    while (!_device.get_physical_qubit(q0_id).is_adjacency(_device.get_physical_qubit(q1_id))) {
        std::tuple<size_t, size_t> q0_next_cost = _device.get_next_swap_cost(q0_id, s1_id);
        std::tuple<size_t, size_t> q1_next_cost = _device.get_next_swap_cost(q1_id, s0_id);

        size_t q0_next = get<0>(q0_next_cost);
        size_t q0_cost = get<1>(q0_next_cost);
        size_t q1_next = get<0>(q1_next_cost);
        size_t q1_cost = get<1>(q1_next_cost);

        if ((q0_cost < q1_cost) || ((q0_cost == q1_cost) && orient &&
                                    _device.get_physical_qubit(q0_id).get_logical_qubit() <
                                        _device.get_physical_qubit(q1_id).get_logical_qubit())) {
            Operation oper(GateType::swap, dvlab::Phase(0), std::make_tuple(q0_id, q0_next),
                           std::make_tuple(q0_cost, q0_cost + SWAP_DELAY));
            _device.apply_gate(oper);
            operation_list.emplace_back(std::move(oper));
            q0_id = q0_next;
        } else {
            Operation oper(GateType::swap, dvlab::Phase(0), std::make_tuple(q1_id, q1_next),
                           std::make_tuple(q0_cost, q0_cost + SWAP_DELAY));
            _device.apply_gate(oper);
            operation_list.emplace_back(std::move(oper));
            q1_id = q1_next;
        }
    }
    assert(_device.get_physical_qubit(q1_id).is_adjacency(_device.get_physical_qubit(q0_id)));

    size_t gate_cost = std::max(_device.get_physical_qubit(q0_id).get_occupied_time(),
                                _device.get_physical_qubit(q1_id).get_occupied_time());

    assert(gate == GateType::cx || gate == GateType::cz);
    Operation cx_gate(gate, phase, swapped ? std::make_tuple(q1_id, q0_id) : std::make_tuple(q0_id, q1_id),
                      std::make_tuple(gate_cost, gate_cost + DOUBLE_DELAY));
    _device.apply_gate(cx_gate);
    cx_gate.set_id(gate_id);
    operation_list.emplace_back(cx_gate);

    return operation_list;
}

/**
 * @brief Find adjacencies and put into priority queue until touched
 *
 * @param qubit
 * @param pq
 * @param source
 * @return tuple<bool, size_t>
 */
std::tuple<bool, size_t> Router::_touch_adjacency(PhysicalQubit& qubit, PriorityQueue& pq, bool source) {
    // mark all the adjacent qubits as seen and push them into the priority queue
    for (auto& i : qubit.get_adjacencies()) {
        PhysicalQubit& adj = _device.get_physical_qubit(i);
        // see if already in the queue
        if (adj.is_marked()) {
            // see if the taken one is from different path from the original qubit
            // if yes, means the two paths meet each other
            if (adj.is_taken()) {
                // touch target
                if (adj.get_source() != source) {
                    assert(adj.get_id() == i);
                    return std::make_tuple(true, adj.get_id());
                }
            }
            continue;
        }

        // push the node into the priority queue
        size_t cost = std::max(qubit.get_cost(), adj.get_occupied_time()) + SWAP_DELAY;
        adj.mark(source, qubit.get_id());

        pq.push(AStarNode(cost, adj.get_id(), source));
    }
    return std::make_tuple(false, SIZE_MAX);
}

/**
 * @brief Traceback the paths
 *
 * @param gt
 * @param ph
 * @param q0
 * @param q1
 * @param t0
 * @param t1
 * @param swapIds
 * @param swapped if the qubits of gate are swapped when added into Duostra
 * @return vector<Operation>
 */
std::vector<Router::Operation> Router::_traceback([[maybe_unused]] GateType gt, size_t gate_id, dvlab::Phase ph, PhysicalQubit& q0, PhysicalQubit& q1, PhysicalQubit& t0, PhysicalQubit& t1, bool swap_ids, bool swapped) {
    assert(t0.get_id() == t0.get_predecessor());
    assert(t1.get_id() == t1.get_predecessor());

    assert(q0.is_adjacency(q1));
    std::vector<Operation> operation_list;

    size_t operation_time = std::max(q0.get_cost(), q1.get_cost());

    assert(gt == GateType::cx || gt == GateType::cz);

    // NOTE - Order of qubits in CX matters
    std::tuple<size_t, size_t> qids = swap_ids ? std::make_tuple(q1.get_id(), q0.get_id()) : std::make_tuple(q0.get_id(), q1.get_id());
    if (swapped) {
        qids = std::make_tuple(get<1>(qids), get<0>(qids));
    }
    Operation cx_gate(gt, ph, qids, std::make_tuple(operation_time, operation_time + DOUBLE_DELAY));
    cx_gate.set_id(gate_id);
    operation_list.emplace_back(cx_gate);

    // traceback by tracing the parent iteratively
    size_t trace0 = q0.get_id();
    size_t trace1 = q1.get_id();
    // traceback by tracing the parent iteratively
    // trace 0
    while (trace0 != t0.get_id()) {
        PhysicalQubit& q_trace0 = _device.get_physical_qubit(trace0);
        size_t trace_pred0 = q_trace0.get_predecessor();

        size_t swap_time = q_trace0.get_swap_time();
        Operation swap_gate(GateType::swap, dvlab::Phase(0), std::make_tuple(trace0, trace_pred0),
                            std::make_tuple(swap_time, swap_time + SWAP_DELAY));
        operation_list.emplace_back(swap_gate);

        trace0 = trace_pred0;
    }
    while (trace1 != t1.get_id())  // trace 1
    {
        PhysicalQubit& q_trace1 = _device.get_physical_qubit(trace1);
        size_t trace_pred1 = q_trace1.get_predecessor();

        size_t swap_time = q_trace1.get_swap_time();
        Operation swap_gate(GateType::swap, dvlab::Phase(0), std::make_tuple(trace1, trace_pred1),
                            std::make_tuple(swap_time, swap_time + SWAP_DELAY));
        operation_list.emplace_back(swap_gate);

        trace1 = trace_pred1;
    }
    // REVIEW - Check time, now the start time
    sort(operation_list.begin(), operation_list.end(), [](Operation const& a, Operation const& b) -> bool {
        return a.get_time_begin() < b.get_time_begin();
    });

    for (size_t i = 0; i < operation_list.size(); ++i) {
        _device.apply_gate(operation_list[i]);
    }

    return operation_list;
}

/**
 * @brief Assign gate
 *
 * @param gate
 * @return vector<Operation>
 */
std::vector<Router::Operation> Router::assign_gate(Gate const& gate) {
    std::tuple<size_t, size_t> physical_qubits_ids = _get_physical_qubits(gate);

    if (!(gate.get_type() == GateType::cx || gate.get_type() == GateType::cz)) {
        assert(get<1>(physical_qubits_ids) == SIZE_MAX);
        Operation op = execute_single(gate.get_type(), gate.get_phase(), get<0>(physical_qubits_ids));
        op.set_id(gate.get_id());
        return std::vector<Operation>(1, op);
    }
    std::vector<Operation> operation_list =
        _duostra
            ? duostra_routing(gate.get_type(), gate.get_id(), gate.get_phase(), physical_qubits_ids, _orient, gate.is_swapped())
            : apsp_routing(gate.get_type(), gate.get_id(), gate.get_phase(), physical_qubits_ids, _orient, gate.is_swapped());
    std::vector<size_t> change_list = _device.mapping();
    std::vector<bool> checker(_logical_to_physical.size(), false);

    // i is the idx of device qubit
    for (size_t i = 0; i < change_list.size(); ++i) {
        auto logical_qubit_id = change_list[i];
        if (logical_qubit_id == SIZE_MAX) {
            continue;
        }
        _logical_to_physical[logical_qubit_id] = i;
    }
    return operation_list;
}

}  // namespace qsyn::duostra