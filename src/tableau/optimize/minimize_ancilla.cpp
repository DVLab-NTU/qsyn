/**
 * @file minimize_ancilla.cpp
 * @brief Implementation of constraint graph and ancilla minimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "../tableau_optimization.hpp"
#include "../classical_tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <sul/dynamic_bitset.hpp>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace qsyn::experimental {

// For diagonal phase columns (Z/I only), commuting through a Hadamard gadget (CCC)
// only swaps the Z-support between the gadget's (reference, ancilla) qubits.
void swap_gadget_phase_slots(PauliRotation& r, size_t reference, size_t ancilla) {
    if (reference == ancilla) {
        return;
    }
    if (!r.is_diagonal()) {
        return;
    }

    std::vector<Pauli> pv(r.n_qubits(), Pauli::i);
    for (size_t q = 0; q < r.n_qubits(); ++q) {
        pv[q] = r.get_pauli_type(q);
    }
    std::swap(pv[reference], pv[ancilla]);
    dvlab::Phase const ph = r.phase();
    r                    = PauliRotation(pv.begin(), pv.end(), ph);

    // Refresh CZ flag: CZ only if diagonal, zero phase, and exactly two Zs.
    if (r.phase() != dvlab::Phase(0)) {
        r.set_is_CZ(false);
        return;
    }
    size_t z_count = 0;
    for (size_t q = 0; q < r.n_qubits(); ++q) {
        if (r.get_pauli_type(q) == Pauli::z) ++z_count;
    }
    r.set_is_CZ(z_count == 2);
}


// Forward declaration for hadamard_degadgetize (defined in hadamard_gadgetize.cpp)
void hadamard_degadgetize(Tableau& tableau, size_t ccc_index, size_t pmc_index);

/**
 * @brief Export all H-gadget pairs (CCC-PMC pairs) from a tableau.
 *        Collects all CCCs and pairs them with PMCs.
 *        Assumes all pairs are already valid and checked.
 *
 * @param tableau The tableau to examine
 * @return Vector of HadamardGadgetPair structures containing pairing information
 */
std::vector<ConstraintGraph::HadamardGadgetPair> export_hadamard_gadget_pairs(Tableau& tableau) {
    std::vector<ConstraintGraph::HadamardGadgetPair> pairs;
    
    // Collect all CCCs and PMCs with their indices
    std::vector<std::pair<size_t, ClassicalControlTableau const*>> ccc_list;
    std::vector<std::pair<size_t, ClassicalControlTableau const*>> pmc_list;
    
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct) {
            if (cct->is_gadget()) {
                ccc_list.emplace_back(idx, cct);
            } else if (cct->is_classical_control()) {
                pmc_list.emplace_back(idx, cct);
            }

        }
    }
    
    // Pair CCCs with PMCs by order (first CCC with first PMC, etc.)
    // Assumes all pairs are valid and counts match
    for (size_t i = 0; i < ccc_list.size(); ++i) {
        auto const& [ccc_idx, ccc_ptr] = ccc_list[i];
        auto const& [pmc_idx, pmc_ptr] = pmc_list[i];
        
        ConstraintGraph::HadamardGadgetPair pair;
        pair.ccc_index = ccc_idx;
        pair.pmc_index = pmc_idx;
        pair.ancilla_qubit = ccc_ptr->ancilla_qubit();
        pair.reference_qubit = ccc_ptr->reference_qubit();
        
        pairs.push_back(pair);
        
        // Log the paired Hadamard gadget
        if (pair.reference_qubit.has_value()) {
            spdlog::info("Hadamard Gadget Pair: CCC[{}] <-> PMC[{}] | ancilla={}, reference={}",
                       ccc_idx, pmc_idx, pair.ancilla_qubit, pair.reference_qubit.value());
        } else {
            spdlog::info("Hadamard Gadget Pair: CCC[{}] <-> PMC[{}] | ancilla={}, reference=N/A",
                       ccc_idx, pmc_idx, pair.ancilla_qubit);
        }
    }
    
    spdlog::info("Exported {} H-gadget pairs from tableau ({} CCCs, {} PMCs)", 
                 pairs.size(), ccc_list.size(), pmc_list.size());
    
    return pairs;
}

// ConstraintGraph implementation

// Unified vertex creation for gadgets
size_t ConstraintGraph::create_vertex(HadamardGadgetPair const& hg) {
    size_t vertex_id = vertices.size();  // Sequential ID: 0, 1, 2, ...
    vertices.emplace_back(VertexType::GADGET, vertex_id, hg);
    outgoing_edges.emplace_back();
    incoming_edges.emplace_back();
    return vertex_id;
}

