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

// SECTION - Class AStarNode Member Functions

/**
 * @brief Construct a new AStarNode::AStarNode object
 *
 * @param cost
 * @param id
 * @param source
 */
AStarNode::AStarNode(size_t cost, size_t id, bool source)
    : _estimatedCost(cost), _id(id), _source(source) {}

/**
 * @brief Construct a new AStarNode::AStarNode object
 *
 * @param other
 */
AStarNode::AStarNode(const AStarNode& other)
    : _estimatedCost(other._estimatedCost), _id(other._id), _source(other._source) {}

/**
 * @brief Assignment operator overloading for AStarNode
 *
 * @param other
 * @return AStarNode&
 */
AStarNode& AStarNode::operator=(const AStarNode& other) {
    _estimatedCost = other._estimatedCost;
    _id = other._id;
    _source = other._source;
    return *this;
}

// SECTION - Class Router Member Functions

/**
 * @brief Construct a new Router:: Router object
 *
 * @param device
 * @param cost
 * @param orient
 */
Router::Router(Device&& device, const string& cost, bool orient) noexcept
    : _greedyType(false),
      _duostra(false),
      _orient(orient),
      _APSP(false),
      _device(device),
      _logical2Physical({}) {
    init(cost);
}

/**
 * @brief Construct a new Router:: Router object
 *
 * @param other
 */
Router::Router(const Router& other) noexcept
    : _greedyType(other._greedyType),
      _duostra(other._duostra),
      _orient(other._orient),
      _APSP(other._APSP),
      _device(other._device),
      _logical2Physical(other._logical2Physical) {}

/**
 * @brief Construct a new Router:: Router object
 *
 * @param other
 */
Router::Router(Router&& other) noexcept
    : _greedyType(other._greedyType),
      _duostra(other._duostra),
      _orient(other._orient),
      _APSP(other._APSP),
      _device(move(other._device)),
      _logical2Physical(move(other._logical2Physical)) {}

/**
 * @brief Clone router
 *
 * @return unique_ptr<Router>
 */
unique_ptr<Router> Router::clone() const {
    return make_unique<Router>(*this);
}

/**
 * @brief Initialize router
 *
 * @param type
 * @param cost
 */
void Router::init(const string& cost) {
    // DUOSTRA_ROUTER 0:apsp 1:duostra
    if (DUOSTRA_ROUTER == 0) {
        _APSP = true;
        _duostra = false;
    } else if (DUOSTRA_ROUTER == 1) {
        _duostra = true;
    } else {
        cerr << "Error: router type not found" << endl;
        abort();
    }

    if (cost == "end") {
        _greedyType = true;
        _APSP = true;
    } else if (cost == "start") {
        _greedyType = false;
    } else {
        cerr << cost << " is not a cost type" << endl;
        abort();
    }

    if (_APSP) {
        _device.calculatePath();
    }

    size_t numQubits = _device.getNQubit();
    _logical2Physical.resize(numQubits);
    for (size_t i = 0; i < numQubits; ++i) {
        _logical2Physical[_device.getPhysicalQubit(i).getLogicalQubit()] = i;
    }
}

/**
 * @brief Get physical qubits of gate
 *
 * @param gate
 * @return tuple<size_t, size_t> Note: (_, ERROR_CODE) if single-qubit gate
 */
tuple<size_t, size_t> Router::getPhysicalQubits(const Gate& gate) const {
    // NOTE - Only for 1- or 2-qubit gates
    size_t logicalId0 = get<0>(gate.getQubits());        // get logical qubit index of gate in topology
    size_t physicalId0 = _logical2Physical[logicalId0];  // get physical qubit index of the gate
    size_t physicalId1 = ERROR_CODE;

    if ((gate.getType() == GateType::CX || gate.getType() == GateType::CZ)) {
        size_t logicalId1 = get<1>(gate.getQubits());  // get logical qubit index of gate in topology
        assert(logicalId1 != ERROR_CODE);
        physicalId1 = _logical2Physical[logicalId1];  // get physical qubit index of the gate
    }
    return make_tuple(physicalId0, physicalId1);
}

/**
 * @brief Get cost of gate
 *
 * @param gate
 * @param minMax
 * @param APSPCoeff
 * @return size_t
 */
