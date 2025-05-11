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
    _grid = other._grid;

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
    _grid = LatticeSurgeryGrid();
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

bool LatticeSurgery::check_connectivity(std::vector<QubitIdType> const& patch_ids) const {
    if (patch_ids.empty()) return false;

    // Use BFS to check if all patches are connected
    std::unordered_set<QubitIdType> visited;
    std::queue<QubitIdType> q;
    q.push(patch_ids[0]);
    visited.insert(patch_ids[0]);

    while (!q.empty()) {
        QubitIdType current = q.front();
        q.pop();

        // Check all adjacent patches
        for (QubitIdType adj : get_adjacent_patches(current)) {
            // Only consider patches that are in our target set
            if (std::find(patch_ids.begin(), patch_ids.end(), adj) != patch_ids.end() &&
                !visited.contains(adj)) {
                visited.insert(adj);
                q.push(adj);
            }
        }
    }

    // Check if all patches were visited
    return visited.size() == patch_ids.size();
}

bool LatticeSurgery::check_same_logical_id(std::vector<QubitIdType> const& patch_ids) const {
    if (patch_ids.empty()) return false;
    
    QubitIdType logical_id = get_patch(patch_ids[0]).get_logical_id();
    for (size_t i = 1; i < patch_ids.size(); ++i) {
        if (get_patch(patch_ids[i]).get_logical_id() != logical_id) {
            return false;
        }
    }
    return true;
}

QubitIdType LatticeSurgery::get_smallest_logical_id(std::vector<QubitIdType> const& patch_ids) const {
    if (patch_ids.empty()) return 0;
    
    QubitIdType smallest = get_patch(patch_ids[0]).get_logical_id();
    for (size_t i = 1; i < patch_ids.size(); ++i) {
        QubitIdType current = get_patch(patch_ids[i]).get_logical_id();
        if (current < smallest) {
            smallest = current;
        }
    }
    return smallest;
}

void LatticeSurgery::_init_logical_tracking(size_t num_patches) {
    _logical_parent.resize(num_patches);
    _logical_rank.resize(num_patches, 0);
    for (size_t i = 0; i < num_patches; ++i) {
        _logical_parent[i] = i;  // Each patch starts as its own logical qubit
    }
}

QubitIdType LatticeSurgery::_find_logical_id(QubitIdType id) const {
    // Since we can't modify _logical_parent in a const function,
    // we'll just return the current parent without path compression
    if (_logical_parent[id] != id) {
        return _find_logical_id(_logical_parent[id]);
    }
    return id;
}

// Add a non-const version for internal use
QubitIdType LatticeSurgery::_find_logical_id_with_compression(QubitIdType id) {
    if (_logical_parent[id] != id) {
        _logical_parent[id] = _find_logical_id_with_compression(_logical_parent[id]);  // Path compression
    }
    return _logical_parent[id];
}

void LatticeSurgery::_union_logical_ids(QubitIdType id1, QubitIdType id2) {
    QubitIdType root1 = _find_logical_id(id1);
    QubitIdType root2 = _find_logical_id(id2);
    
    if (root1 == root2) return;  // Already in same set
    
    // Union by rank
    if (_logical_rank[root1] < _logical_rank[root2]) {
        _logical_parent[root1] = root2;
    } else if (_logical_rank[root1] > _logical_rank[root2]) {
        _logical_parent[root2] = root1;
    } else {
        _logical_parent[root2] = root1;
        _logical_rank[root1]++;
    }
}

