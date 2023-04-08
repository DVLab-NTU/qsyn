/****************************************************************************
  FileName     [ duostra.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "duostra.h"

#include "checker.h"
#include "variables.h"
using namespace std;
extern size_t verbose;

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

extern size_t verbose;

/**
 * @brief Get the Scheduler Type Str object
 *
 * @return string
 */
string getSchedulerTypeStr() {
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
string getRouterTypeStr() {
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
string getPlacerTypeStr() {
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
size_t getSchedulerType(string str) {
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
size_t getRouterType(string str) {
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
size_t getPlacerType(string str) {
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
Duostra::Duostra(QCir* cir, Device dev, bool check, bool tqdm, bool silent) : _logicalCircuit(cir), _physicalCircuit(new QCir(0)), _device(dev), _check(check) {
    _tqdm = (silent == true) ? false : tqdm;
    _silent = silent;
    if (verbose > 3) cout << "Creating dependency of quantum circuit..." << endl;
    makeDependency();
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
Duostra::Duostra(const vector<Operation>& cir, size_t nQubit, Device dev, bool check, bool tqdm, bool silent) : _logicalCircuit(nullptr), _physicalCircuit(new QCir(0)), _device(dev), _check(check) {
    _tqdm = (silent == true) ? false : tqdm;
    _silent = silent;
    if (verbose > 3) cout << "Creating dependency of quantum circuit..." << endl;
    makeDependency(cir, nQubit);
}

/**
 * @brief Make dependency graph from QCir*
 *
 */
void Duostra::makeDependency() {
    vector<Gate> allGates;
    for (const auto& g : _logicalCircuit->getGates()) {
        size_t id = g->getId();

        GateType type = g->getType();

        size_t q2 = ERROR_CODE;
        BitInfo first = g->getQubits()[0];
        BitInfo second = {};
        if (g->getQubits().size() > 1) {
            second = g->getQubits()[1];
            q2 = second._qubit;
        }

        tuple<size_t, size_t> temp{first._qubit, q2};
        Gate tempGate{id, type, g->getPhase(), temp};
        if (first._parent != nullptr) tempGate.addPrev(first._parent->getId());
        if (first._child != nullptr) tempGate.addNext(first._child->getId());
        if (g->getQubits().size() > 1) {
            if (second._parent != nullptr) tempGate.addPrev(second._parent->getId());
            if (second._child != nullptr) tempGate.addNext(second._child->getId());
        }
        allGates.push_back(move(tempGate));
    }
    _dependency = make_shared<DependencyGraph>(_logicalCircuit->getNQubit(), move(allGates));
}

/**
 * @brief Make dependency graph from vector<Operation>
 *
 * @param oper in topological order
 */
void Duostra::makeDependency(const vector<Operation>& oper, size_t nQubit) {
    vector<size_t> lastGate;  // idx:qubit value: Gate id
    vector<Gate> allGates;
    lastGate.resize(nQubit, ERROR_CODE);
    for (size_t i = 0; i < oper.size(); i++) {
        Gate tempGate{i, oper[i].getType(), oper[i].getPhase(), oper[i].getQubits()};
        size_t q0Gate = lastGate[get<0>(oper[i].getQubits())];
        size_t q1Gate = lastGate[get<1>(oper[i].getQubits())];
        tempGate.addPrev(q0Gate);
        if (q0Gate != q1Gate)
            tempGate.addPrev(q1Gate);
        if (q0Gate != ERROR_CODE)
            allGates[q0Gate].addNext(i);
        if (q1Gate != ERROR_CODE && q1Gate != q0Gate) {
            allGates[q1Gate].addNext(i);
        }
        lastGate[get<0>(oper[i].getQubits())] = i;
        lastGate[get<1>(oper[i].getQubits())] = i;
        allGates.push_back(move(tempGate));
    }
    _dependency = make_shared<DependencyGraph>(nQubit, move(allGates));
}

/**
 * @brief Main flow of Duostra mapper
 *
 * @return size_t
 */
size_t Duostra::flow(bool useDeviceAsPlacement) {
    unique_ptr<CircuitTopo> topo;
    topo = make_unique<CircuitTopo>(_dependency);
    auto checkTopo = topo->clone();
    auto checkDevice(_device);

    if (verbose > 3) cout << "Creating device..." << endl;
    if (topo->getNumQubits() > _device.getNQubit()) {
        cerr << "Error: number of logical qubits are larger than the device!!" << endl;
        return ERROR_CODE;
    }
    vector<size_t> assign;
    if (!useDeviceAsPlacement) {
        if (verbose > 3) cout << "Initial placing..." << endl;
        auto placer = getPlacer();
        assign = placer->placeAndAssign(_device);
    }
    // scheduler
    if (verbose > 3) cout << "Creating Scheduler..." << endl;
    auto sched = getScheduler(move(topo), _tqdm);

    // router
    if (verbose > 3) cout << "Creating Router..." << endl;
    string cost = (DUOSTRA_SCHEDULER == 3) ? "end" : "start";
    auto router = make_unique<Router>(move(_device), cost, DUOSTRA_ORIENT);

    // routing
    if (!_silent) cout << "Routing..." << endl;
    _device = sched->assignGatesAndSort(move(router));

    if (_check) {
        if (!_silent) cout << "Checking..." << endl;
        Checker checker(*checkTopo, checkDevice, sched->getOperations(), assign, _tqdm);
        if (!checker.testOperations()) {
            return ERROR_CODE;
        }
    }
    if (!_silent) {
        cout << "Duostra Result: " << endl;
        cout << endl;
        cout << "Scheduler:      " << getSchedulerTypeStr() << endl;
        cout << "Router:         " << getRouterTypeStr() << endl;
        cout << "Placer:         " << getPlacerTypeStr() << endl;
        cout << endl;
        cout << "Mapping Depth:  " << sched->getFinalCost() << "\n";
        cout << "Total Time:     " << sched->getTotalTime() << "\n";
        cout << "#SWAP:          " << sched->getSwapNum() << "\n";
        cout << endl;
    }
    assert(sched->isSorted());
    assert(sched->getOrder().size() == _dependency->gates().size());
    _result = sched->getOperations();
    storeOrderInfo(sched->getOrder());
    buildCircuitByResult();
    cout.clear();
    return sched->getFinalCost();
}

/**
 * @brief Convert index to full information of gate
 *
 * @param order
 */
void Duostra::storeOrderInfo(const std::vector<size_t>& order) {
    for (const auto& gateId : order) {
        const Gate& g = _dependency->getGate(gateId);
        tuple<size_t, size_t> qubits = g.getQubits();
        if (g.isSwapped())
            qubits = make_tuple(get<1>(qubits), get<0>(qubits));
        Operation op(g.getType(), g.getPhase(), qubits, {});
        op.setId(g.getId());
        _order.emplace_back(op);
    }
}

/**
 * @brief Print as qasm form
 *
 */
void Duostra::printAssembly() const {
    cout << "Mapping Result: " << endl;
    cout << endl;
    for (size_t i = 0; i < _result.size(); ++i) {
        const auto& op = _result.at(i);
        string gateName{gateType2Str[op.getType()]};
        cout << left << setw(5) << gateName << " ";
        tuple<size_t, size_t> qubits = op.getQubits();
        string res = "q[" + to_string(get<0>(qubits)) + "]";
        if (get<1>(qubits) != ERROR_CODE) {
            res = res + ",q[" + to_string(get<1>(qubits)) + "]";
        }
        res += ";";
        cout << left << setw(20) << res;
        cout << " // (" << op.getOperationTime() << "," << op.getCost() << ")   Origin gate: " << op.getId() << "\n";
    }
}

/**
 * @brief Construct physical QCir by operation
 *
 */
void Duostra::buildCircuitByResult() {
    if (_logicalCircuit != nullptr) {
        _physicalCircuit->addProcedure("Duostra", _logicalCircuit->getProcedures());
        _physicalCircuit->setFileName(_logicalCircuit->getFileName());
    }
    _physicalCircuit->addQubit(_device.getNQubit());
    for (const auto& operation : _result) {
        string gateName{gateType2Str[operation.getType()]};
        tuple<size_t, size_t> qubits = operation.getQubits();
        vector<size_t> qu;
        qu.emplace_back(get<0>(qubits));
        if (get<1>(qubits) != ERROR_CODE) {
            qu.emplace_back(get<1>(qubits));
        }
        if (operation.getType() == GateType::SWAP) {
            // NOTE - Decompose SWAP into three CX
            vector<size_t> quReverse;
            quReverse.emplace_back(get<1>(qubits));
            quReverse.emplace_back(get<0>(qubits));
            _physicalCircuit->addGate("CX", qu, Phase(0), true);
            _physicalCircuit->addGate("CX", quReverse, Phase(0), true);
            _physicalCircuit->addGate("CX", qu, Phase(0), true);
        } else
            _physicalCircuit->addGate(gateName, qu, operation.getPhase(), true);
    }
}
