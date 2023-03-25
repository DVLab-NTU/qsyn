/****************************************************************************
  FileName     [ duostra.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "duostra.h"

using namespace std;
extern size_t verbose;

/**
 * @brief Construct a new Duostra:: Duostra object
 *
 * @param cir
 * @param dev
 */
Duostra::Duostra(QCir* cir, Device dev) : _logicalCircuit(cir), _physicalCircuit(new QCir(0)), _device(dev) {
}

/**
 * @brief Make dependency graph
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
 * @brief Main flow of Duostra mapper
 *
 * @return size_t
 */
size_t Duostra::flow() {
    if (verbose > 3) cout << "Creating dependency of quantum circuit..." << endl;
    makeDependency();
    unique_ptr<CircuitTopo> topo;
    topo = make_unique<CircuitTopo>(_dependency);

    if (verbose > 3) cout << "Creating device..." << endl;
    if (topo->getNumQubits() > _device.getNQubit()) {
        cerr << "You cannot assign more qubits than the device." << endl;
        abort();
    }

    if (verbose > 3) cout << "Initial placing..." << endl;
    string placerType = "dfs";
    auto placer = getPlacer(placerType);
    placer->placeAndAssign(_device);

    // scheduler

    if (verbose > 3) cout << "Creating Scheduler..." << endl;
    string schedulerType = "search";
    auto sched = getScheduler(schedulerType, move(topo));

    // router
    if (verbose > 3) cout << "Creating Router..." << endl;
    string routerType = "duostra";
    bool orient = true;
    string greedyCost = "end";
    string cost = (schedulerType == "greedy") ? greedyCost : "start";
    auto router = make_unique<Router>(move(_device), routerType, cost, orient);

    // routing
    cout << "Routing..." << endl;
    sched->assignGatesAndSort(move(router));

    cout << "Duostra Result: " << endl;
    cout << endl;
    cout << "Placer:         " << placerType << endl;
    cout << "Scheduler:      " << schedulerType << endl;
    cout << "Router:         " << routerType << endl;
    cout << endl;
    cout << "Mapping Depth:  " << sched->getFinalCost() << "\n";
    cout << "Total Time:     " << sched->getTotalTime() << "\n";
    cout << "#SWAP:          " << sched->getSwapNum() << "\n";
    cout << endl;

    assert(sched->isSorted());
    _result = sched->getOperations();
    buildCircuitByResult();
    cout.clear();
    return sched->getFinalCost();
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
        cout << " // (" << op.getOperationTime() << "," << op.getCost() << ")\n";
    }
}

/**
 * @brief Construct physical QCir by operation
 *
 */
void Duostra::buildCircuitByResult() {
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