// Unified vertex creation for PRs
size_t ConstraintGraph::create_vertex(PRInfo const& pr) {
    size_t vertex_id = vertices.size();  // Sequential ID: continues from gadgets
    vertices.emplace_back(VertexType::PR, vertex_id, pr);
    outgoing_edges.emplace_back();
    incoming_edges.emplace_back();
    return vertex_id;
}

void ConstraintGraph::add_edge(size_t u, size_t v) {
    if (u >= outgoing_edges.size() || v >= outgoing_edges.size()) {
        spdlog::error("ConstraintGraph::add_edge: Invalid vertex indices {} -> {}", u, v);
        return;
    }
    // Check if edge already exists
    for (size_t i = 0; i < outgoing_edges[u].size(); ++i) {
        if (outgoing_edges[u][i] == v) {
            return;  // Edge already exists
        }
    }
    // Add to outgoing edges of u and incoming edges of v
    outgoing_edges[u].push_back(v);
    incoming_edges[v].push_back(u);
}

std::optional<std::vector<size_t>> ConstraintGraph::topological_sort() const {
    std::vector<size_t> result;
    std::vector<bool> visited(vertices.size(), false);
    
    std::function<void(size_t)> dfs = [&](size_t v) {
        if (v >= vertices.size()) {
            return;
        }
        
        if (visited[v]) return;
        visited[v] = true;
        
        // Process all outgoing edges, including to removed vertices
        for (size_t u : outgoing_edges[v]) {
            if (u >= vertices.size()) {
                continue;
            }
            
            // Removed vertices can only be reached from gadget vertices
            // and can only reach gadget vertices
            // So we only traverse edges involving removed vertices if both are gadgets
            if (vertices[u].removed || vertices[v].removed) {
                // Both must be gadgets (removed vertices are always gadgets)
                if (vertices[v].type == VertexType::GADGET && 
                    vertices[u].type == VertexType::GADGET) {
                    if (!visited[u]) {
                        dfs(u);
                    }
                }
            } else {
                // Normal traversal for non-removed vertices
                if (!visited[u]) {
                    dfs(u);
                }
            }
        }
        
        // Add vertex to result after processing all descendants (including removed ones)
        result.push_back(v);
    };
    
    // Start DFS from all vertices (including removed ones)
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (!visited[i]) {
            dfs(i);
        }
    }
    
    std::reverse(result.begin(), result.end());
    return result;
}

