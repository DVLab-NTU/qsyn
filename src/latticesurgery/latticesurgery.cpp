/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Implementation of LatticeSurgery class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "latticesurgery/latticesurgery.hpp"
#include <fmt/core.h>
#include "latticesurgery/latticesurgery_gate.hpp"
#include "latticesurgery/latticesurgery_io.hpp"
#include "latticesurgery/latticesurgery_qubit.hpp"
#include "qsyn/qsyn_type.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <queue>
#include <string>
#include <tl/enumerate.hpp>
#include <tl/fold.hpp>
#include <tl/to.hpp>
#include <unordered_set>
#include <utility>
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

    // Check if qubit is used in any 
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
    _id_to_gates[id] = std::make_unique<LatticeSurgeryGate>(id, gate.get_operation_type(), gate.get_qubits(), gate.get_measure_types(), gate.get_depth());
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
                             return std::string(
                                 gate->get_operation_type() == LatticeSurgeryOpType::merge ? "Merge" : 
                                 gate->get_operation_type() == LatticeSurgeryOpType::split ? "Split" : "Measure"
                             ).size();
                         }));
    auto const time_print_width =
        std::to_string(std::ranges::max(times | std::views::values)).size();

    auto const print_one_gate([&](size_t id) {
        auto const gate   = get_gate(id);
        auto const qubits = gate->get_qubits();
        std::string op_type_str;
        switch (gate->get_operation_type()) {
            case LatticeSurgeryOpType::merge:
                op_type_str = "Merge";
                break;
            case LatticeSurgeryOpType::split:
                op_type_str = "Split";
                break;
            case LatticeSurgeryOpType::hadamard_l:
                op_type_str = "Hadamard_L";
                break;
            case LatticeSurgeryOpType::measure:
                op_type_str = "Measure";
                break;
            default:
                op_type_str = "Unknown";
                break;
        }
        
        fmt::println("{0:>{1}} (t={2:>{3}}): {4:<{5}} {6:>5}", id, id_print_width,
                     times.at(id), time_print_width,
                     op_type_str, 
                     repr_print_width,
                     fmt::join(qubits | std::views::transform([](QubitIdType qid) {
                                   return fmt::format("q[{}]", qid);
                               }),
                               ", "));
        
        // For measure operations, display the measure types
        if (gate->get_operation_type() == LatticeSurgeryOpType::measure) {
            auto const& measure_types = gate->get_measure_types();
            if (!measure_types.empty()) {
                fmt::println("  Measure types: {}", fmt::join(measure_types | std::views::transform([](MeasureType type) {
                    switch (type) {
                        case MeasureType::x: return "X";
                        case MeasureType::y: return "Y";
                        case MeasureType::z: return "Z";
                        default: return "Unknown";
                    }
                }), ", "));
            }
        }
        
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
    size_t num_measure = 0;
    size_t num_hadamard_l = 0;
    for (auto const& [id, gate] : _id_to_gates) {
        auto op_type = gate->get_operation_type();
        if (op_type == LatticeSurgeryOpType::merge) {
            num_merge++;
        } else if (op_type == LatticeSurgeryOpType::split) {
            num_split++;
        } else if (op_type == LatticeSurgeryOpType::hadamard_l) {
            num_hadamard_l++;
        } else if (op_type == LatticeSurgeryOpType::measure) {
            num_measure++;
        }
    }
    fmt::println("LS ({} qubits, {} operations, {} Merge, {} Split, {} Hadamard_L, {} Measure, {} depths)",
                get_num_qubits(),
                get_num_gates(),
                num_merge,
                num_split,
                num_hadamard_l,
                num_measure,
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

        // Print debug info
        auto adjacents = get_adjacent_patches(current);

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
    
    QubitIdType logical_id = get_patch(patch_ids[0])->get_logical_id();
    for (size_t i = 1; i < patch_ids.size(); ++i) {
        if (get_patch(patch_ids[i])->get_logical_id() != logical_id) {
            return false;
        }
    }
    return true;
}

QubitIdType LatticeSurgery::get_smallest_logical_id(std::vector<QubitIdType> const& patch_ids) const {
    if (patch_ids.empty()) return 0;
    
    QubitIdType smallest = get_patch(patch_ids[0])->get_logical_id();
    for (size_t i = 1; i < patch_ids.size(); ++i) {
        QubitIdType current = get_patch(patch_ids[i])->get_logical_id();
        if (current < smallest) {
            smallest = current;
        }
    }
    return smallest;
}

void LatticeSurgery::_init_logical_tracking(size_t num_patches) {
    fmt::println("Initializing logical tracking with {} patches", num_patches);
    if (num_patches == 0) {
        fmt::println("WARNING: Attempted to initialize logical tracking with 0 patches");
        num_patches = 1; // Ensure at least one patch
    }
    
    _logical_parent.resize(num_patches);
    _logical_rank.resize(num_patches, 0);
    for (size_t i = 0; i < num_patches; ++i) {
        _logical_parent[i] = get_patch(i)->get_id();  // Each patch starts as its own logical qubit
    }
    
}

QubitIdType LatticeSurgery::_find_logical_id(QubitIdType id) const {
    // Since we can't modify _logical_parent in a const function,
    // we'll just return the current parent without path compression
    // Check if the id is within the valid range
    if (id >= _logical_parent.size()) {
        fmt::println("WARNING: Attempt to find logical ID of patch {} but _logical_parent size is only {}", 
                    id, _logical_parent.size());
        return id; // Return the id itself as a fallback
    }
    if (_logical_parent[id] != id) {
        return _find_logical_id(_logical_parent[id]);
    }
    return id;
}

// Add a non-const version for internal use
QubitIdType LatticeSurgery::_find_logical_id_with_compression(QubitIdType id) {
    // Check if the id is within the valid range
    if (id >= _logical_parent.size()) {
        fmt::println("WARNING: Attempt to find and compress logical ID of patch {} but _logical_parent size is only {}", 
                    id, _logical_parent.size());
        return id; // Return the id itself as a fallback
    }
    
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

size_t LatticeSurgery::get_patch_id(size_t col, size_t row) const {
    // Check if coordinates are within grid bounds
    if (col >= get_grid_cols() || row >= get_grid_rows()) {
        fmt::println("WARNING: Attempted to access patch at ({}, {}), but grid size is {}x{}", 
                   col, row, get_grid_cols(), get_grid_rows());
        return 0; // Return a default value
    }
    
    // Get the patch and check if it's null
    LatticeSurgeryQubit* patch = get_patch(col, row);
    if (patch == nullptr) {
        fmt::println("WARNING: Null patch at ({}, {})", col, row);
        return 0;
    }
    
    return patch->get_id();
}


std::vector<std::vector<QubitIdType>> LatticeSurgery::_get_connected_components(std::vector<QubitIdType> const& patch_ids) const {
    std::vector<std::vector<QubitIdType>> components;
    std::unordered_set<QubitIdType> visited;
    
    // Create a set of patches to split for quick lookup
    std::unordered_set<QubitIdType> patches_to_split(patch_ids.begin(), patch_ids.end());
    
    // First, find all patches that share the same logical ID as the patches to split
    QubitIdType target_logical_id = get_patch(patch_ids[0])->get_logical_id();
    std::vector<QubitIdType> all_related_patches;
    for (size_t i = 0; i < get_num_qubits(); ++i) {
        if (get_patch(i)->get_logical_id() == target_logical_id) {
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
    // Check for empty arguments
    if (patch_ids.empty()) {
        fmt::println("WARNING: Cannot merge empty patch list");
        return false;
    }
    
    // Debug: print all patch IDs
    fmt::println("Patches to merge (without measure types): {}", fmt::join(patch_ids, ", "));
    
    // Check if patches are connected
    if (!check_connectivity(patch_ids)) {
        spdlog::error("Cannot merge patches: patches are not connected");
        return false;
    }

    // Find the smallest non-zero logical ID among the patches
    QubitIdType smallest_logical_id = std::numeric_limits<QubitIdType>::max();
    bool found_non_zero = false;
    for (QubitIdType patch_id : patch_ids) {
        QubitIdType logical_id = get_patch(patch_id)->get_logical_id();
        if (logical_id != 0 && logical_id < smallest_logical_id) {
            smallest_logical_id = logical_id;
            found_non_zero = true;
        }
    }

    // If no non-zero logical ID found, use the first patch's ID
    if (!found_non_zero) {
        smallest_logical_id = patch_ids[0];
    }

    // Union all patches into one logical qubit
    for (size_t i = 1; i < patch_ids.size(); ++i) {
        _union_logical_ids(patch_ids[0], patch_ids[i]);
    }

    // Update logical IDs for all patches
    for (QubitIdType patch_id : patch_ids) {
        get_patch(patch_id)->set_logical_id(smallest_logical_id);
    }

    return true;
}

bool LatticeSurgery::merge_patches(std::vector<QubitIdType> patch_ids, std::vector<MeasureType> measure_types, bool color_flip = false, size_t depth = 0){
    // Check for empty arguments
    if (patch_ids.empty()) {
        fmt::println("WARNING: Cannot merge empty patch list");
        return false;
    }
    
    if (measure_types.empty()) {
        fmt::println("WARNING: No measure types provided for merge");
        return false;
    }
    
    // Debug: print all patch IDs
    fmt::println("Patches to merge (depth: {}): {}", depth, fmt::join(patch_ids, ", "));
    
    for (auto id : patch_ids) {
        // Validate all patch IDs exist
        if (get_patch(id) == nullptr) {
            fmt::println("WARNING: Cannot merge non-existent patch with ID {}", id);
            return false;
        }
        // Check all patches are within the grid bounds before checking connectivity
        if (id >= get_num_qubits()) {
            fmt::println("WARNING: Patch ID {} is out of bounds (max: {})", id, get_num_qubits()-1);
            return false;
        }
    }
    
    // Check if patches are connected
    if (!check_connectivity(patch_ids)) {
        spdlog::error("Cannot merge patches: patches are not connected");
        return false;
    }

    // Find the smallest non-zero logical ID among the patches
    QubitIdType smallest_logical_id = std::numeric_limits<QubitIdType>::max();
    bool found_non_zero = false;
    for (QubitIdType patch_id : patch_ids) {
        QubitIdType logical_id = get_patch(patch_id)->get_logical_id();
        if (logical_id != 0 && logical_id < smallest_logical_id) {
            smallest_logical_id = logical_id;
            found_non_zero = true;
        }
    }

    // If no non-zero logical ID found, use the first patch's ID
    if (!found_non_zero) {
        // return false;
        smallest_logical_id = patch_ids[0];
    }

    // Union all patches into one logical qubit
    for (size_t i = 1; i < patch_ids.size(); ++i) {
        _union_logical_ids(patch_ids[0], patch_ids[i]);
    }

    size_t max_depth = 0;
    // Update logical IDs for all patches
    if (depth == 0){
        for (QubitIdType patch_id : patch_ids) {
            get_patch(patch_id)->set_logical_id(smallest_logical_id);
            if(get_patch(patch_id)->get_depth() > max_depth) max_depth = get_patch(patch_id)->get_depth();
        }

        
    }

    // update the depth
    for(QubitIdType patch_id: patch_ids){
        get_patch(patch_id)->set_occupied(true);
        get_patch(patch_id)->set_depth(depth > 0 ? depth : max_depth+1);
    }
    

    append({color_flip ? LatticeSurgeryOpType::measure_c : LatticeSurgeryOpType::measure, patch_ids, measure_types, depth > 0 ? depth : max_depth+1});

    return true;
};

bool LatticeSurgery::split_patches(std::vector<QubitIdType> const& patch_ids) {
    fmt::println("Patches to split: {}", fmt::join(patch_ids, ", "));
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
    QubitIdType original_logical_id = get_patch(patch_ids[0])->get_logical_id();

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

                get_patch(patch_id)->set_logical_id(new_logical_id+1);
                _logical_parent[patch_id] = new_logical_id;
                _logical_rank[patch_id] = 0;
            }
        } else {
            // This component doesn't contain any patches we're splitting
            // Keep the original logical ID for all patches in this component
            for (QubitIdType patch_id : components[i]) {
                get_patch(patch_id)->set_logical_id(original_logical_id);
                _logical_parent[patch_id] = original_logical_id;
                _logical_rank[patch_id] = 0;
            }
        }
    }

    size_t max_depth = 0;
    for(auto patch_id: patch_ids){
        if(max_depth < get_patch(patch_id)->get_depth()) max_depth = get_patch(patch_id)->get_depth();
    }

    append({LatticeSurgeryOpType::split, patch_ids, max_depth+1});

    return true;
}

void LatticeSurgery::hadamard(size_t col, size_t row){
    // get the depth for the cur patch
    assert(col < get_grid_cols() && row < get_grid_rows());

    get_patch(col, row)->rotate();
    
};

void LatticeSurgery::hadamard(std::pair<size_t, size_t> start, std::vector<std::pair<size_t, size_t>>& dest_list, bool preserve_start, bool is_x){
    // New L-shaped Hadamard logic with color continuity
    // is_x: true start with | x direction, false if start with <-> z direction
    //get the involved patches

    size_t max_depth = 0;
    std::vector<size_t> merge_1_list;
    std::vector<size_t> merge_2_list;
    std::vector<size_t> discard_list;

    if(dest_list.size() == 1){
        if(is_x){
            // | x direction
            size_t ys = 0;
            size_t yl = 0;
            if(start.second < dest_list[0].second){
                ys = start.second;
                yl = dest_list[0].second;
            }
            else{
                ys = dest_list[0].second;
                yl = start.second;
            }
            for(size_t y=ys; y<=yl; y++){
                merge_1_list.push_back(get_patch_id(start.first, y));
                if(y != start.second || (y == start.second && !preserve_start)) discard_list.push_back(get_patch_id(start.first, y));
            }
            size_t xs = 0;
            size_t xl = 0;
            if(start.first < dest_list[0].first){
                xs = start.first;
                xl = dest_list[0].first;
            }
            else{
                xs = dest_list[0].first;
                xl = start.first;
            }
            for(size_t x=xs; x<=xl; x++){
                merge_2_list.push_back(get_patch_id(x, dest_list[0].second));
                if(x != dest_list[0].first) discard_list.push_back(get_patch_id(x, dest_list[0].second));
            }
        }
        else{
            // <-> z direction
            size_t xs = 0;
            size_t xl = 0;
            if(start.first < dest_list[0].first){
                xs = start.first;
                xl = dest_list[0].first;
            }
            else{
                xs = dest_list[0].first;
                xl = start.first;
            }
            for(size_t x=xs; x<=xl; x++){
                merge_1_list.push_back(get_patch_id(x, start.second));
                if(x != start.first || (x == start.first && !preserve_start)) discard_list.push_back(get_patch_id(x, start.second));
            }
            size_t ys = 0;
            size_t yl = 0;
            if(start.second < dest_list[0].second){
                ys = start.second;
                yl = dest_list[0].second;
            }
            else{
                ys = dest_list[0].second;
                yl = start.second;
            }
            for(size_t y=ys; y<=yl; y++){
                merge_2_list.push_back(get_patch_id(dest_list[0].first, y));
                if(y != dest_list[0].second) discard_list.push_back(get_patch_id(dest_list[0].first, y));
            }
        }
        for(auto m: merge_1_list){
            if(get_patch(m)->get_depth() > max_depth) max_depth = get_patch(m)->get_depth();
        }
        for(auto m: merge_2_list){
            if(get_patch(m)->get_depth() > max_depth) max_depth = get_patch(m)->get_depth();
        }
        merge_patches(merge_1_list, {is_x ? MeasureType::x : MeasureType::z}, false, max_depth+1);
        merge_patches(merge_2_list, {is_x ? MeasureType::z : MeasureType::x}, true, max_depth+1);
        for(auto d: discard_list){
            discard_patch(d, get_patch(d)->get_td_type());
        }
    }
    else{
        if(is_x){
            // | x direction
            size_t xs = 0;
            size_t xl = 0;
            if(start.first < dest_list[0].first){
                xs = start.first;
                xl = dest_list[0].first;
            }
            else{
                xs = dest_list[0].first;
                xl = start.first;
            }
            for(size_t x=xs; x<=xl; x++){
                merge_1_list.push_back(get_patch_id(x, start.second));
                if(x != start.first || (x == start.first && !preserve_start)) discard_list.push_back(get_patch_id(x, start.second));
            }
            size_t ys = start.second;
            size_t yl = start.second;
            for(auto [x, y]: dest_list){
                if(y < ys) ys = y;
                if(y > yl) yl = y;
            }
            for(size_t y=ys; y<=yl; y++){
                merge_2_list.push_back(get_patch_id(dest_list[0].first, y));
                // if (dest_list[0].first, y) not in dest_list, discard_list.push_back(get_patch_id(dest_list[0].first, y));
                if(std::find(dest_list.begin(), dest_list.end(), std::make_pair(dest_list[0].first, y)) == dest_list.end()) discard_list.push_back(get_patch_id(dest_list[0].first, y));
            }
        }
        else{
            // <-> z direction
            size_t ys = 0;
            size_t yl = 0;
            if(start.second < dest_list[0].second){
                ys = start.second;
                yl = dest_list[0].second;
            }
            else{
                ys = dest_list[0].second;
                yl = start.second;
            }
            for(size_t y=ys; y<=yl; y++){
                merge_1_list.push_back(get_patch_id(start.first, y));
                if(y != start.second || (y == start.second && !preserve_start)) discard_list.push_back(get_patch_id(start.first, y));
            }
            size_t xs = start.first;
            size_t xl = start.first;
            for(auto [x, y]: dest_list){
                if(x < xs) xs = x;
                if(x > xl) xl = x;
            }
            for(size_t x=xs; x<=xl; x++){
                merge_2_list.push_back(get_patch_id(x, dest_list[0].second));
                if(std::find(dest_list.begin(), dest_list.end(), std::make_pair(x, dest_list[0].second)) == dest_list.end()) discard_list.push_back(get_patch_id(x, dest_list[0].second));
            }
        }
        for(auto m: merge_1_list){
            if(get_patch(m)->get_depth() > max_depth) max_depth = get_patch(m)->get_depth();
        }
        for(auto m: merge_2_list){
            if(get_patch(m)->get_depth() > max_depth) max_depth = get_patch(m)->get_depth();
        }
        merge_patches(merge_1_list, {is_x ? MeasureType::z : MeasureType::x}, false, max_depth+1);
        merge_patches(merge_2_list, {is_x ? MeasureType::x : MeasureType::z}, true, max_depth+1);
        for(auto d: discard_list){
            discard_patch(d, get_patch(d)->get_td_type());
        }
    }
    
    return;
    
};

void LatticeSurgery::discard_patch(QubitIdType id, MeasureType measure_type){
    // Check if the patch exists
    LatticeSurgeryQubit* patch = get_patch(id);
    if (patch == nullptr) {
        fmt::println("WARNING: Cannot discard non-existent patch with ID {}", id);
        return;
    }

    fmt::println("Patch to discard: {}", id);

    // Check if the patch is occupied
    if (!patch->occupied()) {
        fmt::println("WARNING: Attempting to discard unoccupied patch with ID {}", id);
        return; // The patch is already unoccupied
    }

    // measure out the patch to reset
    append({LatticeSurgeryOpType::measure, {id}, {measure_type}, get_patch(id)->get_depth()});

    // set the patch to ancilla
    patch->set_occupied(false);
    patch->set_logical_id(0);
};

void LatticeSurgery::n_to_n(std::vector<std::pair<size_t,size_t>>& start_list, std::vector<std::pair<size_t,size_t>>& dest_list){
    // Handle empty or trivial cases
    if (start_list.empty() || dest_list.empty()) return;
    if (start_list == dest_list && start_list.size() == 1) return;

    // Combine all coordinates
    std::vector<std::pair<size_t, size_t>> all_coords = start_list;
    all_coords.insert(all_coords.end(), dest_list.begin(), dest_list.end());

    // Find bounding box
    size_t xs = all_coords[0].first, xl = all_coords[0].first;
    size_t ys = all_coords[0].second, yl = all_coords[0].second;
    for (auto [x, y] : all_coords) {
        if (x < xs) xs = x;
        if (x > xl) xl = x;
        if (y < ys) ys = y;
        if (y > yl) yl = y;
    }

    std::vector<size_t> merge_list;
    std::vector<size_t> keep_list; // patches to keep (in start or dest)
    std::vector<size_t> discard_list;

    // Helper lambda to check if a patch is in start or dest
    auto is_in_list = [](const std::vector<std::pair<size_t, size_t>>& lst, size_t x, size_t y) {
        for (auto [lx, ly] : lst) if (lx == x && ly == y) return true;
        return false;
    };

    if (xs == xl) { // vertical merge
        std::vector<bool> keep(yl - ys + 1, false);
        for (size_t i = 0; i < keep.size(); ++i) {
            size_t y = ys + i;
            if (is_in_list(dest_list, xs, y)) keep[i] = true;
        }
        // Check for occupation conflicts
        // for (size_t i = 0; i < keep.size(); ++i) {
        //     size_t patch_id = get_patch_id(xs, ys + i);
        //     if (!keep[i] && get_patch(patch_id)->occupied()) {
        //         fmt::println("ERROR: Cannot merge - patch at ({}, {}) with ID {} is already occupied", xs, ys + i, patch_id);
        //         fmt::println("Aborting n_to_n due to occupied patches in the path");
        //         return;
        //     }
        // }
        for (size_t i = 0; i < keep.size(); ++i) {
            size_t patch_id = get_patch_id(xs, ys + i);
            merge_list.push_back(patch_id);
            if (keep[i]) keep_list.push_back(patch_id);
            else discard_list.push_back(patch_id);
        }
        merge_patches(merge_list, {get_patch(merge_list.front())->get_td_type()});
        split_patches(merge_list);
        for (auto d : discard_list) {
            discard_patch(d, get_patch(d)->get_td_type());
        }
    } else if (ys == yl) { // horizontal merge
        std::vector<bool> keep(xl - xs + 1, false);
        for (size_t i = 0; i < keep.size(); ++i) {
            size_t x = xs + i;
            if (is_in_list(dest_list, x, ys)) keep[i] = true;
        }
        // Check for occupation conflicts
        // for (size_t i = 0; i < keep.size(); ++i) {
        //     size_t patch_id = get_patch_id(xs + i, ys);
        //     if (!keep[i] && get_patch(patch_id)->occupied()) {
        //         fmt::println("ERROR: Cannot merge - patch at ({}, {}) with ID {} is already occupied", xs + i, ys, patch_id);
        //         fmt::println("Aborting n_to_n due to occupied patches in the path");
        //         return;
        //     }
        // }
        for (size_t i = 0; i < keep.size(); ++i) {
            size_t patch_id = get_patch_id(xs + i, ys);
            merge_list.push_back(patch_id);
            if (keep[i]) keep_list.push_back(patch_id);
            else discard_list.push_back(patch_id);
        }
        merge_patches(merge_list, {get_patch(merge_list.front())->get_lr_type()});
        split_patches(merge_list);
        for (auto d : discard_list) {
            discard_patch(d, get_patch(d)->get_lr_type());
        }
    } else {
        fmt::println("n_to_n: Not yet developed for non-aligned cases");
        return;
    }
    fmt::println("n to n");
    print_grid();
    print_occupied();
}

void LatticeSurgery::print_occupied(){
    fmt::println("");
    fmt::println("LS Occupied Status:");
    for(size_t y=0; y<get_grid_cols(); y++){
        for(size_t x=0; x<get_grid_rows(); x++){
            fmt::print("{} ", (int) get_patch(x,y)->occupied());
        }
        fmt::print("\n");
    }
    fmt::println("");
};

// void LatticeSurgery::one_to_n(std::pair<size_t,size_t> start_patch, std::vector<std::pair<size_t,size_t>>& patch_list){
//     // Handle empty patch_list case
//     if (patch_list.empty() || (patch_list.size() == 1 && patch_list[0] == start_patch)) {
//         return;
//     }
    
//     auto [xs, ys]=start_patch;
//     auto [xl, yl]=start_patch;
//     std::vector<size_t> merge_list;
//     std::vector<size_t> discard_list;
//     for(auto [x,y]: patch_list){
//         if(x<xs) xs = x;
//         else if(x>xl) xl = x;

//         if(y<ys) ys=y;
//         else if(y>yl) yl=y;
//     }
//     if(xs == xl){ // |
//         std::vector<bool> check_discard(yl-ys+1, true);
//         for(auto [_, y]: patch_list) check_discard[y-ys] = false;
        
//         // Check if all patches on the path (except START) are unoccupied
//         for(size_t i=0; i<check_discard.size(); i++){
//             // Skip occupied patches that are our destinations
//             if(i == start_patch.second-ys) continue;
            
//             // Check if the patch is occupied
//             size_t patch_id = get_patch_id(xs, ys+i);
//             if(get_patch(patch_id)->occupied()) {
//                 fmt::println("ERROR: Cannot merge - patch at ({}, {}) with ID {} is already occupied", 
//                              xs, ys+i, patch_id);
//                 fmt::println("Aborting one_to_n due to occupied patches in the path");
//                 return;
//             }
//         }
        
        
//         fmt::println("1ton dest size {}", patch_list.size());
//         fmt::println("1ton merge size {}", check_discard.size());
        
//         for(size_t i=0; i<check_discard.size(); i++){
//             merge_list.emplace_back(get_patch_id(xs, ys+i));
//             if(check_discard[i]) discard_list.emplace_back(get_patch_id(xs, ys+i));
//         }
//         fmt::println("1ton discard size {}", discard_list.size());
//         merge_patches(merge_list, {get_patch(merge_list.front())->get_td_type()});
//         split_patches(merge_list);
//         for(auto d: discard_list){
//             discard_patch(d, get_patch(d)->get_td_type());
//         }
//     }
//     else if(ys == yl){ // <->
//         std::vector<bool> check_discard(xl-xs+1, true);
//         for(auto [x, _]: patch_list) check_discard[x-xs] = false;
        
//         // Check if all patches on the path (except START) are unoccupied
//         for(size_t i=0; i<check_discard.size(); i++){
//             // Skip occupied patches that are our destinations
//             if(i == start_patch.first-xs) continue;
            
//             // Check if the patch is occupied
//             size_t patch_id = get_patch_id(xs+i, ys);
//             if(get_patch(patch_id)->occupied()) {
//                 fmt::println("ERROR: Cannot merge - patch at ({}, {}) with ID {} is already occupied", 
//                              xs+i, ys, patch_id);
//                 fmt::println("Aborting one_to_n due to occupied patches in the path");
//                 return;
//             }
//         }
        
        
//         for(size_t i=0; i<check_discard.size(); i++){
//             merge_list.emplace_back(get_patch_id(xs+i, ys));
//             if(check_discard[i]) discard_list.emplace_back(get_patch_id(xs+i, ys));
//         }
//         merge_patches(merge_list, {get_patch(merge_list.front())->get_lr_type()});
//         split_patches(merge_list);
//         for(auto d: discard_list){
//             discard_patch(d, get_patch(d)->get_lr_type());
//         }
//     }
//     else{
//         fmt::println("Not yet develop");
//     }
//     fmt::println("one to n");
//     print_grid();
//     print_occupied();

// };

// void LatticeSurgery::n_to_one(std::vector<std::pair<size_t,size_t>>& patch_list, std::pair<size_t,size_t> dest_patch){
//     // Handle empty patch_list case
//     if (patch_list.empty() || (patch_list.size() == 1 && patch_list[0] == dest_patch)) {
//         return;
//     }
    
//     auto [xs, ys]=dest_patch;
//     auto [xl, yl]=dest_patch;
//     std::vector<size_t> merge_list;
//     std::vector<size_t> discard_list;
//     for(auto [x,y]: patch_list){
//         if(x<xs) xs = x;
//         else if(x>xl) xl = x;

//         if(y<ys) ys=y;
//         else if(y>yl) yl=y;
//     }
//     if(xs == xl){ // |
//         std::vector<bool> check_discard(yl-ys+1, true);
//         check_discard[dest_patch.second-ys] = false;

//         std::vector<bool> check_occupied(yl-ys+1, true);
//         for(auto [_, y]: patch_list) check_occupied[y-ys] = false;
        
//         // Check if all patches on the path (except START) are unoccupied
//         for(size_t i=0; i<check_discard.size(); i++){
//             // Skip if this is our destination patch
//             if(!check_occupied[i] || i == dest_patch.second-ys) continue;
            
//             // Get the patch ID
//             size_t patch_id = get_patch_id(xs, ys+i);
            
//             // If it's not a source patch and it's occupied, we can't merge
//             if(check_discard[i] && get_patch(patch_id)->occupied()) {
//                 fmt::println("ERROR: Cannot merge - patch at ({}, {}) with ID {} is already occupied", 
//                              xs, ys+i, patch_id);
//                 fmt::println("Aborting n_to_one due to occupied patches in the path");
//                 return;
//             }
//         }
        
        
//         for(size_t i=0; i<check_discard.size(); i++){
//             merge_list.emplace_back(get_patch_id(xs, ys+i));
//             if(check_discard[i]) discard_list.emplace_back(get_patch_id(xs, ys+i));
//         }
//         merge_patches(merge_list, {get_patch(merge_list.front())->get_td_type()});
//         split_patches(merge_list);  
//         for(auto d: discard_list){
//             discard_patch(d, get_patch(d)->get_td_type());
//         }
//     }
//     else if(ys == yl){ // <->
//         std::vector<bool> check_discard(xl-xs+1, true);
//         check_discard[dest_patch.first-xs] = false;

//         std::vector<bool> check_occupied(xl-xs+1, true);
//         for(auto [x, _]: patch_list) check_occupied[x-xs] = false;
        
//         // Check if all patches on the path (except source and destination) are unoccupied
//         for(size_t i=0; i<check_discard.size(); i++){
//             // Skip if this is our destination patch
//             if(!check_occupied[i] || i == dest_patch.first-xs) continue;
            
//             // Get the patch ID
//             size_t patch_id = get_patch_id(xs+i, ys);
            
//             // If it's not a source patch and it's occupied, we can't merge
//             if(check_discard[i] && get_patch(patch_id)->occupied()) {
//                 fmt::println("ERROR: Cannot merge - patch at ({}, {}) with ID {} is already occupied", 
//                              xs+i, ys, patch_id);
//                 fmt::println("Aborting n_to_one due to occupied patches in the path");
//                 return;
//             }
//         }
        
        
//         for(size_t i=0; i<check_discard.size(); i++){
//             merge_list.emplace_back(get_patch_id(xs+i, ys));
//             if(check_discard[i]) discard_list.emplace_back(get_patch_id(xs+i, ys));
//         }
//         merge_patches(merge_list, {get_patch(merge_list.front())->get_lr_type()});
//         split_patches(merge_list);
//         for(auto d: discard_list){
//             discard_patch(d, get_patch(d)->get_lr_type());
//         }
//     }
//     else{
//         fmt::println("Not yet develop");
//     }
//     fmt::println("n to one");
//     print_grid();
//     print_occupied();
// };


} // namespace qsyn::latticesurgery