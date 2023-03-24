/****************************************************************************
  FileName     [ router.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Router member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "router.h"

#include "util.h"

using namespace std;
extern size_t verbose;
Router::Router(Device&& device,
               const string& typ,
               const string& cost,
               bool orient) noexcept
    : greedy_type_(false),
      duostra_(false),
      orient_(orient),
      apsp_(false),
      device_(device),
      topo_to_dev_({}) {
    init(typ, cost);
}

void Router::init(const string& typ, const string& cost) {
    if (typ == "apsp") {
        apsp_ = true;
        duostra_ = false;
    } else if (typ == "duostra") {
        duostra_ = true;
    } else {
        cerr << "Error: " << typ << " is not a router type" << endl;
        abort();
    }

    if (cost == "end") {
        greedy_type_ = true;
        apsp_ = true;
    } else if (cost == "start") {
        greedy_type_ = false;
    } else {
        cerr << cost << " is not a cost type" << endl;
        abort();
    }

    if (apsp_) {
        device_.calculatePath();
    }

    size_t num_qubits = device_.getNQubit();
    topo_to_dev_.resize(num_qubits);
    for (size_t i = 0; i < num_qubits; ++i) {
        topo_to_dev_[device_.getPhysicalQubit(i).getLogicalQubit()] = i;
    }
}

Router::Router(const Router& other) noexcept
    : greedy_type_(other.greedy_type_),
      duostra_(other.duostra_),
      orient_(other.orient_),
      apsp_(other.apsp_),
      device_(other.device_),
      topo_to_dev_(other.topo_to_dev_) {}

Router::Router(Router&& other) noexcept
    : greedy_type_(other.greedy_type_),
      duostra_(other.duostra_),
      orient_(other.orient_),
      apsp_(other.apsp_),
      device_(move(other.device_)),
      topo_to_dev_(move(other.topo_to_dev_)) {}

size_t Router::get_gate_cost(const Gate& gate, bool min_max, size_t apsp_coef) {
    tuple<size_t, size_t> device_qubits_idx = get_device_qubits_idx(gate);

    if (!(gate.get_type() == GateType::CX || gate.get_type() == GateType::CZ)) {
        assert(get<1>(device_qubits_idx) == ERROR_CODE);
        return device_.getPhysicalQubit(get<0>(device_qubits_idx)).getOccupiedTime();
    }

    size_t q0_id = get<0>(device_qubits_idx);
    size_t q1_id = get<1>(device_qubits_idx);
    const PhyQubit& q0 = device_.getPhysicalQubit(q0_id);
    const PhyQubit& q1 = device_.getPhysicalQubit(q1_id);
    size_t apsp_cost = 0;
    if (apsp_) {
        apsp_cost = 1 * device_.getPath(q0_id, q1_id).size();  // NOTE - 1 for coefficient
        // assert(apsp_cost == device_.get_shortest_cost(q1_id, q0_id));
    }
    size_t avail = min_max ? max(q0.getOccupiedTime(), q1.getOccupiedTime()) : min(q0.getOccupiedTime(), q1.getOccupiedTime());
    return avail + apsp_cost / apsp_coef;
}

vector<Operation> Router::assign_gate(const Gate& gate) {
    tuple<size_t, size_t> device_qubits_idx = get_device_qubits_idx(gate);

    if (!(gate.get_type() == GateType::CX || gate.get_type() == GateType::CZ)) {
        assert(get<1>(device_qubits_idx) == ERROR_CODE);
        Operation op = execute_single(gate.get_type(), get<0>(device_qubits_idx));
        return vector<Operation>(1, op);
    }
    vector<Operation> op_list =
        duostra_
            ? duostra_routing(gate.get_type(), device_qubits_idx, orient_)
            : apsp_routing(gate.get_type(), device_qubits_idx, orient_);
    vector<size_t> change_list = device_.mapping();
    vector<bool> checker(topo_to_dev_.size(), false);

    // i is the idx of device qubit
    for (size_t i = 0; i < change_list.size(); ++i) {
        size_t topo_qubit_id = change_list[i];
        if (topo_qubit_id == ERROR_CODE) {
            continue;
        }
        // assert(checker[i] == false);
        // checker[i] = true;
        topo_to_dev_[topo_qubit_id] = i;
    }
    // for (size_t i = 0; i < checker.size(); ++i) {
    //     assert(checker[i]);
    // }
    // cout << "Gate: Q" << get<0>(gate.get_qubits()) << " Q"
    //           << get<1>(gate.get_qubits()) << "\n";
    // device_.print_device_state(cout);
    return op_list;
}

bool Router::is_executable(const Gate& gate) {
    if (!(gate.get_type() == GateType::CX || gate.get_type() == GateType::CZ)) {
        assert(get<1>(gate.get_qubits()) == ERROR_CODE);
        return true;
    }

    tuple<size_t, size_t> device_qubits_idx{get_device_qubits_idx(gate)};
    assert(get<1>(device_qubits_idx) != ERROR_CODE);
    const PhyQubit& q0 = device_.getPhysicalQubit(get<0>(device_qubits_idx));
    const PhyQubit& q1 = device_.getPhysicalQubit(get<1>(device_qubits_idx));
    return q0.isAdjacency(q1);
}

unique_ptr<Router> Router::clone() const {
    return make_unique<Router>(*this);
}

tuple<size_t, size_t> Router::get_device_qubits_idx(const Gate& gate) const {
    // NOTE - Only for 1- or 2-qubit gates
    size_t topo_idx_q0 =
        get<0>(gate.get_qubits());  // get operation qubit index of
                                    // gate in topology
    size_t device_idx_q0 =
        topo_to_dev_[topo_idx_q0];  // get device qubit index of the gate

    size_t device_idx_q1 = ERROR_CODE;

    if ((gate.get_type() == GateType::CX || gate.get_type() == GateType::CZ)) {
        // get operation qubit index of
        size_t topo_idx_q1 = get<1>(gate.get_qubits());
        // gate in topology
        assert(topo_idx_q1 != ERROR_CODE);
        device_idx_q1 = topo_to_dev_[topo_idx_q1];  // get device qubit
                                                    // index of the gate
    }
    return make_tuple(device_idx_q0, device_idx_q1);
}

Operation Router::execute_single(GateType gate, size_t q) {
    PhyQubit& qubit = device_.getPhysicalQubit(q);
    size_t starttime = qubit.getOccupiedTime();
    size_t endtime = starttime + SINGLE_DELAY;
    qubit.setOccupiedTime(endtime);
    qubit.reset();

    Operation op(gate, make_tuple(q, ERROR_CODE),
                 make_tuple(starttime, endtime));

    if (verbose > 3) cout << op << endl;

    return op;
}

std::vector<Operation> Router::duostra_routing(GateType gate, std::tuple<size_t, size_t> qs, bool orient) {
    assert(gate == GateType::CX || gate == GateType::CZ);
    size_t q0_idx = get<0>(qs);  // source 0
    size_t q1_idx = get<1>(qs);  // source 1
    // If two sources compete for the same qubit, the one with smaller occu goes first
    if (device_.getPhysicalQubit(q0_idx).getOccupiedTime() >
        device_.getPhysicalQubit(q1_idx).getOccupiedTime()) {
        swap(q0_idx, q1_idx);
    } else if (device_.getPhysicalQubit(q0_idx).getOccupiedTime() ==
               device_.getPhysicalQubit(q1_idx).getOccupiedTime()) {
        // orientation means qubit with smaller logical idx has a little priority
        if (orient && device_.getPhysicalQubit(q0_idx).getLogicalQubit() >
                          device_.getPhysicalQubit(q1_idx).getLogicalQubit()) {
            swap(q0_idx, q1_idx);
        }
    }

    PhyQubit& t0 = device_.getPhysicalQubit(q0_idx);  // target 0
    PhyQubit& t1 = device_.getPhysicalQubit(q1_idx);  // target 1
    // priority queue: pop out the node with the smallest cost from both the sources
    PriorityQueue pq;

    // init conditions for the sources
    t0.mark(false, t0.getId());
    t0.takeRoute(t0.getCost(), 0);
    t1.mark(true, t1.getId());
    t1.takeRoute(t1.getCost(), 0);
    tuple<bool, size_t> touch0 = touch_adj(t0, pq, false);
    bool is_adj = get<0>(touch0);
#ifdef DEBUG
    tuple<bool, size_t> touch1 = touch_adj(t1, pq, true);
    assert(is_adj == get<0>(touch1));
#else
    touch_adj(t1, pq, true);
#endif

    // the two paths from the two sources propagate until the two paths meet each other
    while (!is_adj) {
        // each iteration gets an element from the priority queue
        AStarNode next(pq.top());
        pq.pop();
        size_t q_next_idx = next.get_id();
        PhyQubit& q_next = device_.getPhysicalQubit(q_next_idx);
        // FIXME - swtch to source
        assert(q_next.getSource() == next.get_swtch());

        // mark the element as visited and check its neighbors
        size_t cost = next.get_cost();
        assert(cost >= SWAP_DELAY);
        size_t op_time = cost - SWAP_DELAY;
        q_next.takeRoute(cost, op_time);
        tuple<bool, size_t> touch = touch_adj(q_next, pq, next.get_swtch());
        is_adj = get<0>(touch);
        if (is_adj) {
            if (next.get_swtch())  // 0 get true means touch 1's set
            {
                q0_idx = get<1>(touch);
                q1_idx = q_next_idx;
            } else {
                q0_idx = q_next_idx;
                q1_idx = get<1>(touch);
            }
        }
    }
    vector<Operation> ops =
        traceback(gate, device_.getPhysicalQubit(q0_idx), device_.getPhysicalQubit(q1_idx), t0, t1);

    if (verbose > 3) {
        for (size_t i = 0; i < ops.size(); ++i) {
            cout << ops[i] << endl;
        }
    }

#ifdef DEBUG
    vector<bool> checker(device_.getNQubit(), false);
#endif
    for (size_t i = 0; i < device_.getNQubit(); ++i) {
        PhyQubit& qubit = device_.getPhysicalQubit(i);
        qubit.reset();
        assert(qubit.getLogicalQubit() < device_.getNQubit());
#ifdef DEBUG
        if (i != ERROR_CODE) {
            assert(checker[i] == false);
            checker[i] = true;
        }
#endif
    }
    return ops;
}

vector<Operation> Router::apsp_routing(GateType gate, std::tuple<size_t, size_t> qs, bool orient) {
    vector<Operation> ops;
    // TODO -
    return ops;
}

tuple<bool, size_t> Router::touch_adj(PhyQubit& qubit, PriorityQueue& pq, bool swtch) {
    // mark all the adjacent qubits as seen and push them into the priority queue
    for (auto& i : qubit.getAdjacencies()) {
        PhyQubit& adj = device_.getPhysicalQubit(i);
        // see if already in the queue
        if (adj.isMarked()) {
            // see if the taken one is from different path from the original qubit
            // if yes, means the two paths meet each other
            if (adj.isTaken()) {
                // touch target
                if (adj.getSource() != swtch) {
                    assert(adj.getId() == i);
                    return make_tuple(true, adj.getId());
                }
            }
            continue;
        }

        // push the node into the priority queue
        size_t cost = max(qubit.getCost(), adj.getOccupiedTime()) + SWAP_DELAY;
        adj.mark(swtch, qubit.getId());

        pq.push(AStarNode(cost, adj.getId(), swtch));
    }
    return make_tuple(false, ERROR_CODE);
}

vector<Operation> Router::traceback([[maybe_unused]] GateType gt, PhyQubit& q0, PhyQubit& q1, PhyQubit& t0, PhyQubit& t1) {
    assert(t0.getId() == t0.getPred());
    assert(t1.getId() == t1.getPred());

    assert(q0.isAdjacency(q1));
    vector<Operation> ops;

    size_t op_time = max(q0.getCost(), q1.getCost());
    // FIXME - CZ issue (need decompose?)
    assert(gt == GateType::CX);

    Operation CX_gate(GateType::CX, make_tuple(q0.getId(), q1.getId()),
                      make_tuple(op_time, op_time + DOUBLE_DELAY));
    ops.push_back(CX_gate);

    // traceback by tracing the parent iteratively
    size_t trace_0 = q0.getId();
    size_t trace_1 = q1.getId();

    // traceback by tracing the parent iteratively
    // trace 0
    while (trace_0 != t0.getId()) {
        PhyQubit& q_trace_0 = device_.getPhysicalQubit(trace_0);
        size_t trace_pred_0 = q_trace_0.getPred();

        size_t swap_time = q_trace_0.getSwapTime();
        Operation swap_gate(GateType::SWAP, make_tuple(trace_0, trace_pred_0),
                            make_tuple(swap_time, swap_time + SWAP_DELAY));
        ops.push_back(swap_gate);

        trace_0 = trace_pred_0;
    }
    while (trace_1 != t1.getId())  // trace 1
    {
        PhyQubit& q_trace_1 = device_.getPhysicalQubit(trace_1);
        size_t trace_pred_1 = q_trace_1.getPred();

        size_t swap_time = q_trace_1.getSwapTime();
        Operation swap_gate(GateType::SWAP, make_tuple(trace_1, trace_pred_1),
                            make_tuple(swap_time, swap_time + SWAP_DELAY));
        ops.push_back(swap_gate);

        trace_1 = trace_pred_1;
    }

    // REVIEW - Check time, now the start time
    sort(ops.begin(), ops.end(), [](const Operation& a, const Operation& b) -> bool {
        return a.get_op_time() < b.get_op_time();
    });

    for (size_t i = 0; i < ops.size(); ++i) {
        device_.applyGate(ops[i]);
    }

    return ops;
}