size_t ConstraintGraph::break_cycles() {
    // Count already removed gadgets
    size_t removed_count = 0;
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].removed && vertices[i].type == VertexType::GADGET) {
            removed_count++;
        }
    }
    
    // Store all cycles: each cycle is a vector of vertex indices
    std::vector<std::vector<size_t>> all_cycles;
    // Map from gadget vertex index to set of cycle indices it appears in
    std::unordered_map<size_t, std::set<size_t>> gadget_to_cycles;
    
    // Track visited vertices for cycle detection
    enum class Color { WHITE, GRAY, BLACK };
    std::vector<Color> color(vertices.size(), Color::WHITE);
    std::vector<size_t> current_path;
    std::unordered_map<size_t, size_t> path_index;  // vertex -> index in current_path
    
    // Helper function to extract cycle from path when back edge is found
    auto extract_cycle = [&](size_t cycle_start_idx) {
        // Extract cycle from current_path[cycle_start_idx] to end
        std::vector<size_t> cycle;
        for (size_t i = cycle_start_idx; i < current_path.size(); ++i) {
            cycle.push_back(current_path[i]);
        }
        cycle.push_back(current_path[cycle_start_idx]);  // Close the cycle
        
        // Only count gadgets in this cycle and track which cycles each gadget appears in
        size_t cycle_id = all_cycles.size();
        all_cycles.push_back(cycle);
        
        for (size_t v : cycle) {
            if (v < vertices.size() && !vertices[v].removed && 
                vertices[v].type == VertexType::GADGET) {
                gadget_to_cycles[v].insert(cycle_id);
            }
        }
    };
    
    // DFS function to find cycles starting from a given vertex
    std::function<void(size_t)> dfs = [&](size_t v) {
        // Skip removed vertices
        if (v >= vertices.size() || vertices[v].removed) {
            return;
        }
        
        color[v] = Color::GRAY;
        path_index[v] = current_path.size();
        current_path.push_back(v);
        
        for (size_t u : outgoing_edges[v]) {
            // Skip removed target vertices
            if (u >= vertices.size() || vertices[u].removed) {
                continue;
            }
            
            if (color[u] == Color::GRAY) {
                // Back edge found - cycle detected
                // Extract cycle from u's position in path to current position
                extract_cycle(path_index[u]);
            } else if (color[u] == Color::WHITE) {
                dfs(u);
            }
        }
        
        current_path.pop_back();
        path_index.erase(v);
        color[v] = Color::BLACK;
    };
    
    // Apply one DFS to extract all cycles
    // Start DFS from each unremoved HadamardGadget vertex
    // Only start from vertices that are still WHITE (unvisited)
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].removed || vertices[i].type != VertexType::GADGET) {
            continue;
        }
        
        // Only start DFS from vertices that are still WHITE (unvisited)
        // After a DFS, all reachable vertices will be BLACK, so we skip them
        if (color[i] == Color::WHITE) {
            // Clear path tracking for this new DFS (but keep colors)
            current_path.clear();
            path_index.clear();
            dfs(i);
        }
    }
    
    // Track which cycles are still active (not yet broken)
    std::set<size_t> active_cycles;
    for (size_t i = 0; i < all_cycles.size(); ++i) {
        active_cycles.insert(i);
    }
    
    // Helper function to remove a gadget and all cycles containing it
    auto remove_gadget_and_cycles = [&](size_t gadget_idx) {
        // Remove the gadget
        vertices[gadget_idx].removed = true;
        removed_count++;
        
        // Remove all cycles containing this gadget from active cycles
        auto cycles_to_remove = gadget_to_cycles[gadget_idx];
        for (size_t cycle_id : cycles_to_remove) {
            active_cycles.erase(cycle_id);
            // Remove this cycle from all gadgets that appear in it
            for (size_t v : all_cycles[cycle_id]) {
                if (v < vertices.size() && vertices[v].type == VertexType::GADGET) {
                    gadget_to_cycles[v].erase(cycle_id);
                }
            }
        }
        
        // Remove the gadget from the map
        gadget_to_cycles.erase(gadget_idx);
    };
    
    // First pass: Remove gadgets in cycles with length == 1 (crucial gadgets)
    std::set<size_t> cycles_to_check = active_cycles;  // Copy to iterate safely
    for (size_t cycle_id : cycles_to_check) {
        // Skip if cycle is no longer active
        if (active_cycles.count(cycle_id) == 0) {
            continue;
        }
        
        // Check if cycle has length 1 (only one gadget)
        if (all_cycles[cycle_id].size() == 1) {
            size_t gadget_idx = all_cycles[cycle_id][0];
            // Verify it's a valid, non-removed gadget
            if (gadget_idx < vertices.size() && !vertices[gadget_idx].removed) {
                remove_gadget_and_cycles(gadget_idx);
            }
        }
    }
    
    // Second pass: Greedily remove gadgets with most appearances until no cycles remain
    while (!active_cycles.empty()) {
        // Find the gadget that appears in the most active cycles
        size_t max_count = 0;
        size_t vertex_to_remove = std::numeric_limits<size_t>::max();
        
        for (auto const& [gadget_idx, cycle_set] : gadget_to_cycles) {
            // Count how many active cycles this gadget appears in
            size_t active_count = 0;
            for (size_t cycle_id : cycle_set) {
                if (active_cycles.count(cycle_id) > 0) {
                    active_count++;
                }
            }
            
            if (active_count > max_count) {
                max_count = active_count;
                vertex_to_remove = gadget_idx;
            }
        }
        
        // If no gadget found, break to avoid infinite loop
        if (vertex_to_remove == std::numeric_limits<size_t>::max()) {
            break;
        }
        
        // Remove the gadget
        remove_gadget_and_cycles(vertex_to_remove);
    }
    
    return removed_count;
}

