/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Implementation of LatticeSurgery class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "latticesurgery/latticesurgery.hpp"
#include "latticesurgery/latticesurgery_io.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <queue>
#include <string>
#include <tl/enumerate.hpp>
#include <tl/fold.hpp>
#include <tl/to.hpp>
#include <unordered_set>
#include <variant>
#include <vector>
#include <filesystem>
#include <fstream>

namespace qsyn::latticesurgery {

LatticeSurgery::LatticeSurgery(LatticeSurgery const& other) {
    _gate_id = other._gate_id;
    _filename = other._filename;
    _qubits = other._qubits;
    _predecessors = other._predecessors;
    _successors = other._successors;

    // Deep copy gates
    for (auto const& [id, gate] : other._id_to_gates) {
        _id_to_gates[id] = std::make_unique<LatticeSurgeryGate>(*gate);
    }
}

void LatticeSurgery::reset() {
    _gate_id = 0;
    _filename.clear();
    _qubits.clear();
    _id_to_gates.clear();
    _predecessors.clear();
    _successors.clear();
    _gate_list.clear();
    _dirty = true;
}

void LatticeSurgery::insert_qubit(QubitIdType id) {
    if (id >= _qubits.size()) {
        _qubits.resize(id + 1);
    }
    _qubits[id] = LatticeSurgeryQubit(id);
}

void LatticeSurgery::add_qubits(size_t num) {
    size_t start = _qubits.size();
    _qubits.resize(start + num);
    for (size_t i = 0; i < num; ++i) {
        _qubits[start + i] = LatticeSurgeryQubit(start + i);
    }
}

bool LatticeSurgery::remove_qubit(QubitIdType qid) {
    if (qid >= _qubits.size()) {
        return false;
    }

    // Check if qubit is used in any gate
    for (auto const& [id, gate] : _id_to_gates) {
        if (gate->get_pin_by_qubit(qid).has_value()) {
            return false;
        }
    }

    _qubits.erase(_qubits.begin() + qid);
    return true;
}

size_t LatticeSurgery::append(LatticeSurgeryGate const& gate) {
    size_t id = _gate_id++;
    _id_to_gates[id] = std::make_unique<LatticeSurgeryGate>(id, gate.get_operation_type(), gate.get_qubits());
    _predecessors.emplace(id, std::vector<std::optional<size_t>>(gate.get_qubits().size(), std::nullopt));
    _successors.emplace(id, std::vector<std::optional<size_t>>(gate.get_qubits().size(), std::nullopt));
    auto* g = _id_to_gates[id].get();

    for (auto const& qb : g->get_qubits()) {
        auto last_gate = _qubits[qb].get_last_gate();
        if (last_gate.has_value()) {
            _connect(last_gate.value(), g->get_id(), qb);
        } else {
            _qubits[qb].set_first_gate(std::optional<size_t>(g->get_id()));
        }
        _qubits[qb].set_last_gate(std::optional<size_t>(g->get_id()));
    }
    _dirty = true;
    return id;
}

size_t LatticeSurgery::prepend(LatticeSurgeryGate const& gate) {
    size_t id = _gate_id++;
    _id_to_gates[id] = std::make_unique<LatticeSurgeryGate>(id, gate.get_operation_type(), gate.get_qubits());
    _predecessors.emplace(id, std::vector<std::optional<size_t>>(gate.get_qubits().size(), std::nullopt));
    _successors.emplace(id, std::vector<std::optional<size_t>>(gate.get_qubits().size(), std::nullopt));
    auto* g = _id_to_gates[id].get();

    for (auto const& qb : g->get_qubits()) {
        auto first_gate = _qubits[qb].get_first_gate();
        if (first_gate.has_value()) {
            _connect(g->get_id(), first_gate.value(), qb);
        } else {
            _qubits[qb].set_last_gate(std::optional<size_t>(g->get_id()));
        }
        _qubits[qb].set_first_gate(std::optional<size_t>(g->get_id()));
    }
    _dirty = true;
    return id;
}

bool LatticeSurgery::remove_gate(size_t id) {
    if (!_id_to_gates.contains(id)) {
        return false;
    }

    _id_to_gates.erase(id);
    _predecessors.erase(id);
    _successors.erase(id);
    _dirty = true;
    return true;
}

LatticeSurgeryGate* LatticeSurgery::get_gate(std::optional<size_t> gid) const {
    if (!gid.has_value()) return nullptr;
    if (_id_to_gates.contains(*gid)) {
        return _id_to_gates.at(*gid).get();
    }
    return nullptr;
}

void LatticeSurgery::_update_topological_order() const {
    if (!_dirty) return;

    _gate_list.clear();
    std::unordered_set<size_t> visited;
    std::unordered_set<size_t> temp;

    std::function<void(size_t)> dfs = [&](size_t id) {
        if (visited.count(id)) return;
        if (temp.count(id)) {
            spdlog::error("Circuit contains cycles!");
            return;
        }
        temp.insert(id);
        if (_successors.contains(id)) {
            for (auto const& succ : _successors.at(id)) {
                if (succ) {
                    dfs(*succ);
                }
            }
        }
        temp.erase(id);
        visited.insert(id);
        if (_id_to_gates.contains(id)) {
            _gate_list.push_back(_id_to_gates.at(id).get());
        }
    };

    for (auto const& [id, _] : _id_to_gates) {
        if (!visited.count(id)) {
            dfs(id);
        }
    }

    std::reverse(_gate_list.begin(), _gate_list.end());
    _dirty = false;
}

std::unordered_map<size_t, size_t> LatticeSurgery::calculate_gate_times() const {
    auto gate_times = std::unordered_map<size_t, size_t>{};
    auto const lambda = [&](LatticeSurgeryGate const* curr_gate) {
        auto const predecessors = get_predecessors(curr_gate->get_id());
        size_t const max_time = std::ranges::max(
            predecessors |
            std::views::transform(
                [&](std::optional<size_t> predecessor) {
                    return predecessor ? gate_times.at(*predecessor) : 0;
                }));
        gate_times[curr_gate->get_id()] = max_time + 1;
    };

    for (auto const* gate : get_gates()) {
        lambda(gate);
    }

    return gate_times;
}

size_t LatticeSurgery::calculate_depth() const {
    if (is_empty()) return 0;
    auto const times = calculate_gate_times();
    return std::ranges::max(times | std::views::values);
}

void LatticeSurgery::_set_predecessor(size_t gate_id, size_t pin, std::optional<size_t> pred) {
    auto pred_it = _predecessors.find(gate_id);
    if (pred_it != _predecessors.end() && pin < pred_it->second.size()) {
        pred_it->second[pin] = pred;
    }
}

void LatticeSurgery::_set_successor(size_t gate_id, size_t pin, std::optional<size_t> succ) {
    auto succ_it = _successors.find(gate_id);
    if (succ_it != _successors.end() && pin < succ_it->second.size()) {
        succ_it->second[pin] = succ;
    }
}

void LatticeSurgery::_set_predecessors(size_t gate_id, std::vector<std::optional<size_t>> const& preds) {
    auto pred_it = _predecessors.find(gate_id);
    if (pred_it != _predecessors.end()) {
        pred_it->second = preds;
    }
}

void LatticeSurgery::_set_successors(size_t gate_id, std::vector<std::optional<size_t>> const& succs) {
    auto succ_it = _successors.find(gate_id);
    if (succ_it != _successors.end()) {
        succ_it->second = succs;
    }
}

void LatticeSurgery::_connect(size_t gid1, size_t gid2, QubitIdType qubit) {
    auto* gate1 = get_gate(gid1);
    auto* gate2 = get_gate(gid2);
    if (!gate1 || !gate2) {
        return;
    }

    auto pin1 = gate1->get_pin_by_qubit(qubit);
    auto pin2 = gate2->get_pin_by_qubit(qubit);
    if (!pin1 || !pin2) {
        return;
    }

    _set_successor(gid1, *pin1, gid2);
    _set_predecessor(gid2, *pin2, gid1);
}

std::optional<size_t> LatticeSurgery::get_predecessor(std::optional<size_t> gate_id, size_t pin) const {
    if (!gate_id) return std::nullopt;
    auto pred_it = _predecessors.find(*gate_id);
    if (pred_it != _predecessors.end() && pin < pred_it->second.size()) {
        return pred_it->second[pin];
    }
    return std::nullopt;
}

std::optional<size_t> LatticeSurgery::get_successor(std::optional<size_t> gate_id, size_t pin) const {
    if (!gate_id) return std::nullopt;
    auto succ_it = _successors.find(*gate_id);
    if (succ_it != _successors.end() && pin < succ_it->second.size()) {
        return succ_it->second[pin];
    }
    return std::nullopt;
}

std::vector<std::optional<size_t>> LatticeSurgery::get_predecessors(std::optional<size_t> gate_id) const {
    if (!gate_id) return {};
    auto pred_it = _predecessors.find(*gate_id);
    if (pred_it != _predecessors.end()) {
        return pred_it->second;
    }
    return {};
}

std::vector<std::optional<size_t>> LatticeSurgery::get_successors(std::optional<size_t> gate_id) const {
    if (!gate_id) return {};
    auto succ_it = _successors.find(*gate_id);
    if (succ_it != _successors.end()) {
        return succ_it->second;
    }
    return {};
}

LatticeSurgeryGate* LatticeSurgery::get_first_gate(QubitIdType qubit) const {
    if (qubit >= _qubits.size()) {
        return nullptr;
    }
    auto first_gate = _qubits[qubit].get_first_gate();
    return first_gate ? get_gate(*first_gate) : nullptr;
}

LatticeSurgeryGate* LatticeSurgery::get_last_gate(QubitIdType qubit) const {
    if (qubit >= _qubits.size()) {
        return nullptr;
    }
    auto last_gate = _qubits[qubit].get_last_gate();
    return last_gate ? get_gate(*last_gate) : nullptr;
}

void LatticeSurgery::print_gates(bool print_neighbors, std::span<size_t> gate_ids) const {
    fmt::println("Listed by gate ID");

    auto const print_predecessors = [this](size_t gate_id) {
        auto const get_predecessor_gate_id =
            [](std::optional<size_t> pred) -> std::string {
            return pred.has_value() ? fmt::format("{}", *pred) : "Start";
        };
        auto const predecessors = get_predecessors(gate_id);
        fmt::println(
            "- Predecessors: {}",
            fmt::join(predecessors | std::views::transform(get_predecessor_gate_id),
                      ", "));
    };

    auto const print_successors = [this](size_t gate_id) {
        auto const get_successor_gate_id =
            [](std::optional<size_t> succ) -> std::string {
            return succ.has_value() ? fmt::format("{}", *succ) : "End";
        };
        auto const successors = get_successors(gate_id);
        fmt::println(
            "- Successors  : {}",
            fmt::join(successors | std::views::transform(get_successor_gate_id),
                      ", "));
    };

    auto const times = calculate_gate_times();

    auto const id_print_width =
        std::to_string(std::ranges::max(_id_to_gates | std::views::keys)).size();
    auto const repr_print_width =
        std::ranges::max(_id_to_gates | std::views::values |
                         std::views::transform([](auto const& gate) {
                             return std::string(gate->get_operation_type() == LatticeSurgeryOpType::merge ? "Merge" : "Split").size();
                         }));
    auto const time_print_width =
        std::to_string(std::ranges::max(times | std::views::values)).size();

    auto const print_one_gate([&](size_t id) {
        auto const gate   = get_gate(id);
        auto const qubits = gate->get_qubits();
        fmt::println("{0:>{1}} (t={2:>{3}}): {4:<{5}} {6:>5}", id, id_print_width,
                     times.at(id), time_print_width,
                     gate->get_operation_type() == LatticeSurgeryOpType::merge ? "Merge" : "Split", 
                     repr_print_width,
                     fmt::join(qubits | std::views::transform([](QubitIdType qid) {
                                   return fmt::format("q[{}]", qid);
                               }),
                               ", "));
        if (print_neighbors) {
            print_predecessors(id);
            print_successors(id);
        }
    });

    if (gate_ids.empty()) {
        for (auto const& [id, gate] : _id_to_gates) {
            print_one_gate(id);
        }
    } else {
        for (auto const id : gate_ids) {
            auto const gate = get_gate(id);
            if (gate == nullptr) {
                spdlog::error("Gate ID {} not found!!", id);
                continue;
            }
            print_one_gate(id);
        }
    }
}

void LatticeSurgery::print_ls() const {
    fmt::println("LS ({} qubits, {} operations)", get_num_qubits(), get_num_gates());
}

void LatticeSurgery::print_ls_info() const {
    size_t num_merge = 0;
    size_t num_split = 0;
    for (size_t i = 0; i < get_num_gates(); ++i) {
        auto const gate = get_gate(i);
        if (gate->get_operation_type() == LatticeSurgeryOpType::merge) {
            num_merge++;
        } else {
            num_split++;
        }
    }
    fmt::println("LS ({} qubits, {} operations, {} Merge, {} Split, {} depths)",
                get_num_qubits(),
                get_num_gates(),
                num_merge,
                num_split,
                calculate_depth());
}

} // namespace qsyn::latticesurgery