/****************************************************************************
  FileName     [ circuitTopology.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class circuitTopology member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "circuitTopology.h"

using namespace std;

// SECTION - Class Gate Member Functions

/**
 * @brief Construct a new Gate:: Gate object
 *
 * @param id
 * @param type
 * @param ph
 * @param qs
 */
Gate::Gate(size_t id, GateType type, Phase ph, std::tuple<size_t, size_t> qs)
    : _id(id), _type(type), _phase(ph), _swap(false), _qubits(qs), _prevs({}), _nexts({}) {
    if (std::get<0>(_qubits) > std::get<1>(_qubits)) {
        _qubits = std::make_tuple(std::get<1>(_qubits), std::get<0>(_qubits));
        _swap = true;
    }
}

/**
 * @brief Construct a new Gate:: Gate object
 *
 * @param other
 */
Gate::Gate(Gate&& other)
    : _id(other._id), _type(other._type), _phase(other._phase), _swap(other._swap), _qubits(other._qubits), _prevs(other._prevs), _nexts(other._nexts) {}

/**
 * @brief Add previous gate
 *
 * @param p
 */
void Gate::addPrev(size_t p) {
    if (p != ERROR_CODE)
        _prevs.push_back(p);
}

/**
 * @brief Add next gate
 *
 * @param n
 */
void Gate::addNext(size_t n) {
    if (n != ERROR_CODE)
        _nexts.push_back(n);
}

/**
 * @brief Is the gate are ready to be executed
 *
 * @param executedGates
 * @return true : all previous gates are executed,
 * @return false : else
 */
bool Gate::isAvailable(const unordered_map<size_t, size_t>& executedGates) const {
    return all_of(_prevs.begin(), _prevs.end(), [&](size_t prev) -> bool {
        return executedGates.find(prev) != executedGates.end();
    });
}

// SECTION - Class CircuitTopo Member Functions

/**
 * @brief Construct a new Circuit Topo:: Circuit Topo object
 *
 * @param dep
 */
CircuitTopo::CircuitTopo(shared_ptr<DependencyGraph> dep) : _dependencyGraph(dep), _availableGates({}), _executedGates({}) {
    for (size_t i = 0; i < _dependencyGraph->gates().size(); i++) {
        if (_dependencyGraph->getGate(i).isAvailable(_executedGates))
            _availableGates.push_back(i);
    }
}

/**
 * @brief Construct a new Circuit Topo:: Circuit Topo object
 *
 * @param other
 */
CircuitTopo::CircuitTopo(const CircuitTopo& other)
    : _dependencyGraph(other._dependencyGraph),
      _availableGates(other._availableGates),
      _executedGates(other._executedGates) {}

/**
 * @brief Construct a new Circuit Topo:: Circuit Topo object
 *
 * @param other
 */
CircuitTopo::CircuitTopo(CircuitTopo&& other)
    : _dependencyGraph(move(other._dependencyGraph)),
      _availableGates(move(other._availableGates)),
      _executedGates(move(other._executedGates)) {}

/**
 * @brief Clone CircuitTopo
 *
 * @return unique_ptr<CircuitTopo>
 */
unique_ptr<CircuitTopo> CircuitTopo::clone() const {
    return std::make_unique<CircuitTopo>(*this);
}

/**
 * @brief Update available gates by the executed gate
 *
 * @param executed
 */
void CircuitTopo::updateAvailableGates(size_t executed) {
    assert(find(begin(_availableGates), end(_availableGates), executed) != end(_availableGates));
    const Gate& gateExecuted = getGate(executed);
    _availableGates.erase(remove(begin(_availableGates), end(_availableGates), executed), end(_availableGates));
    assert(gateExecuted.getId() == executed);

    _executedGates[executed] = 0;
    for (size_t next : gateExecuted.getNexts()) {
        if (getGate(next).isAvailable(_executedGates))
            _availableGates.push_back(next);
    }

    vector<size_t> gatesToTrim;
    for (size_t prevId : gateExecuted.getPrevs()) {
        const auto& prev_gate = getGate(prevId);
        ++_executedGates[prevId];
        if (_executedGates[prevId] >= prev_gate.getNexts().size())
            gatesToTrim.push_back(prevId);
    }
    for (size_t gateId : gatesToTrim)
        _executedGates.erase(gateId);
}

/**
 * @brief Print gates with their successors
 *
 */
void CircuitTopo::printGatesWithNexts() {
    cout << "Successors of each gate" << endl;
    const auto& gates = _dependencyGraph->gates();
    for (size_t i = 0; i < gates.size(); i++) {
        vector<size_t> temp = gates[i].getNexts();
        cout << gates[i].getId() << "(" << gates[i].getType() << ") || ";
        for (size_t j = 0; j < temp.size(); j++)
            cout << temp[j] << " ";
        cout << endl;
    }
}

/**
 * @brief Print gates with their predecessors
 *
 */
void CircuitTopo::printGatesWithPrevs() {
    cout << "Predecessors of each gate" << endl;
    const auto& gate = _dependencyGraph->gates();
    for (size_t i = 0; i < gate.size(); i++) {
        const auto& prevs = gate.at(i).getPrevs();
        cout << gate.at(i).getId() << "(" << gate.at(i).getType() << ") || ";
        for (size_t j = 0; j < prevs.size(); j++)
            cout << prevs[j] << " ";
        cout << endl;
    }
}