ConstraintGraph build_constraint_graph(Tableau& tableau,
                                       std::optional<std::string> const& export_path) {
    ConstraintGraph graph;
    
    // Extract gadgets, create vertices, and establish CCC ordering edges
    auto gadgets = export_hadamard_gadget_pairs(tableau);
    
    std::unordered_map<size_t, size_t> gadget_id_to_vertex;
    std::unordered_map<size_t, std::vector<size_t>> reference_gadgets;  // Group gadgets by reference qubit
    
    // Create all gadget vertices first (will get IDs 0, 1, 2, ..., num_gadgets-1)
    for (size_t g_idx = 0; g_idx < gadgets.size(); ++g_idx) {
        auto const& gadget = gadgets[g_idx];
        gadget_id_to_vertex[g_idx] = graph.create_vertex(gadget);
        
        // Group gadgets by reference qubit for ordering
        if (gadget.reference_qubit.has_value()) {
            reference_gadgets[gadget.reference_qubit.value()].push_back(g_idx);
        }
    }

    size_t ccc_ordering_edges = 0;
    for (auto& [ref_qubit, gadget_indices] : reference_gadgets) {
        // Sort by CCC index - smaller index comes before bigger index
        std::sort(gadget_indices.begin(), gadget_indices.end(),
                  [&](size_t a, size_t b) {
                      return gadgets[a].ccc_index < gadgets[b].ccc_index;
                  });
        
        // Add edges: CCC[i] -> CCC[i+1] for consecutive gadgets with same reference qubit
        for (size_t i = 0; i + 1 < gadget_indices.size(); ++i) {
            size_t g1_idx = gadget_indices[i];
            size_t g2_idx = gadget_indices[i + 1];
            graph.add_edge(gadget_id_to_vertex[g1_idx], gadget_id_to_vertex[g2_idx]);
            ccc_ordering_edges++;
        }
    }

    // Collect all phase columns (every PauliRotation in the PR block). This includes
    // T/S/Z/Sdg rotations, CZ (Z on two qubits), and single-qubit Z rotations.
    std::vector<ConstraintGraph::PRInfo> all_prs;
    size_t global_pr_counter = 0;
    
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[idx]);
        if (!pr_vec) continue;
        
        for (size_t pr_idx = 0; pr_idx < pr_vec->size(); ++pr_idx) {
            ConstraintGraph::PRInfo pr_info = {idx, pr_idx, global_pr_counter, &(*pr_vec)[pr_idx]};
            all_prs.push_back(pr_info);
            ++global_pr_counter;
        }
    }
    
    // Export constraint graph data to a text file if path is provided
    if (export_path.has_value()) {
        std::ofstream out(*export_path);
        if (!out) {
            spdlog::warn("build_constraint_graph: could not open export file '{}'", *export_path);
        } else {
            size_t const qubit_count = tableau.n_qubits();
            size_t const ancilla_count = tableau.n_ancilla();
            size_t const no_ref = std::numeric_limits<size_t>::max();

            out << "qubit_count: " << qubit_count << "\n";
            out << "ancilla_count: " << ancilla_count << "\n";
            out << "gadget\n";
            for (size_t g_idx = 0; g_idx < gadgets.size(); ++g_idx) {
                auto const& gadget = gadgets[g_idx];
                size_t const a = gadget.reference_qubit.value_or(no_ref);
                size_t const b = gadget.ancilla_qubit;
                out << g_idx << " " << a << " " << b << "\n";
            }

            // Ordered operation stream for deterministic SAT-side status propagation.
            // Source it directly from the properized G/C segment:
            // - when a gadget CCT is encountered: emit "gadget gid a b"
            // - when a Clifford ST is encountered: extract Clifford ops and emit each CX as "cx c t"
            out << "ops\n";
            std::unordered_map<size_t, size_t> ccc_index_to_gid;
            ccc_index_to_gid.reserve(gadgets.size());
            for (size_t gid = 0; gid < gadgets.size(); ++gid) {
                ccc_index_to_gid[gadgets[gid].ccc_index] = gid;
            }

            // Skip the leading front Clifford (index 0), and stop once PR segment starts.
            for (size_t idx = 1; idx < tableau.size(); ++idx) {
                if (std::holds_alternative<std::vector<PauliRotation>>(tableau[idx])) {
                    break;
                }
                if (auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx])) {
                    if (!cct->is_gadget()) {
                        continue;
                    }
                    auto it = ccc_index_to_gid.find(idx);
                    if (it == ccc_index_to_gid.end()) {
                        spdlog::warn("build_constraint_graph: gadget CCT at index {} has no gid mapping", idx);
                        continue;
                    }
                    size_t const gid = it->second;
                    size_t const a = cct->reference_qubit();
                    size_t const b = cct->ancilla_qubit();
                    out << "gadget " << gid << " " << a << " " << b << "\n";
                    continue;
                }

                auto const* st = std::get_if<StabilizerTableau>(&tableau[idx]);
                if (!st) {
                    continue;
                }
                auto const ops = extract_clifford_operators(*st);
                for (auto const& [type, qubits] : ops) {
                    if (type == CliffordOperatorType::cx) {
                        out << "cx " << qubits[0] << " " << qubits[1] << "\n";
                    }
                }
            }

            // Paulis: for each pauli, take the Z part of the bit string (first n chars),
            // output Z part only.
            out << "paulis\n";
            for (size_t i = 0; i < all_prs.size(); ++i) {
                auto const* pr = all_prs[i].pr_ptr;

                // Extract Z basis bits (length = qubit_count), then apply the permutation so that
                // position i gets the bit from original position permutation[i]. We always print
                // exactly qubit_count bits after the index.
                std::string z_original(qubit_count, '0');
                if (pr) {
                    std::string const full_bits = pr->to_bit_string();
                    // Z part is the first qubit_count characters (PauliProduct::to_bit_string
                    // formats as: Z-bits, space, X-bits, space, sign-bit).
                    if (full_bits.size() >= qubit_count) {
                        z_original = full_bits.substr(0, qubit_count);
                    } else {
                        // Fallback: copy what we have, pad with '0's.
                        std::copy(full_bits.begin(),
                                  full_bits.begin() + std::min(qubit_count, full_bits.size()),
                                  z_original.begin());
                    }
                }

                out << i + gadgets.size() << " " << z_original << "\n";
            }
            spdlog::info("build_constraint_graph: exported to '{}' ({} gadgets, {} paulis)",
                         *export_path, gadgets.size(), all_prs.size());
        }
    }
    
    // Create vertices for all PRs (will get IDs num_gadgets, num_gadgets+1, ..., num_gadgets+num_prs-1)
    std::unordered_map<size_t, size_t> pr_index_to_vertex;
    for (auto const& pr_info : all_prs) {
        pr_index_to_vertex[pr_info.global_pr_index] = graph.create_vertex(pr_info);
    }
    
    // Add PR-gadget edges (Constraints 2, 3, and 4) based on phase patterns
    size_t constraint2_edges = 0;  // PR after CCC (pattern 1,0)
    size_t constraint3_edges = 0;  // PR before CCC (pattern 0,1)
    size_t constraint4_cycles = 0; // Ungadgetizable (pattern 1,1)
    
    for (auto const& pr_info : all_prs) {
        auto const& pr = *pr_info.pr_ptr;
        size_t pr_vertex = pr_index_to_vertex[pr_info.global_pr_index];
        
        for (size_t g_idx = 0; g_idx < gadgets.size(); ++g_idx) {
            auto const& gadget = gadgets[g_idx];
            if (!gadget.reference_qubit.has_value()) continue;
            if (!gadget_id_to_vertex.count(g_idx)) continue;
            
            size_t ref_qubit = gadget.reference_qubit.value();
            size_t ancilla_qubit = gadget.ancilla_qubit;
            size_t gadget_vertex = gadget_id_to_vertex[g_idx];
            
            // Get phase pattern: (phase_on_ref, phase_on_ancilla)
            bool has_phase_ref = (pr.get_pauli_type(ref_qubit) == Pauli::z);
            bool has_phase_ancilla = (pr.get_pauli_type(ancilla_qubit) == Pauli::z);
            
            // Constraint 2: PR with pattern (1,0) should be after CCC
            if (has_phase_ref && !has_phase_ancilla) {
                graph.add_edge(gadget_vertex, pr_vertex);
                constraint2_edges++;
            }
            
            // Constraint 3: PR with pattern (0,1) should be before CCC
            if (!has_phase_ref && has_phase_ancilla) {
                graph.add_edge(pr_vertex, gadget_vertex);
                constraint3_edges++;
            }
            
            // Constraint 4: PR with pattern (1,1) creates a cycle (both constraint 2 and 3 triggered)
            // This makes the gadget ungadgetizable - mark it as removed immediately
            if (has_phase_ref && has_phase_ancilla) {
                if (!graph.vertices[gadget_vertex].removed) {
                    graph.vertices[gadget_vertex].removed = true;
                    constraint4_cycles++;
                }
            }
        }
    }

    spdlog::debug("=== Finished build_constraint_graph ===");
    return graph;
}

