/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Placer member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./placer.hpp"

#include <cassert>
#include <random>

#include "./duostra.hpp"
#include "device/device.hpp"
#include "duostra/duostra_def.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/util.hpp"

namespace qsyn::duostra {

/**
 * @brief Get the Placer object
 *
 * @return unique_ptr<BasePlacer>
 */
std::unique_ptr<BasePlacer> get_placer(PlacerType type) {
    switch (type) {
        case PlacerType::naive:
            return std::make_unique<StaticPlacer>();
        case PlacerType::random:
            return std::make_unique<RandomPlacer>();
        case PlacerType::dfs:
            return std::make_unique<DFSPlacer>();
        case PlacerType::qmdla:
            return std::make_unique<QMDLAPlacer>();
            break;
    }
    return nullptr;
}

// SECTION - Class BasePlacer Member Functions

/**
 * @brief Place and assign logical qubit
 *
 * @param device
 */
std::vector<QubitIdType> BasePlacer::place_and_assign(
    Device& device,
    const std::vector<QubitIdType>& qubit_priority,
    const std::vector<std::vector<size_t>>& qpi) {
    auto assign = _place(device, qubit_priority, qpi);
    device.place(assign);
    return assign;
}

// SECTION - Class RandomPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
std::vector<QubitIdType> RandomPlacer::_place(
    Device& device,
    const std::vector<QubitIdType>& /*unused*/,
    const std::vector<std::vector<size_t>>& /*unused*/) const {
    std::vector<QubitIdType> assign;
    for (size_t i = 0; i < device.get_num_qubits(); ++i)
        assign.emplace_back(i);

    std::ranges::shuffle(assign, std::default_random_engine(std::random_device{}()));
    return assign;
}

// SECTION - Class StaticPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
std::vector<QubitIdType> StaticPlacer::_place(
    Device& device,
    const std::vector<QubitIdType>& /*unused*/,
    const std::vector<std::vector<size_t>>& /*unused*/) const {
    std::vector<QubitIdType> assign;
    for (size_t i = 0; i < device.get_num_qubits(); ++i)
        assign.emplace_back(i);
    return assign;
}

// SECTION - Class DFSPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
std::vector<QubitIdType> DFSPlacer::_place(
    Device& device,
    const std::vector<QubitIdType>& /*unused*/,
    const std::vector<std::vector<size_t>>& /*unused*/) const {
    std::vector<QubitIdType> assign;
    std::vector<bool> qubit_mark(device.get_num_qubits(), false);
    _dfs_device(0, device, assign, qubit_mark);
    assert(assign.size() == device.get_num_qubits());
    return assign;
}

/**
 * @brief Depth-first search the device
 *
 * @param current
 * @param device
 * @param assign
 * @param qubitMark
 */
void DFSPlacer::_dfs_device(QubitIdType current, Device& device, std::vector<QubitIdType>& assign, std::vector<bool>& qubit_marks) const {
    if (qubit_marks[current]) {
        fmt::println("{}", current);
    }
    assert(!qubit_marks[current]);
    qubit_marks[current] = true;
    assign.emplace_back(current);

    auto const& q = device.get_physical_qubit(current);
    std::vector<QubitIdType> adjacency_waitlist;

    for (auto& adj : q.get_adjacencies()) {
        // already marked
        if (qubit_marks[adj])
            continue;
        assert(!q.get_adjacencies().empty());
        // corner
        if (q.get_adjacencies().size() == 1)
            _dfs_device(adj, device, assign, qubit_marks);
        else
            adjacency_waitlist.emplace_back(adj);
    }

    for (size_t i = 0; i < adjacency_waitlist.size(); ++i) {
        auto adj = adjacency_waitlist[i];
        if (qubit_marks[adj])
            continue;
        _dfs_device(adj, device, assign, qubit_marks);
    }
}

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
std::vector<QubitIdType> QMDLAPlacer::_place(
    Device& device,
    const std::vector<QubitIdType>& qubit_priority_input,
    const std::vector<std::vector<size_t>>& qpi) const {
    
    // Initialize mappings with invalid values
    std::vector<QubitIdType> M1(device.get_num_qubits(), -1);  // logical -> physical
    std::vector<QubitIdType> M2(device.get_num_qubits(), -1);  // physical -> logical
    
    // Keep track of all possible mappings
    std::vector<std::vector<QubitIdType>> Maps;
    Maps.push_back(M1);  // Start with empty mapping

    // Calculate Physical Coupling Score (PCS)
    std::vector<size_t> pcs(device.get_num_qubits(), 0);
    for (size_t i = 0; i < device.get_num_qubits(); ++i) {
        const auto& q = device.get_physical_qubit(i);
        size_t score = 0;
        score += q.get_adjacencies().size();  // 1-hop neighbors
        for (auto adj : q.get_adjacencies()) {
            const auto& adj_q = device.get_physical_qubit(adj);
            for (auto second_adj : adj_q.get_adjacencies()) {
                if (second_adj != i && 
                    std::find(q.get_adjacencies().begin(), 
                            q.get_adjacencies().end(), 
                            second_adj) == q.get_adjacencies().end()) {
                    score++; // 2-hop neighbors
                }
            }
        }
        pcs[i] = score;
    }

    // Make a copy of priority list that we can modify
    std::vector<QubitIdType> qubit_priority = qubit_priority_input;

    // For each qubit in priority order
    while (!qubit_priority.empty()) {
        std::vector<std::vector<QubitIdType>> new_Maps;
        
        for (auto& M : Maps) {  // For each current mapping possibility
            // Get highest priority unmapped qubit
            QubitIdType curr_qubit = qubit_priority[0];
            
            // Get logical neighbors of current qubit
            std::vector<QubitIdType> curr_neighbors;
            for (size_t i = 0; i < qpi[curr_qubit].size(); i++) {
                if (qpi[curr_qubit][i] > 0) {
                    curr_neighbors.push_back(i);
                }
            }

            // Check if current qubit has any mapped neighbors
            bool has_mapped_neighbor = false;
            for (auto neighbor : curr_neighbors) {
                if (M[neighbor] != -1) {
                    has_mapped_neighbor = true;
                    break;
                }
            }

            if (!has_mapped_neighbor) {
                // Case 1: No mapped neighbors - use PCS
                QubitIdType best_physical = -1;
                size_t best_pcs = 0;
                
                // Find unassigned physical qubit with highest PCS
                for (size_t p = 0; p < device.get_num_qubits(); p++) {
                    bool is_used = false;
                    for (size_t i = 0; i < device.get_num_qubits(); i++) {
                        if (M[i] == (QubitIdType)p) {
                            is_used = true;
                            break;
                        }
                    }
                    if (!is_used && pcs[p] > best_pcs) {
                        best_pcs = pcs[p];
                        best_physical = p;
                    }
                }
                
                if (best_physical != -1) {
                    auto M_new = M;
                    M_new[curr_qubit] = best_physical;
                    new_Maps.push_back(M_new);
                }
                
            } else {
                // Case 2: Has mapped neighbors - use QBN
                std::vector<std::pair<QubitIdType, size_t>> candidates;
                
                // For each unmapped physical qubit
                for (size_t p = 0; p < device.get_num_qubits(); p++) {
                    bool is_used = false;
                    for (size_t i = 0; i < device.get_num_qubits(); i++) {
                        if (M[i] == (QubitIdType)p) {
                            is_used = true;
                            break;
                        }
                    }
                    if (is_used) continue;
                    
                    // Calculate QBN score
                    size_t qbn_score = 0;
                    const auto& phys_q = device.get_physical_qubit(p);
                    
                    for (auto neighbor : curr_neighbors) {
                        if (M[neighbor] != -1) {  // if neighbor is mapped
                            if (std::find(phys_q.get_adjacencies().begin(),
                                        phys_q.get_adjacencies().end(),
                                        M[neighbor]) != phys_q.get_adjacencies().end()) {
                                qbn_score += qpi[curr_qubit][neighbor];
                            }
                        }
                    }
                    
                    if (qbn_score > 0) {
                        candidates.emplace_back(p, qbn_score);
                    }
                }
                
                // Sort candidates by QBN score
                std::sort(candidates.begin(), candidates.end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
                
                if (!candidates.empty()) {
                    // Get all candidates with the highest score
                    size_t best_score = candidates[0].second;
                    for (const auto& [p, score] : candidates) {
                        if (score == best_score) {
                            auto M_new = M;
                            M_new[curr_qubit] = p;
                            new_Maps.push_back(M_new);
                        } else {
                            break;
                        }
                    }
                }
            }
        }
        
        // Update Maps with new mappings
        if (!new_Maps.empty()) {
            Maps = new_Maps;
        }
        
        // Remove current qubit from priority list
        qubit_priority.erase(qubit_priority.begin());
    }
    
    // Choose the first valid mapping
    for (const auto& M : Maps) {
        bool is_valid = true;
        std::vector<bool> used(device.get_num_qubits(), false);
        
        // Check if mapping is complete and valid
        for (size_t i = 0; i < M.size(); i++) {
            if (M[i] == -1 || M[i] >= device.get_num_qubits() || used[M[i]]) {
                is_valid = false;
                break;
            }
            used[M[i]] = true;
        }
        
        if (is_valid) {
            return M;
        }
    }
    
    // Fallback to identity mapping if no valid mapping found
    std::vector<QubitIdType> fallback(device.get_num_qubits());
    std::iota(fallback.begin(), fallback.end(), 0);
    return fallback;
}

}  // namespace qsyn::duostra
