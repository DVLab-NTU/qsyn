/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Router member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./router.hpp"

#include <spdlog/spdlog.h>

#include <gsl/narrow>
#include <gsl/util>

#include "device/device.hpp"
#include "duostra/duostra_def.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir_gate.hpp"
#include "qsyn/qsyn_type.hpp"

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
AStarNode::AStarNode(size_t cost, QubitIdType id, bool source)
    : _estimated_cost(cost), _id(id), _source(source) {}

// SECTION - Class Router Member Functions

/**
 * @brief Construct a new Router:: Router object
 *
 * @param device
 * @param cost
 * @param orient
 */
Router::Router(Device device, Router::CostStrategyType cost_strategy, MinMaxOptionType tie_breaking_strategy)
    : _tie_breaking_strategy(tie_breaking_strategy),
      _device(std::move(device)),
      _logical_to_physical({}),
      _apsp(DuostraConfig::ROUTER_TYPE == RouterType::shortest_path || cost_strategy == CostStrategyType::end),
      _duostra(DuostraConfig::ROUTER_TYPE == RouterType::duostra) {
    _initialize();
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
void Router::_initialize() {
    if (_apsp) {
        _device.calculate_path();
    }

    auto const num_qubits = _device.get_num_qubits();
    _logical_to_physical.resize(num_qubits);
    for (size_t i = 0; i < num_qubits; ++i) {
        auto const& qubit = _device.get_physical_qubit(gsl::narrow<QubitIdType>(i)).get_logical_qubit();
        assert(qubit.has_value());
        _logical_to_physical[qubit.value()] = gsl::narrow<QubitIdType>(i);
    }
}

/**
 * @brief Get physical qubits of gate
 *
 * @param gate
 * @return tuple<size_t, size_t> Note: (_, SIZE_MAX) if single-qubit gate
 */
std::tuple<QubitIdType, QubitIdType> Router::_get_physical_qubits(qcir::QCirGate const& gate) const {
    // NOTE - Only for 1- or 2-qubit gates
    auto logical_id0  = gate.get_qubit(0);                  // get logical qubit index of gate in topology
    auto physical_id0 = _logical_to_physical[logical_id0];  // get physical qubit index of the gate
    auto physical_id1 = max_qubit_id;

    if (gate.get_num_qubits() == 2) {
        auto logical_id1 = gate.get_qubit(1);                  // get logical qubit index of gate in topology
        physical_id1     = _logical_to_physical[logical_id1];  // get physical qubit index of the gate
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
size_t Router::get_gate_cost(qcir::QCirGate const& gate, MinMaxOptionType min_max, size_t apsp_coeff) {
    auto const physical_qubits_ids = _get_physical_qubits(gate);

    if (gate.get_num_qubits() == 1) {
        assert(get<1>(physical_qubits_ids) == max_qubit_id);
        return _device.get_physical_qubit(get<0>(physical_qubits_ids)).get_occupied_time();
    }

    auto const q0_id     = get<0>(physical_qubits_ids);
    auto const q1_id     = get<1>(physical_qubits_ids);
    auto const& q0       = _device.get_physical_qubit(q0_id);
    auto const& q1       = _device.get_physical_qubit(q1_id);
    auto const apsp_cost = _apsp ? _device.get_path(q0_id, q1_id).size() : 0;

    auto const avail = min_max == MinMaxOptionType::max ? std::max(q0.get_occupied_time(), q1.get_occupied_time()) : std::min(q0.get_occupied_time(), q1.get_occupied_time());
    return avail + apsp_cost / apsp_coeff;
}

/**
 * @brief Is gate executable or not
 *
 * @param gate
 * @return true
 * @return false
 */
bool Router::is_executable(qcir::QCirGate const& gate) {
    if (gate.get_num_qubits() == 1) return true;

    auto physical_qubits_ids{_get_physical_qubits(gate)};
    assert(get<1>(physical_qubits_ids) != max_qubit_id);
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
qcir::QCirGate Router::execute_single(qcir::QCirGate const& gate, GateIdToTime& gate_id_to_time, QubitIdType q) {
    auto& qubit           = _device.get_physical_qubit(q);
    auto const start_time = qubit.get_occupied_time();
    auto const end_time   = start_time + gate.get_delay();
    qubit.set_occupied_time(end_time);
    qubit.reset();
    auto op = qcir::QCirGate{gate_id_to_time.size(), gate.get_operation(), QubitIdList{q, max_qubit_id}};
    gate_id_to_time.emplace(op.get_id(), std::make_pair(start_time, end_time));
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
std::vector<qcir::QCirGate> Router::duostra_routing(qcir::QCirGate const& gate, GateIdToTime& gate_id_to_time, std::tuple<QubitIdType, QubitIdType> qubit_pair, MinMaxOptionType tie_breaking_strategy) {
    assert(gate.get_num_qubits() == 2);
    auto q0_id    = get<0>(qubit_pair);  // source 0
    auto q1_id    = get<1>(qubit_pair);  // source 1
    bool swap_ids = false;
    // If two sources compete for the same qubit, the one with smaller occupied time goes first
    if (_device.get_physical_qubit(q0_id).get_occupied_time() >
        _device.get_physical_qubit(q1_id).get_occupied_time()) {
        std::swap(q0_id, q1_id);
        swap_ids = true;
    } else if (_device.get_physical_qubit(q0_id).get_occupied_time() ==
               _device.get_physical_qubit(q1_id).get_occupied_time()) {
        // orientation means qubit with smaller logical idx has a little priority
        if (tie_breaking_strategy == MinMaxOptionType::min && _device.get_physical_qubit(q0_id).get_logical_qubit() >
                                                                  _device.get_physical_qubit(q1_id).get_logical_qubit()) {
            std::swap(q0_id, q1_id);
            swap_ids = true;
        }
    }

    PhysicalQubit& t0 = _device.get_physical_qubit(q0_id);  // target 0
    PhysicalQubit& t1 = _device.get_physical_qubit(q1_id);  // target 1
    // priority queue: pop out the node with the smallest cost from both the sources
    PriorityQueue priority_queue;

    // init conditions for the sources
    t0.mark(false, t0.get_id());
    t0.take_route(t0.get_cost(), 0);
    t1.mark(true, t1.get_id());
    t1.take_route(t1.get_cost(), 0);
    auto const touch0 = _touch_adjacency(t0, priority_queue, false);
    auto is_adjacent  = get<0>(touch0);
    _touch_adjacency(t1, priority_queue, true);

    // the two paths from the two sources propagate until the two paths meet each other
    while (!is_adjacent) {
        // each iteration gets an element from the priority queue
        auto const next{priority_queue.top()};
        priority_queue.pop();
        auto const q_next_id = next.get_id();
        auto& q_next         = _device.get_physical_qubit(q_next_id);
        // FIXME - swtch to source
        assert(q_next.get_source() == next.get_source());

        // mark the element as visited and check its neighbors
        auto const cost = next.get_cost();
        assert(cost >= SWAP_DELAY);
        auto const operation_time = cost - SWAP_DELAY;
        q_next.take_route(cost, operation_time);
        auto const touch = _touch_adjacency(q_next, priority_queue, next.get_source());
        is_adjacent      = get<0>(touch);
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
    auto operation_list =
        _traceback(gate, gate_id_to_time, _device.get_physical_qubit(q0_id), _device.get_physical_qubit(q1_id), t0, t1, swap_ids);

    for (size_t i = 0; i < _device.get_num_qubits(); ++i) {
        auto& qubit = _device.get_physical_qubit(gsl::narrow<QubitIdType>(i));
        qubit.reset();
        assert(qubit.get_logical_qubit() < _device.get_num_qubits());
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
std::vector<qcir::QCirGate> Router::apsp_routing(qcir::QCirGate const& gate, GateIdToTime& gate_id_to_time, std::tuple<QubitIdType, QubitIdType> qs, MinMaxOptionType tie_breaking_strategy) {
    std::vector<qcir::QCirGate> operation_list;
    auto s0_id = get<0>(qs);
    auto s1_id = get<1>(qs);
    auto q0_id = s0_id;
    auto q1_id = s1_id;

    while (!_device.get_physical_qubit(q0_id).is_adjacency(_device.get_physical_qubit(q1_id))) {
        auto q0_next_cost = _device.get_next_swap_cost(q0_id, s1_id);
        auto q1_next_cost = _device.get_next_swap_cost(q1_id, s0_id);

        auto q0_next = get<0>(q0_next_cost);
        auto q0_cost = get<1>(q0_next_cost);
        auto q1_next = get<0>(q1_next_cost);
        auto q1_cost = get<1>(q1_next_cost);

        if ((q0_cost < q1_cost) || ((q0_cost == q1_cost) && (tie_breaking_strategy == MinMaxOptionType::min) &&
                                    _device.get_physical_qubit(q0_id).get_logical_qubit() <
                                        _device.get_physical_qubit(q1_id).get_logical_qubit())) {
            auto op = qcir::QCirGate(gate_id_to_time.size(), SwapGate{}, QubitIdList{q0_id, q0_next});
            gate_id_to_time.emplace(op.get_id(), std::make_pair(q0_cost, q0_cost + SWAP_DELAY));
            operation_list.emplace_back(std::move(op));
            _device.apply_gate(op, q0_cost);
            q0_id = q0_next;
        } else {
            auto op = qcir::QCirGate(gate_id_to_time.size(), SwapGate{}, QubitIdList{q1_id, q1_next});
            gate_id_to_time.emplace(op.get_id(), std::make_pair(q0_cost, q0_cost + SWAP_DELAY));
            _device.apply_gate(op, q0_cost);
            operation_list.emplace_back(std::move(op));
            q1_id = q1_next;
        }
    }
    assert(_device.get_physical_qubit(q1_id).is_adjacency(_device.get_physical_qubit(q0_id)));

    auto const gate_cost = std::max(_device.get_physical_qubit(q0_id).get_occupied_time(),
                                    _device.get_physical_qubit(q1_id).get_occupied_time());

    auto cx_gate = qcir::QCirGate(gate_id_to_time.size(), gate.get_operation(), QubitIdList{q0_id, q1_id});
    gate_id_to_time.emplace(cx_gate.get_id(), std::make_pair(gate_cost, gate_cost + gate.get_delay()));
    _device.apply_gate(cx_gate, gate_cost);
    operation_list.emplace_back(std::move(cx_gate));

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
std::tuple<bool, QubitIdType> Router::_touch_adjacency(PhysicalQubit& qubit, PriorityQueue& pq, bool source) {
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
        auto const cost = std::max(qubit.get_cost(), adj.get_occupied_time()) + SWAP_DELAY;
        adj.mark(source, qubit.get_id());

        pq.push(AStarNode(cost, adj.get_id(), source));
    }
    return std::make_tuple(false, max_qubit_id);
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
std::vector<qcir::QCirGate> Router::_traceback(qcir::QCirGate const& gate, GateIdToTime& gate_id_to_time, PhysicalQubit& q0, PhysicalQubit& q1, PhysicalQubit& t0, PhysicalQubit& t1, bool swap_ids) {
    assert(t0.get_id() == t0.get_predecessor());
    assert(t1.get_id() == t1.get_predecessor());

    assert(q0.is_adjacency(q1));
    std::vector<qcir::QCirGate> operation_list;

    auto const operation_time = std::max(q0.get_cost(), q1.get_cost());

    assert(gate.get_num_qubits() == 2);

    // NOTE - Order of qubits in CX matters
    auto const qids = swap_ids ? QubitIdList{q1.get_id(), q0.get_id()} : QubitIdList{q0.get_id(), q1.get_id()};

    auto cx_gate = qcir::QCirGate(gate_id_to_time.size(), gate.get_operation(), qids);
    gate_id_to_time.emplace(cx_gate.get_id(), std::make_pair(operation_time, operation_time + gate.get_delay()));
    operation_list.emplace_back(std::move(cx_gate));

    // traceback by tracing the parent iteratively
    auto trace0 = q0.get_id();
    auto trace1 = q1.get_id();
    // traceback by tracing the parent iteratively
    // trace 0
    while (trace0 != t0.get_id()) {
        auto const& q_trace0   = _device.get_physical_qubit(trace0);
        auto const trace_pred0 = q_trace0.get_predecessor();

        auto const swap_time = q_trace0.get_swap_time();
        auto op              = qcir::QCirGate(gate_id_to_time.size(), SwapGate{}, QubitIdList{trace0, trace_pred0});
        gate_id_to_time.emplace(op.get_id(), std::make_pair(swap_time, swap_time + SWAP_DELAY));
        operation_list.emplace_back(std::move(op));
        trace0 = trace_pred0;
    }
    while (trace1 != t1.get_id())  // trace 1
    {
        auto& q_trace1         = _device.get_physical_qubit(trace1);
        auto const trace_pred1 = q_trace1.get_predecessor();

        auto const swap_time = q_trace1.get_swap_time();
        auto op              = qcir::QCirGate(gate_id_to_time.size(), SwapGate{}, QubitIdList{trace1, trace_pred1});
        gate_id_to_time.emplace(op.get_id(), std::make_pair(swap_time, swap_time + SWAP_DELAY));
        operation_list.emplace_back(std::move(op));
        trace1 = trace_pred1;
    }
    // REVIEW - Check time, now the start time
    std::ranges::sort(operation_list, [&gate_id_to_time](qcir::QCirGate const& a, qcir::QCirGate const& b) -> bool {
        return gate_id_to_time.at(a.get_id()).first < gate_id_to_time.at(b.get_id()).first;
    });

    for (size_t i = 0; i < operation_list.size(); ++i) {
        _device.apply_gate(operation_list[i], gate_id_to_time.at(operation_list[i].get_id()).first);
    }

    return operation_list;
}

/**
 * @brief Assign gate
 *
 * @param gate
 * @return vector<Operation>
 */
std::vector<qcir::QCirGate> Router::assign_gate(qcir::QCirGate const& gate, GateIdToTime& gate_id_to_time) {
    auto physical_qubits_ids = _get_physical_qubits(gate);

    if (gate.get_num_qubits() == 1) {
        assert(get<1>(physical_qubits_ids) == max_qubit_id);
        auto op = execute_single(gate, gate_id_to_time, get<0>(physical_qubits_ids));
        return std::vector(1, op);
    }
    auto operation_list =
        _duostra
            ? duostra_routing(gate, gate_id_to_time, physical_qubits_ids, _tie_breaking_strategy)
            : apsp_routing(gate, gate_id_to_time, physical_qubits_ids, _tie_breaking_strategy);
    auto const change_list = _device.mapping();

    // i is the idx of device qubit
    for (size_t i = 0; i < change_list.size(); ++i) {
        auto logical_qubit_id = change_list[i];
        if (logical_qubit_id == std::nullopt) {
            continue;
        }
        _logical_to_physical[logical_qubit_id.value()] = gsl::narrow<QubitIdType>(i);
    }
    return operation_list;
}

}  // namespace qsyn::duostra
