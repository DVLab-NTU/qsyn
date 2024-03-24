/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class circuit_topology member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./circuit_topology.hpp"

#include <fmt/ranges.h>

#include <ranges>

using namespace qsyn::qcir;

namespace qsyn::duostra {

bool CircuitTopology::_is_available(size_t gate_idx) const {
    auto const preds = _dependency_graph->get_predecessors(gate_idx);  // circumvents g++ 11.4 compiler bug
    return std::ranges::all_of(
        preds |
            std::views::filter([](std::optional<size_t> const& id) { return id.has_value(); }) |
            std::views::transform([&](std::optional<size_t> const& id) { return id.value(); }),
        [&](size_t prev) -> bool {
            return _executed_gates.find(prev) != _executed_gates.end();
        });
}

// SECTION - Class CircuitTopo Member Functions

/**
 * @brief Construct a new Circuit Topo:: Circuit Topo object
 *
 * @param dep
 */
CircuitTopology::CircuitTopology(std::shared_ptr<qcir::QCir> const& dep) : _dependency_graph(dep), _available_gates({}), _executed_gates({}) {
    for (size_t i = 0; i < _dependency_graph->get_gates().size(); i++) {
        if (_is_available(_dependency_graph->get_gate(i)->get_id()))
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
    auto const& gate_executed = get_gate(executed);
    _available_gates.erase(remove(begin(_available_gates), end(_available_gates), executed), end(_available_gates));
    assert(gate_executed.get_id() == executed);

    _executed_gates[executed] = 0;
    for (auto next : _dependency_graph->get_successors(executed)) {
        if (!next) continue;
        if (_is_available(_dependency_graph->get_gate(*next)->get_id()))
            _available_gates.emplace_back(*next);
    }

    std::vector<size_t> gates_to_trim;
    for (auto prev_id : _dependency_graph->get_predecessors(executed)) {
        if (!prev_id) continue;
        ++_executed_gates[*prev_id];
        auto tmp                  = _dependency_graph->get_successors(prev_id);  // circumvents g++ 11.4 compiler bug
        auto prev_gate_successors = tmp | std::views::filter([](std::optional<size_t> const& id) { return id.has_value(); }) | tl::to<std::vector>();
        if (_executed_gates[*prev_id] >= prev_gate_successors.size())
            gates_to_trim.emplace_back(*prev_id);
    }
    for (auto gate_id : gates_to_trim)
        _executed_gates.erase(gate_id);
}

}  // namespace qsyn::duostra