size_t Router::getGateCost(const Gate& gate, bool minMax, size_t APSPCoeff) {
    tuple<size_t, size_t> physicalQubitsIds = getPhysicalQubits(gate);

    if (!(gate.getType() == GateType::CX || gate.getType() == GateType::CZ)) {
        assert(get<1>(physicalQubitsIds) == ERROR_CODE);
        return _device.getPhysicalQubit(get<0>(physicalQubitsIds)).getOccupiedTime();
    }

    size_t q0Id = get<0>(physicalQubitsIds);
    size_t q1Id = get<1>(physicalQubitsIds);
    const PhysicalQubit& q0 = _device.getPhysicalQubit(q0Id);
    const PhysicalQubit& q1 = _device.getPhysicalQubit(q1Id);
    size_t apspCost = 0;
    if (_APSP)
        apspCost = 1 * _device.getPath(q0Id, q1Id).size();  // NOTE - 1 for coefficient

    size_t avail = minMax ? max(q0.getOccupiedTime(), q1.getOccupiedTime()) : min(q0.getOccupiedTime(), q1.getOccupiedTime());
    return avail + apspCost / APSPCoeff;
}

/**
 * @brief Is gate executable or not
 *
 * @param gate
 * @return true
 * @return false
 */
bool Router::isExecutable(const Gate& gate) {
    if (!(gate.getType() == GateType::CX || gate.getType() == GateType::CZ)) {
        assert(get<1>(gate.getQubits()) == ERROR_CODE);
        return true;
    }

    tuple<size_t, size_t> physicalQubitsIds{getPhysicalQubits(gate)};
    assert(get<1>(physicalQubitsIds) != ERROR_CODE);
    const PhysicalQubit& q0 = _device.getPhysicalQubit(get<0>(physicalQubitsIds));
    const PhysicalQubit& q1 = _device.getPhysicalQubit(get<1>(physicalQubitsIds));
    return q0.isAdjacency(q1);
}

/**
 * @brief
 *
 * @param gate
 * @param phase
 * @param q
 * @return Operation
 */
