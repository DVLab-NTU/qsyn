/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class circuit_topology member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./circuit_topology.hpp"

#include <fmt/ranges.h>

#include "qcir/gate_type.hpp"

using namespace qsyn::qcir;

namespace qsyn::duostra {

// SECTION - Class Gate Member Functions

/**
 * @brief Construct a new Gate:: Gate object
 *
 * @param id
 * @param type
 * @param ph
 * @param qs
 */
Gate::Gate(size_t id, GateRotationCategory type, dvlab::Phase ph, std::tuple<size_t, size_t> qs)
    : _id(id), _type(type), _phase(ph), _qubits(qs) {
    if (std::get<0>(_qubits) > std::get<1>(_qubits)) {
        _qubits = std::make_tuple(std::get<1>(_qubits), std::get<0>(_qubits));
        _swap   = true;
    }
}

/**
 * @brief Add previous gate
 *
 * @param p
 */
void Gate::add_prev(size_t prev_gate_id) {
    assert(prev_gate_id != SIZE_MAX);
    _prevs.emplace_back(prev_gate_id);
}

/**
 * @brief Add next gate
 *
 * @param n
 */
void Gate::add_next(size_t next_gate_id) {
    assert(next_gate_id != SIZE_MAX);
    _nexts.emplace_back(next_gate_id);
}

/**
 * @brief Is the gate are ready to be executed
 *
 * @param executedGates
 * @return true : all previous gates are executed,
 * @return false : else
 */
bool Gate::is_available(std::unordered_map<size_t, size_t> const& executed_gates) const {
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
CircuitTopology::CircuitTopology(std::shared_ptr<DependencyGraph> const& dep) : _dependency_graph(dep), _available_gates({}), _executed_gates({}) {
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
std::unique_ptr<CircuitTopology> CircuitTopology::clone() const {
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
    for (auto next : gate_executed.get_nexts()) {
        if (get_gate(next).is_available(_executed_gates))
            _available_gates.emplace_back(next);
    }

    std::vector<size_t> gates_to_trim;
    for (auto prev_id : gate_executed.get_prevs()) {
        auto const& prev_gate = get_gate(prev_id);
        ++_executed_gates[prev_id];
        if (_executed_gates[prev_id] >= prev_gate.get_nexts().size())
            gates_to_trim.emplace_back(prev_id);
    }
    for (auto gate_id : gates_to_trim)
        _executed_gates.erase(gate_id);
}

/**
 * @brief Print gates with their successors
 *
 */
void CircuitTopology::print_gates_with_nexts() {
    fmt::println("Successors of each gate");
    for (auto const& gate : _dependency_graph->get_gates()) {
        fmt::println("{}({}) || {}", gate.get_id(), gate.get_type(), fmt::join(gate.get_nexts(), " "));
    }
}

/**
 * @brief Print gates with their predecessors
 *
 */
void CircuitTopology::print_gates_with_prevs() {
    fmt::println("Predecessors of each gate");
    for (auto const& gate : _dependency_graph->get_gates()) {
        fmt::println("{}({}) || {}", gate.get_id(), gate.get_type(), fmt::join(gate.get_prevs(), " "));
    }
}

/**
 * @brief HOTFIX map<old, new>
 *
 * @param map
 */
void Gate::set_prevs(std::unordered_map<size_t, size_t> const& map) {
    for (size_t i = 0; i < _prevs.size(); i++) {
        _prevs[i] = map.at(_prevs[i]);
    }
}
void Gate::set_nexts(std::unordered_map<size_t, size_t> const& map) {
    for (size_t i = 0; i < _nexts.size(); i++) {
        _nexts[i] = map.at(_nexts[i]);
    }
}

}  // namespace qsyn::duostra