std::vector<std::vector<QubitIdType>> LatticeSurgery::_get_connected_components(std::vector<QubitIdType> const& patch_ids) const {
    std::vector<std::vector<QubitIdType>> components;
    std::unordered_set<QubitIdType> visited;
    
    // Create a set of patches to split for quick lookup
    std::unordered_set<QubitIdType> patches_to_split(patch_ids.begin(), patch_ids.end());
    
    // First, find all patches that share the same logical ID as the patches to split
    QubitIdType target_logical_id = get_patch(patch_ids[0]).get_logical_id();
    std::vector<QubitIdType> all_related_patches;
    for (size_t i = 0; i < get_num_qubits(); ++i) {
        if (get_patch(i).get_logical_id() == target_logical_id) {
            all_related_patches.push_back(i);
        }
    }
    
    // For each unvisited patch in the related patches, start a new component
    for (QubitIdType start_patch : all_related_patches) {
        if (visited.contains(start_patch)) continue;
        
        // Start a new component
        std::vector<QubitIdType> component;
        std::queue<QubitIdType> q;
        q.push(start_patch);
        visited.insert(start_patch);
        component.push_back(start_patch);
        
        // BFS to find all connected patches
        while (!q.empty()) {
            QubitIdType current = q.front();
            q.pop();
            
            // Check all adjacent patches
            for (QubitIdType adj : get_adjacent_patches(current)) {
                // Skip if the adjacent patch is in our split set and not the current patch
                // This effectively removes edges between patches we want to split
                if (patches_to_split.contains(adj) && patches_to_split.contains(current)) {
                    continue;
                }
                
                // Only consider patches that are in our target set and not visited
                if (std::find(all_related_patches.begin(), all_related_patches.end(), adj) != all_related_patches.end() &&
                    !visited.contains(adj)) {
                    visited.insert(adj);
                    q.push(adj);
                    component.push_back(adj);
                }
            }
        }
        
        components.push_back(std::move(component));
    }
    
    return components;
}

bool LatticeSurgery::merge_patches(std::vector<QubitIdType> const& patch_ids) {
    // Check if patches are connected
    if (!check_connectivity(patch_ids)) {
        spdlog::error("Cannot merge patches: patches are not connected");
        return false;
    }

    // Union all patches into one logical qubit
    for (size_t i = 1; i < patch_ids.size(); ++i) {
        _union_logical_ids(patch_ids[0], patch_ids[i]);
    }

    // Update logical IDs for all patches
    QubitIdType target_logical_id = _find_logical_id_with_compression(patch_ids[0]);
    for (QubitIdType patch_id : patch_ids) {
        get_patch(patch_id).set_logical_id(target_logical_id);
    }

    return true;
}

bool LatticeSurgery::split_patches(std::vector<QubitIdType> const& patch_ids) {
    // Check if patches are connected and have the same logical ID
    if (!check_connectivity(patch_ids)) {
        spdlog::error("Cannot split patches: patches are not connected");
        return false;
    }
    if (!check_same_logical_id(patch_ids)) {
        spdlog::error("Cannot split patches: patches have different logical IDs");
        return false;
    }

    // Get connected components after removing edges between patches
    std::vector<std::vector<QubitIdType>> components = _get_connected_components(patch_ids);
    if (components.size() < 2) {
        spdlog::error("Cannot split patches: patches form a single connected component");
        return false;
    }

    // Find the original logical ID that these patches shared
    QubitIdType original_logical_id = get_patch(patch_ids[0]).get_logical_id();

    // Create a set of patches to split for quick lookup
    std::unordered_set<QubitIdType> patches_to_split(patch_ids.begin(), patch_ids.end());

    // For each component, determine which patch in the split set it contains
    // and assign logical IDs accordingly
    for (size_t i = 0; i < components.size(); ++i) {
        // Check if this component contains any patches we're splitting
        bool contains_split_patch = std::any_of(components[i].begin(), components[i].end(),
                                              [&patches_to_split](QubitIdType id) {
                                                  return patches_to_split.contains(id);
                                              });

        if (contains_split_patch) {
            // This component contains patches we're splitting
            // Use the smallest patch ID in the entire component as the new logical ID
            QubitIdType new_logical_id = *std::min_element(components[i].begin(), 
                                                         components[i].end());
            
            // Assign the new logical ID to all patches in this component
            for (QubitIdType patch_id : components[i]) {
                get_patch(patch_id).set_logical_id(new_logical_id);
                _logical_parent[patch_id] = new_logical_id;
                _logical_rank[patch_id] = 0;
            }
        } else {
            // This component doesn't contain any patches we're splitting
            // Keep the original logical ID for all patches in this component
            for (QubitIdType patch_id : components[i]) {
                get_patch(patch_id).set_logical_id(original_logical_id);
                _logical_parent[patch_id] = original_logical_id;
                _logical_rank[patch_id] = 0;
            }
        }
    }

    return true;
}

} // namespace qsyn::latticesurgery