Operation Router::executeSingle(GateType gate, Phase phase, size_t q) {
    PhysicalQubit& qubit = _device.getPhysicalQubit(q);
    size_t startTime = qubit.getOccupiedTime();
    size_t endTime = startTime + SINGLE_DELAY;
    qubit.setOccupiedTime(endTime);
    qubit.reset();
    Operation op(gate, phase, make_tuple(q, ERROR_CODE), make_tuple(startTime, endTime));
    if (verbose > 3) cout << op << endl;
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
vector<Operation> Router::duostraRouting(GateType gate, size_t gateId, Phase phase, tuple<size_t, size_t> qubitPair, bool orient, bool swapped) {
    assert(gate == GateType::CX || gate == GateType::CZ);
    size_t q0Id = get<0>(qubitPair);  // source 0
    size_t q1Id = get<1>(qubitPair);  // source 1
    bool swapIds = false;
    // If two sources compete for the same qubit, the one with smaller occupied time goes first
    if (_device.getPhysicalQubit(q0Id).getOccupiedTime() >
        _device.getPhysicalQubit(q1Id).getOccupiedTime()) {
        swap(q0Id, q1Id);
        swapIds = true;
    } else if (_device.getPhysicalQubit(q0Id).getOccupiedTime() ==
               _device.getPhysicalQubit(q1Id).getOccupiedTime()) {
        // orientation means qubit with smaller logical idx has a little priority
        if (orient && _device.getPhysicalQubit(q0Id).getLogicalQubit() >
                          _device.getPhysicalQubit(q1Id).getLogicalQubit()) {
            swap(q0Id, q1Id);
            swapIds = true;
        }
    }

    PhysicalQubit& t0 = _device.getPhysicalQubit(q0Id);  // target 0
    PhysicalQubit& t1 = _device.getPhysicalQubit(q1Id);  // target 1
    // priority queue: pop out the node with the smallest cost from both the sources
    PriorityQueue pq;

    // init conditions for the sources
    t0.mark(false, t0.getId());
    t0.takeRoute(t0.getCost(), 0);
    t1.mark(true, t1.getId());
    t1.takeRoute(t1.getCost(), 0);
    tuple<bool, size_t> touch0 = touchAdjacency(t0, pq, false);
    bool isAdjacent = get<0>(touch0);
#ifdef DEBUG
    tuple<bool, size_t> touch1 = touchAdjacency(t1, pq, true);
    assert(isAdjacent == get<0>(touch1));
#else
    touchAdjacency(t1, pq, true);
#endif

    // the two paths from the two sources propagate until the two paths meet each other
    while (!isAdjacent) {
        // each iteration gets an element from the priority queue
        AStarNode next(pq.top());
        pq.pop();
        size_t qNextId = next.getId();
        PhysicalQubit& qNext = _device.getPhysicalQubit(qNextId);
        // FIXME - swtch to source
        assert(qNext.getSource() == next.getSource());

        // mark the element as visited and check its neighbors
        size_t cost = next.getCost();
        assert(cost >= SWAP_DELAY);
        size_t operationTime = cost - SWAP_DELAY;
        qNext.takeRoute(cost, operationTime);
        tuple<bool, size_t> touch = touchAdjacency(qNext, pq, next.getSource());
        isAdjacent = get<0>(touch);
        if (isAdjacent) {
            if (next.getSource())  // 0 get true means touch 1's set
            {
                q0Id = get<1>(touch);
                q1Id = qNextId;
            } else {
                q0Id = qNextId;
                q1Id = get<1>(touch);
            }
        }
    }
    vector<Operation> operationList =
        traceback(gate, gateId, phase, _device.getPhysicalQubit(q0Id), _device.getPhysicalQubit(q1Id), t0, t1, swapIds, swapped);

    if (verbose > 3) {
        for (size_t i = 0; i < operationList.size(); ++i) {
            cout << operationList[i] << endl;
        }
    }

#ifdef DEBUG
    vector<bool> checker(_device.getNQubit(), false);
#endif
    for (size_t i = 0; i < _device.getNQubit(); ++i) {
        PhysicalQubit& qubit = _device.getPhysicalQubit(i);
        qubit.reset();
        assert(qubit.getLogicalQubit() < _device.getNQubit());
#ifdef DEBUG
        if (i != ERROR_CODE) {
            assert(checker[i] == false);
            checker[i] = true;
        }
#endif
    }
    return operationList;
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
vector<Operation> Router::apspRouting(GateType gate, size_t gateId, Phase phase, tuple<size_t, size_t> qs, bool orient, bool swapped) {
    vector<Operation> operationList;
    size_t s0Id = get<0>(qs);
    size_t s1Id = get<1>(qs);
    size_t q0Id = s0Id;
    size_t q1Id = s1Id;

    while (!_device.getPhysicalQubit(q0Id).isAdjacency(_device.getPhysicalQubit(q1Id))) {
        tuple<size_t, size_t> q0NextCost = _device.getNextSwapCost(q0Id, s1Id);
        tuple<size_t, size_t> q1NextCost = _device.getNextSwapCost(q1Id, s0Id);

        size_t q0Next = get<0>(q0NextCost);
        size_t q0Cost = get<1>(q0NextCost);
        size_t q1Next = get<0>(q1NextCost);
        size_t q1Cost = get<1>(q1NextCost);

        if ((q0Cost < q1Cost) || ((q0Cost == q1Cost) && orient &&
                                  _device.getPhysicalQubit(q0Id).getLogicalQubit() <
                                      _device.getPhysicalQubit(q1Id).getLogicalQubit())) {
            Operation oper(GateType::SWAP, Phase(0), make_tuple(q0Id, q0Next),
                           make_tuple(q0Cost, q0Cost + SWAP_DELAY));
            _device.applyGate(oper);
            operationList.push_back(move(oper));
            q0Id = q0Next;
        } else {
            Operation oper(GateType::SWAP, Phase(0), make_tuple(q1Id, q1Next),
                           make_tuple(q0Cost, q0Cost + SWAP_DELAY));
            _device.applyGate(oper);
            operationList.push_back(move(oper));
            q1Id = q1Next;
        }
    }
    assert(_device.getPhysicalQubit(q1Id).isAdjacency(_device.getPhysicalQubit(q0Id)));

    size_t gateCost = max(_device.getPhysicalQubit(q0Id).getOccupiedTime(),
                          _device.getPhysicalQubit(q1Id).getOccupiedTime());
    // REVIEW - CZ Issue
    assert(gate == GateType::CX || gate == GateType::CZ);
    Operation CXGate(gate, phase, swapped ? make_tuple(q1Id, q0Id) : make_tuple(q0Id, q1Id),
                     make_tuple(gateCost, gateCost + DOUBLE_DELAY));
    _device.applyGate(CXGate);
    CXGate.setId(gateId);
    operationList.push_back(CXGate);

    return operationList;
}

/**
 * @brief Find adjacencies and put into priority queue until touched
 *
 * @param qubit
 * @param pq
 * @param source
 * @return tuple<bool, size_t>
 */
tuple<bool, size_t> Router::touchAdjacency(PhysicalQubit& qubit, PriorityQueue& pq, bool source) {
    // mark all the adjacent qubits as seen and push them into the priority queue
    for (auto& i : qubit.getAdjacencies()) {
        PhysicalQubit& adj = _device.getPhysicalQubit(i);
        // see if already in the queue
        if (adj.isMarked()) {
            // see if the taken one is from different path from the original qubit
            // if yes, means the two paths meet each other
            if (adj.isTaken()) {
                // touch target
                if (adj.getSource() != source) {
                    assert(adj.getId() == i);
                    return make_tuple(true, adj.getId());
                }
            }
            continue;
        }

        // push the node into the priority queue
        size_t cost = max(qubit.getCost(), adj.getOccupiedTime()) + SWAP_DELAY;
        adj.mark(source, qubit.getId());

        pq.push(AStarNode(cost, adj.getId(), source));
    }
    return make_tuple(false, ERROR_CODE);
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
vector<Operation> Router::traceback([[maybe_unused]] GateType gt, size_t gateId, Phase ph, PhysicalQubit& q0, PhysicalQubit& q1, PhysicalQubit& t0, PhysicalQubit& t1, bool swapIds, bool swapped) {
    assert(t0.getId() == t0.getPred());
    assert(t1.getId() == t1.getPred());

    assert(q0.isAdjacency(q1));
    vector<Operation> operationList;

    size_t operationTime = max(q0.getCost(), q1.getCost());
    // REVIEW - CZ issue (need decompose?)
    assert(gt == GateType::CX || gt == GateType::CZ);
    // REVIEW - Order of qubits in CX matters
    tuple<size_t, size_t> qids = swapIds ? make_tuple(q1.getId(), q0.getId()) : make_tuple(q0.getId(), q1.getId());
    if (swapped) {
        qids = make_tuple(get<1>(qids), get<0>(qids));
    }
    Operation CXGate(gt, ph, qids, make_tuple(operationTime, operationTime + DOUBLE_DELAY));
    CXGate.setId(gateId);
    operationList.push_back(CXGate);

    // traceback by tracing the parent iteratively
    size_t trace0 = q0.getId();
    size_t trace1 = q1.getId();
    // traceback by tracing the parent iteratively
    // trace 0
    while (trace0 != t0.getId()) {
        PhysicalQubit& qTrace0 = _device.getPhysicalQubit(trace0);
        size_t tracePred0 = qTrace0.getPred();

        size_t swapTime = qTrace0.getSwapTime();
        Operation swapGate(GateType::SWAP, Phase(0), make_tuple(trace0, tracePred0),
                           make_tuple(swapTime, swapTime + SWAP_DELAY));
        operationList.push_back(swapGate);

        trace0 = tracePred0;
    }
    while (trace1 != t1.getId())  // trace 1
    {
        PhysicalQubit& qTrace1 = _device.getPhysicalQubit(trace1);
        size_t tracePred1 = qTrace1.getPred();

        size_t swapTime = qTrace1.getSwapTime();
        Operation swapGate(GateType::SWAP, Phase(0), make_tuple(trace1, tracePred1),
                           make_tuple(swapTime, swapTime + SWAP_DELAY));
        operationList.push_back(swapGate);

        trace1 = tracePred1;
    }
    // REVIEW - Check time, now the start time
    sort(operationList.begin(), operationList.end(), [](const Operation& a, const Operation& b) -> bool {
        return a.getOperationTime() < b.getOperationTime();
    });

    for (size_t i = 0; i < operationList.size(); ++i) {
        _device.applyGate(operationList[i]);
    }

    return operationList;
}

/**
 * @brief Assign gate
 *
 * @param gate
 * @return vector<Operation>
 */
vector<Operation> Router::assignGate(const Gate& gate) {
    tuple<size_t, size_t> physicalQubitsIds = getPhysicalQubits(gate);

    if (!(gate.getType() == GateType::CX || gate.getType() == GateType::CZ)) {
        assert(get<1>(physicalQubitsIds) == ERROR_CODE);
        Operation op = executeSingle(gate.getType(), gate.getPhase(), get<0>(physicalQubitsIds));
        op.setId(gate.getId());
        return vector<Operation>(1, op);
    }
    vector<Operation> operationList =
        _duostra
            ? duostraRouting(gate.getType(), gate.getId(), gate.getPhase(), physicalQubitsIds, _orient, gate.isSwapped())
            : apspRouting(gate.getType(), gate.getId(), gate.getPhase(), physicalQubitsIds, _orient, gate.isSwapped());
    vector<size_t> changeList = _device.mapping();
    vector<bool> checker(_logical2Physical.size(), false);

    // i is the idx of device qubit
    for (size_t i = 0; i < changeList.size(); ++i) {
        size_t logicalQubitId = changeList[i];
        if (logicalQubitId == ERROR_CODE) {
            continue;
        }
        _logical2Physical[logicalQubitId] = i;
    }
    return operationList;
}