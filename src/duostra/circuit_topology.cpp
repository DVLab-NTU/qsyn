/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class circuit_topology member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./circuit_topology.hpp"

#include "qcir/gate_type.hpp"

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
 * @brief Add previous gate
 *
 * @param p
 */
void Gate::add_prev(size_t p) {
    if (p != SIZE_MAX)
        _prevs.emplace_back(p);
}

/**
 * @brief Add next gate
 *
 * @param n
 */
void Gate::add_next(size_t n) {
    if (n != SIZE_MAX)
        _nexts.emplace_back(n);
}

/**
 * @brief Is the gate are ready to be executed
 *
 * @param executedGates
 * @return true : all previous gates are executed,
 * @return false : else
 */
bool Gate::is_available(unordered_map<size_t, size_t> const& executed_gates) const {
    return all_of(_prevs.begin(), _prevs.end(), [&](size_t prev) -> bool {
        return executed_gates.find(prev) != executed_gates.end();
    });
}

// SECTION - Class CircuitTopo Member Functions

/**
 * @brief Construct a new Circuit Topo:: Circuit Topo object
 *
 * @param dep
 */
CircuitTopology::CircuitTopology(shared_ptr<DependencyGraph> const& dep) : _dependency_graph(dep), _available_gates({}), _executed_gates({}) {
    for (size_t i = 0; i < _dependency_graph->get_gates().size(); i++) {
        if (_dependency_graph->get_gate(i).is_available(_executed_gates))
            _available_gates.emplace_back(i);
    }
}

/**
 * @brief Clone CircuitTopo
 *
 * @return unique_ptr<CircuitTopo>
 */
unique_ptr<CircuitTopology> CircuitTopology::clone() const {
    return std::make_unique<CircuitTopology>(*this);
}

/**
 * @brief Update available gates by the executed gate
 *
 * @param executed
 */
void CircuitTopology::update_available_gates(size_t executed) {
    assert(find(begin(_available_gates), end(_available_gates), executed) != end(_available_gates));
    Gate const& gate_executed = get_gate(executed);
    _available_gates.erase(remove(begin(_available_gates), end(_available_gates), executed), end(_available_gates));
    assert(gate_executed.get_id() == executed);

    _executed_gates[executed] = 0;
    for (size_t next : gate_executed.get_nexts()) {
        if (get_gate(next).is_available(_executed_gates))
            _available_gates.emplace_back(next);
    }

    vector<size_t> gates_to_trim;
    for (size_t prev_id : gate_executed.get_prevs()) {
        auto const& prev_gate = get_gate(prev_id);
        ++_executed_gates[prev_id];
        if (_executed_gates[prev_id] >= prev_gate.get_nexts().size())
            gates_to_trim.emplace_back(prev_id);
    }
    for (size_t gate_id : gates_to_trim)
        _executed_gates.erase(gate_id);
}

/**
 * @brief Print gates with their successors
 *
 */
void CircuitTopology::print_gates_with_nexts() {
    cout << "Successors of each gate" << endl;
    auto const& gates = _dependency_graph->get_gates();
    for (size_t i = 0; i < gates.size(); i++) {
        vector<size_t> temp = gates[i].get_nexts();
        cout << gates[i].get_id() << "(" << gates[i].get_type() << ") || ";
        for (size_t j = 0; j < temp.size(); j++)
            cout << temp[j] << " ";
        cout << endl;
    }
}

/**
 * @brief Print gates with their predecessors
 *
 */
void CircuitTopology::print_gates_with_prevs() {
    cout << "Predecessors of each gate" << endl;
    auto const& gate = _dependency_graph->get_gates();
    for (size_t i = 0; i < gate.size(); i++) {
        auto const& prevs = gate.at(i).get_prevs();
        cout << gate.at(i).get_id() << "(" << gate.at(i).get_type() << ") || ";
        for (size_t j = 0; j < prevs.size(); j++)
            cout << prevs[j] << " ";
        cout << endl;
    }
}