void print_constraint_graph_info(ConstraintGraph const& graph) {
    spdlog::info("=== Constraint Graph Information ===");
    
    // First, print all vertices
    spdlog::info("Vertices:");
    for (auto const& vertex : graph.vertices) {
        std::string removed_str = vertex.removed ? " [REMOVED]" : "";
        if (vertex.type == ConstraintGraph::VertexType::GADGET) {
            auto const& gadget = vertex.hadamard_gadget_pair;
            if (gadget.reference_qubit.has_value()) {
                spdlog::info("  Vertex {} (GADGET{}): ref_qubit={}, ancilla_qubit={}", 
                           vertex.id,
                           removed_str,
                           gadget.reference_qubit.value(), 
                           gadget.ancilla_qubit);
            } else {
                spdlog::info("  Vertex {} (GADGET{}): ref_qubit=<none>, ancilla_qubit={}", 
                           vertex.id,
                           removed_str,
                           gadget.ancilla_qubit);
            }
        } else if (vertex.type == ConstraintGraph::VertexType::PR) {
            auto const& pr_info = vertex.pr_info;
            if (pr_info.pr_ptr) {
                spdlog::info("  Vertex {} (PR{}): PR column = {}", 
                           vertex.id,
                           removed_str,
                           fmt::format("{}", *pr_info.pr_ptr));
            } else {
                spdlog::info("  Vertex {} (PR{}): PR column = <null>", 
                           vertex.id,
                           removed_str);
            }
        }
    }
    
    // Then, print edges for gadget vertices
    spdlog::info("\nEdges for GADGET vertices:");
    for (auto const& vertex : graph.vertices) {
        if (vertex.type == ConstraintGraph::VertexType::GADGET) {
            std::string removed_str = vertex.removed ? " [REMOVED]" : "";
            // Print incoming edges (vertices pointing to this gadget)
            if (!graph.incoming_edges[vertex.id].empty()) {
                spdlog::info("  Vertex {} (GADGET{}): incoming edges from: [{}]", 
                           vertex.id,
                           removed_str,
                           fmt::join(graph.incoming_edges[vertex.id], ", "));
            } else {
                spdlog::info("  Vertex {} (GADGET{}): incoming edges from: []", 
                           vertex.id,
                           removed_str);
            }
            
            // Print outgoing edges (vertices this gadget points to)
            if (!graph.outgoing_edges[vertex.id].empty()) {
                spdlog::info("  Vertex {} (GADGET{}): outgoing edges to: [{}]", 
                           vertex.id,
                           removed_str,
                           fmt::join(graph.outgoing_edges[vertex.id], ", "));
            } else {
                spdlog::info("  Vertex {} (GADGET{}): outgoing edges to: []", 
                           vertex.id,
                           removed_str);
            }
        }
    }
    
    spdlog::info("=== End Constraint Graph Information ===");
}



void reorder_n_degadgetize(Tableau& tableau) {
    spdlog::debug("=== Starting reorder_n_degadgetize ===");
    // Save original qubit count
    size_t original_n_qubits = tableau.n_qubits();
    // spdlog::debug("Original tableau: {:g}", tableau);
    auto structure_info = properize_for_degadgetization(tableau);
    // spdlog::debug("Properized tableau: {:g}", tableau);
    if (!structure_info.is_valid) {
        spdlog::error("Invalid circuit structure in reorder_n_degadgetize");
        return;
    }
    // spdlog::trace("Circuit after properize_for_degadgetization:\n{:b}", tableau);
    
    spdlog::debug("Building constraint graph...");
    ConstraintGraph graph = build_constraint_graph(tableau, "/home/ferayer/TODD/note/gadget_constraint_0320.txt");

    print_constraint_graph_info(graph);
    
    graph.break_cycles();
    size_t removed_count = std::count_if(graph.vertices.begin(), graph.vertices.end(),
                                        [](auto const& v) { return v.removed; });
    
    std::optional<std::vector<size_t>> topological_order_opt = graph.topological_sort();
    
    if (!topological_order_opt.has_value()) {
        spdlog::error("Topological sort failed - graph may still have cycles");
        return;
    }
    
    spdlog::debug("Topological order of vertices:");
    for (size_t i = 0; i < topological_order_opt.value().size(); ++i) {
        spdlog::debug("  Vertex {} (original index {})", i, topological_order_opt.value()[i]);
    }
    std::vector<size_t> const& topological_order = topological_order_opt.value();
    

    // Extract front StabilizerTableau
    StabilizerTableau front_st = *std::get_if<StabilizerTableau>(&tableau[0]);
    
    // Extract back StabilizerTableau
    StabilizerTableau back_st = *std::get_if<StabilizerTableau>(&tableau[tableau.size() - 1]);
    
    // Get gadgets to map vertex IDs back to CCC indices
    auto gadgets = export_hadamard_gadget_pairs(tableau);
    size_t num_gadgets = gadgets.size();
    
    // Rebuild PR mapping: vertex_id -> (tableau_idx, pr_vector_idx, PR)
    std::unordered_map<size_t, ConstraintGraph::PRInfo> pr_vertex_to_info;
    size_t global_pr_counter = 0;
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[idx]);
        if (!pr_vec) continue;
        
        for (size_t pr_idx = 0; pr_idx < pr_vec->size(); ++pr_idx) {
            size_t vertex_id = num_gadgets + global_pr_counter;
            ConstraintGraph::PRInfo pr_info = {idx, pr_idx, global_pr_counter, &(*pr_vec)[pr_idx]};
            pr_vertex_to_info[vertex_id] = pr_info;
            ++global_pr_counter;
        }
    }
    spdlog::debug("Mapped {} PR columns to vertex IDs", global_pr_counter);
    
    // Collect PMCs (they stay at the end, in original order)
    std::vector<ClassicalControlTableau> pmcs;
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct && cct->is_classical_control()) {
            pmcs.push_back(*cct);
        }
    }
    spdlog::debug("Collected {} PMCs", pmcs.size());
    
    // Step 1: Create temporary subtableaux vectors in one iteration
    // Collect all CCCs and PR columns in topological order (including removed gadgets)
    std::vector<ClassicalControlTableau> temp_cccs;
    std::unordered_map<size_t, std::vector<PauliRotation>> pr_columns;
    
    for (size_t vertex_id : topological_order) {
        if (vertex_id < num_gadgets) {
            // This is a gadget vertex (CCC) - collect ALL gadgets (removed and non-removed)
            size_t ccc_idx = gadgets[vertex_id].ccc_index;
            if (ccc_idx < tableau.size()) {
                auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[ccc_idx]);
                if (cct && cct->is_gadget()) {
                    temp_cccs.push_back(*cct);
                }
            }
        } else {
            // This is a PR vertex
            if (pr_vertex_to_info.count(vertex_id) > 0) {
                auto const& pr_info = pr_vertex_to_info[vertex_id];
                auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[pr_info.tableau_index]);
                if (pr_vec && pr_info.pr_vector_index < pr_vec->size()) {
                    // Each PR column is saved as its own vector with one element
                    pr_columns[vertex_id] = {(*pr_vec)[pr_info.pr_vector_index]};
                }
            }
        }
    }
    spdlog::debug("Collected {} CCCs and {} PR columns", temp_cccs.size(), pr_columns.size());
    
    spdlog::debug("Assembling new tableau in topological order...");
    Tableau new_tableau{tableau.n_qubits()};
    new_tableau.set_n_ancilla(tableau.n_ancilla());
    if (!tableau.is_empty()) {
        tableau.erase(tableau.begin(), tableau.end());
    }
    new_tableau.push_back(std::move(front_st));
    // Track non-removed gadget vertices and their corresponding CCC indices in the new tableau
    std::vector<size_t> degadgetize_gadgets;
    
    // Step 2: Process topological_order sequentially and assemble according to the order
    for (size_t vertex_id : topological_order) {
        if (vertex_id < num_gadgets) {
            // This is a gadget vertex (CCC)
            if (!temp_cccs.empty()) {
                new_tableau.push_back(std::move(temp_cccs[0]));
                temp_cccs.erase(temp_cccs.begin());
                if (!graph.vertices[vertex_id].removed) {
                    degadgetize_gadgets.push_back(new_tableau.size() - 1);
                }
            }
        } else {
            // This is a PR vertex
            if (pr_columns.count(vertex_id) > 0) {
                
                std::vector<PauliRotation> pr_column = std::move(pr_columns[vertex_id]);
                spdlog::debug("PR vertex {}: pr_column BEFORE commuting through {} remaining CCC(s): size={}",
                              vertex_id, temp_cccs.size(), pr_column.size());
                for (size_t i = 0; i < pr_column.size(); ++i) {
                    spdlog::debug("  PR vertex {}: BEFORE  col[{}] = {:b}", vertex_id, i, pr_column[i]);
                }
                
                for (auto it = temp_cccs.rbegin(); it != temp_cccs.rend(); ++it) {
                    auto& ccc = *it;
                    // When commuting (diagonal) PR columns through a Hadamard gadget,
                    // only the gadget qubit pair is needed: swap phase slots on (reference, ancilla).

                    size_t const a = ccc.reference_qubit();
                    size_t const b = ccc.ancilla_qubit();
                    spdlog::debug("PR vertex {}: commuting through gadget pair (ref={}, anc={}) | size={}",
                                  vertex_id, a, b, pr_column.size());
                    for (size_t i = 0; i < pr_column.size(); ++i) {
                        spdlog::debug("  PR vertex {}: pair(ref={},anc={}) BEFORE col[{}] = {:b}", vertex_id, a, b, i,
                                      pr_column[i]);
                    }
                    for (auto& rotation : pr_column) {
                        swap_gadget_phase_slots(rotation, a, b);
                    }
                    for (size_t i = 0; i < pr_column.size(); ++i) {
                        spdlog::debug("  PR vertex {}: pair(ref={},anc={}) AFTER  col[{}] = {:b}", vertex_id, a, b, i,
                                      pr_column[i]);
                    }
                }

                spdlog::debug("PR vertex {}: pr_column AFTER commuting through all remaining CCC(s): size={}",
                              vertex_id, pr_column.size());
                for (size_t i = 0; i < pr_column.size(); ++i) {
                    spdlog::debug("  PR vertex {}: AFTER   col[{}] = {:b}", vertex_id, i, pr_column[i]);
                }
                
                new_tableau.push_back(std::move(pr_column));
            }
        }
    }
    spdlog::debug("New tableau assembled: {} elements (before PMCs and back ST)", new_tableau.size());
    spdlog::debug("New tableau: {:b}", new_tableau);
    // Add PMCs & back StabilizerTableau at the end
    for (auto& pmc : pmcs) {
        new_tableau.push_back(std::move(pmc));
    }
    new_tableau.push_back(std::move(back_st));


    spdlog::debug("Added {} PMCs and back StabilizerTableau. New tableau size: {}", pmcs.size(), new_tableau.size());
    // spdlog::debug("New tableau: {:b}", new_tableau);
    // Re-establish CCC-PMC pairing after reordering (pairing pointers were invalidated)
    spdlog::debug("Re-establishing CCC-PMC pairing...");
    reestablish_hadamard_gadget_pairing(new_tableau);
    
    std::sort(degadgetize_gadgets.begin(), degadgetize_gadgets.end(), 
              [&new_tableau](size_t a, size_t b) {
                  auto* ccc_a = std::get_if<ClassicalControlTableau>(&new_tableau[a]);
                  auto* ccc_b = std::get_if<ClassicalControlTableau>(&new_tableau[b]);
                  if (!ccc_a || !ccc_b) return false;
                  return ccc_a->ancilla_qubit() > ccc_b->ancilla_qubit();
              });


    // Track which ancilla qubits are being removed (in descending order)
    std::set<size_t> removed_ancilla_qubits;

    
    spdlog::debug("Starting degadgetization of {} gadgets...", degadgetize_gadgets.size());
    size_t degadgetized_count = 0;
    for (size_t ccc_index : degadgetize_gadgets) {
        auto pmc_index_opt = new_tableau.find_pmc_index(ccc_index);
        if (!pmc_index_opt.has_value()) {
            spdlog::error("CCC at index {} has no paired PMC, skipping degadgetization", ccc_index);
            continue;
        }
        
        // Get the ancilla qubit being removed
        auto* ccc_ptr = std::get_if<ClassicalControlTableau>(&new_tableau[ccc_index]);
        if (!ccc_ptr) continue;
        size_t ancilla_qubit = ccc_ptr->ancilla_qubit();
        removed_ancilla_qubits.insert(ancilla_qubit);
        
        hadamard_degadgetize(new_tableau, ccc_index, pmc_index_opt.value());
        degadgetized_count++;

    }
    spdlog::debug("Completed degadgetization: {} gadgets processed", degadgetized_count);
    
    if (new_tableau.n_ancilla() > 0) {
        // Set the last n ancilla qubits to PLUS state, where n == new_tableau.n_ancilla()
        size_t n_ancilla = new_tableau.n_ancilla();
        size_t n_qubits = new_tableau.n_qubits();
        for (size_t anc_idx = n_qubits - n_ancilla; anc_idx < n_qubits; ++anc_idx) {
            new_tableau.add_ancilla_state(anc_idx, AncillaInitialState::PLUS);
        }
    }
    
    tableau = new_tableau;
    remove_identities(tableau);
    spdlog::debug("=== Finished reorder_n_degadgetize: final tableau size {} with {} qubits ({} ancillae) ===", 
                 tableau.size(), tableau.n_qubits(), tableau.n_ancilla());
}


}  // namespace qsyn::experimental

