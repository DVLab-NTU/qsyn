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

struct BinaryConstraintOp {
    enum class Kind {
        SwapAB,  // gadget(a,b): swap bit a and bit b
        CxCT,    // cx(c,t): bit t ^= bit c
    };
    Kind kind;
    size_t q0;
    size_t q1;
};

std::vector<BinaryConstraintOp> extract_ops_for_pr_reverse_apply(
    Tableau const& tableau) {
    std::vector<BinaryConstraintOp> ops;

    // Match the middle export window used by SAT export:
    // skip front ST at index 0, stop when PR segment starts.
    for (size_t idx = 1; idx < tableau.size(); ++idx) {
        if (std::holds_alternative<std::vector<PauliRotation>>(tableau[idx])) {
            break;
        }

        if (auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx])) {
            if (!cct->is_gadget()) {
                continue;
            }
            ops.push_back(BinaryConstraintOp{
                .kind = BinaryConstraintOp::Kind::SwapAB,
                .q0 = cct->reference_qubit(),
                .q1 = cct->ancilla_qubit(),
            });
            continue;
        }

        auto const* st = std::get_if<StabilizerTableau>(&tableau[idx]);
        if (!st) {
            continue;
        }
        auto const clifford_ops = extract_clifford_operators(*st);
        for (auto const& [type, qubits] : clifford_ops) {
            if (type != CliffordOperatorType::cx) {
                continue;
            }
            ops.push_back(BinaryConstraintOp{
                .kind = BinaryConstraintOp::Kind::CxCT,
                .q0 = qubits[0],
                .q1 = qubits[1],
            });
        }
    }

    // PR is commuted through the middle block in reverse direction.
    std::reverse(ops.begin(), ops.end());
    return ops;
}

std::vector<uint8_t> extract_z_bits(PauliRotation const& pr, size_t qubit_count) {
    std::vector<uint8_t> z_bits(qubit_count, 0);
    std::string const bit_string = pr.to_bit_string();
    size_t k = 0;
    for (char c : bit_string) {
        if (c == '0' || c == '1') {
            if (k >= qubit_count) {
                break;
            }
            z_bits[k++] = static_cast<uint8_t>(c - '0');
        }
    }
    return z_bits;
}

void apply_ops_to_z_bits(std::vector<uint8_t>& z_bits, std::vector<BinaryConstraintOp> const& ops) {
    for (auto const& op : ops) {
        if (op.kind == BinaryConstraintOp::Kind::SwapAB) {
            std::swap(z_bits[op.q0], z_bits[op.q1]);
        } else {
            z_bits[op.q1] ^= z_bits[op.q0];
        }
    }
}


/** Export validated H-gadget (CCC, PMC) pairs from a tableau. */
std::vector<ConstraintGraph::HadamardGadgetPair> export_hadamard_gadget_pairs(Tableau& tableau) {
    std::vector<ConstraintGraph::HadamardGadgetPair> pairs;

    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct == nullptr || !cct->is_gadget()) {
            continue;
        }

        auto const pair_opt = find_gadget_pair(tableau, cct->ancilla_qubit());
        if (!pair_opt.has_value()) {
            spdlog::warn(
                "export_hadamard_gadget_pairs: no PMC for gadget at index {} (ancilla={})",
                idx,
                cct->ancilla_qubit());
            continue;
        }

        ConstraintGraph::HadamardGadgetPair pair;
        pair.ccc_index       = pair_opt->gadget_index;
        pair.pmc_index       = pair_opt->pmc_index;
        pair.ancilla_qubit   = cct->ancilla_qubit();
        pair.reference_qubit = cct->reference_qubit();
        pairs.push_back(pair);
    }
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

void ConstraintGraph::disconnect_gadget_pr_edges(size_t gadget_idx) {
    if (gadget_idx >= vertices.size() || vertices[gadget_idx].type != VertexType::GADGET) {
        return;
    }
    if (vertices[gadget_idx].removed) {
        return;
    }
    vertices[gadget_idx].removed = true;

    {
        auto& out = outgoing_edges[gadget_idx];
        out.erase(
            std::remove_if(out.begin(), out.end(), [&](size_t to) {
                if (to >= vertices.size() || vertices[to].type != VertexType::PR) {
                    return false;
                }
                auto& in_to = incoming_edges[to];
                in_to.erase(std::remove(in_to.begin(), in_to.end(), gadget_idx), in_to.end());
                return true;
            }),
            out.end());
    }

    {
        auto& in = incoming_edges[gadget_idx];
        in.erase(
            std::remove_if(in.begin(), in.end(), [&](size_t from) {
                if (from >= vertices.size() || vertices[from].type != VertexType::PR) {
                    return false;
                }
                auto& out_from = outgoing_edges[from];
                out_from.erase(std::remove(out_from.begin(), out_from.end(), gadget_idx), out_from.end());
                return true;
            }),
            in.end());
    }
}

std::optional<ConstraintGraph::ReorderPlan> ConstraintGraph::topological_sort() const {
    size_t const num_gadgets = static_cast<size_t>(std::count_if(
        vertices.begin(),
        vertices.end(),
        [](Vertex const& v) { return v.type == VertexType::GADGET; }));

    ReorderPlan plan;
    plan.slot_to_prs.assign(num_gadgets + 1, {});

    // Full-graph DFS post-order (removed gadgets still enforce gadget-gadget order).
    enum class Color : uint8_t { WHITE, GRAY, BLACK };
    std::vector<Color> color(vertices.size(), Color::WHITE);
    std::vector<size_t> postorder;
    std::vector<size_t> stack_path;
    std::optional<std::pair<size_t, size_t>> back_edge_witness;

    std::function<void(size_t)> dfs = [&](size_t v) {
        if (v >= vertices.size()) {
            return;
        }
        if (color[v] == Color::BLACK) {
            return;
        }
        if (color[v] == Color::GRAY) {
            if (!stack_path.empty() && !back_edge_witness.has_value()) {
                back_edge_witness = std::make_pair(stack_path.back(), v);
            }
            return;
        }
        color[v] = Color::GRAY;
        stack_path.push_back(v);
        for (size_t u : outgoing_edges[v]) {
            if (u >= vertices.size()) {
                continue;
            }
            dfs(u);
        }
        stack_path.pop_back();
        color[v] = Color::BLACK;
        postorder.push_back(v);
    };

    // Visit gadgets first for stable gadget_order, then PRs for orphans.
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].type != VertexType::GADGET) {
            continue;
        }
        if (color[i] == Color::WHITE) {
            dfs(i);
        }
    }
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].type != VertexType::PR) {
            continue;
        }
        if (color[i] == Color::WHITE) {
            dfs(i);
        }
    }

    if (back_edge_witness.has_value()) {
        auto const [from, to] = *back_edge_witness;
        auto type_name = [](VertexType t) {
            return t == VertexType::GADGET ? "GADGET" : "PR";
        };
        spdlog::error(
            "topological_sort: cycle detected via back edge {} ({}) -> {} ({})",
            from,
            type_name(vertices[from].type),
            to,
            type_name(vertices[to].type));
        return std::nullopt;
    }

    std::reverse(postorder.begin(), postorder.end());

    // Extract gadget_order from the linear order (all gadgets, including removed).
    plan.gadget_order.reserve(num_gadgets);
    for (size_t v : postorder) {
        if (vertices[v].type == VertexType::GADGET) {
            plan.gadget_order.push_back(v);
        }
    }

    // PR slot = gadgets before it in linear order, or G if no active gadget edges.
    std::unordered_map<size_t, size_t> position_in_order;
    position_in_order.reserve(postorder.size());
    for (size_t pos = 0; pos < postorder.size(); ++pos) {
        position_in_order[postorder[pos]] = pos;
    }

    std::vector<size_t> gadgets_before(postorder.size(), 0);
    {
        size_t running = 0;
        for (size_t pos = 0; pos < postorder.size(); ++pos) {
            gadgets_before[pos] = running;
            if (vertices[postorder[pos]].type == VertexType::GADGET) {
                ++running;
            }
        }
    }

    auto const has_gadget_edge = [&](size_t pr_vertex) {
        for (size_t from : incoming_edges[pr_vertex]) {
            if (from < vertices.size() && !vertices[from].removed &&
                vertices[from].type == VertexType::GADGET) {
                return true;
            }
        }
        for (size_t to : outgoing_edges[pr_vertex]) {
            if (to < vertices.size() && !vertices[to].removed &&
                vertices[to].type == VertexType::GADGET) {
                return true;
            }
        }
        return false;
    };

    for (size_t pr_vertex = 0; pr_vertex < vertices.size(); ++pr_vertex) {
        if (vertices[pr_vertex].removed || vertices[pr_vertex].type != VertexType::PR) {
            continue;
        }
        size_t slot = num_gadgets;
        if (has_gadget_edge(pr_vertex)) {
            auto const it = position_in_order.find(pr_vertex);
            if (it == position_in_order.end()) {
                spdlog::error("topological_sort: PR vertex {} missing from order", pr_vertex);
                return std::nullopt;
            }
            slot = gadgets_before[it->second];
        }
        plan.slot_to_prs[slot].push_back(pr_vertex);
    }

    // Sort each slot's PRs by vertex ID (original PR-block order).
    for (auto& bucket : plan.slot_to_prs) {
        std::ranges::sort(bucket);
    }

    size_t total_prs = 0;
    for (auto const& bucket : plan.slot_to_prs) {
        total_prs += bucket.size();
    }
    spdlog::info(
        "topological_sort: {} gadgets ordered, {} PRs in {} slots",
        plan.gadget_order.size(),
        total_prs,
        plan.slot_to_prs.size());
    return plan;
}

size_t ConstraintGraph::break_cycles() {
    size_t total_removed = 0;

    // Repeat enumerate-and-greedy-break until DFS finds no cycles.
    for (size_t iteration = 0; iteration < vertices.size() + 1; ++iteration) {
    size_t num_cycles = 0;
    // Per cycle: gadgets that have a PR<->gadget edge to a PR also in that cycle
    std::vector<std::set<size_t>> cycle_pr_gadgets;
    // Map from gadget vertex index to set of cycle indices it appears in (PR-adjacent only)
    std::unordered_map<size_t, std::set<size_t>> gadget_to_cycles;
    
    auto gadget_has_pr_edge_in_cycle =
        [&](size_t gadget_v, std::unordered_set<size_t> const& cycle_vertices) {
            if (gadget_v >= vertices.size() || vertices[gadget_v].type != VertexType::GADGET) {
                return false;
            }
            for (size_t u : outgoing_edges[gadget_v]) {
                if (u < vertices.size() && vertices[u].type == VertexType::PR &&
                    cycle_vertices.count(u) != 0) {
                    return true;
                }
            }
            for (size_t from : incoming_edges[gadget_v]) {
                if (from < vertices.size() && vertices[from].type == VertexType::PR &&
                    cycle_vertices.count(from) != 0) {
                    return true;
                }
            }
            return false;
        };
    
    // Track visited vertices for cycle detection
    enum class Color { WHITE, GRAY, BLACK };
    std::vector<Color> color(vertices.size(), Color::WHITE);
    std::vector<size_t> current_path;
    std::unordered_map<size_t, size_t> path_index;  // vertex -> index in current_path
    
    // Helper function to extract cycle from path when back edge is found
    auto extract_cycle = [&](size_t cycle_start_idx) {
        std::vector<size_t> cycle;
        for (size_t i = cycle_start_idx; i < current_path.size(); ++i) {
            cycle.push_back(current_path[i]);
        }
        cycle.push_back(current_path[cycle_start_idx]);  // Close the cycle

        size_t const cycle_id = num_cycles++;

        std::unordered_set<size_t> cycle_vertices;
        for (size_t v : cycle) {
            cycle_vertices.insert(v);
        }

        std::set<size_t> pr_gadgets;
        for (size_t v : cycle_vertices) {
            if (!gadget_has_pr_edge_in_cycle(v, cycle_vertices)) {
                continue;
            }
            pr_gadgets.insert(v);
            gadget_to_cycles[v].insert(cycle_id);
        }
        cycle_pr_gadgets.push_back(std::move(pr_gadgets));
    };
    
    // Full-graph DFS; removed gadgets still participate in gadget-gadget paths.
    std::function<void(size_t)> dfs = [&](size_t v) {
        if (v >= vertices.size()) {
            return;
        }
        
        color[v] = Color::GRAY;
        path_index[v] = current_path.size();
        current_path.push_back(v);
        
        for (size_t u : outgoing_edges[v]) {
            if (u >= vertices.size()) {
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
    
    // Start DFS from each gadget, then PR, to capture all cycles.
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].type != VertexType::GADGET) {
            continue;
        }
        
        if (color[i] == Color::WHITE) {
            current_path.clear();
            path_index.clear();
            dfs(i);
        }
    }
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].type != VertexType::PR) {
            continue;
        }
        
        if (color[i] == Color::WHITE) {
            current_path.clear();
            path_index.clear();
            dfs(i);
        }
    }
    
    // Track which cycles are still active (not yet broken)
    std::set<size_t> active_cycles;
    for (size_t i = 0; i < num_cycles; ++i) {
        active_cycles.insert(i);
    }
    
    // Helper function to break cycles by disconnecting PR <-> gadget edges
    // for the selected gadget, then marking all its cycles as resolved.
    auto disconnect_pr_edges_and_cycles = [&](size_t gadget_idx) {
        disconnect_gadget_pr_edges(gadget_idx);
        ++total_removed;

        // Remove all cycles containing this gadget from active cycles
        auto cycles_to_remove = gadget_to_cycles[gadget_idx];
        for (size_t cycle_id : cycles_to_remove) {
            active_cycles.erase(cycle_id);
            if (cycle_id >= cycle_pr_gadgets.size()) {
                continue;
            }
            for (size_t v : cycle_pr_gadgets[cycle_id]) {
                auto it = gadget_to_cycles.find(v);
                if (it != gadget_to_cycles.end()) {
                    it->second.erase(cycle_id);
                }
            }
        }
        
        // Remove the gadget from cycle-coverage bookkeeping
        gadget_to_cycles.erase(gadget_idx);
    };
    
    // First pass: if a cycle has exactly one PR-adjacent gadget, disconnect it.
    std::set<size_t> cycles_to_check = active_cycles;  // Copy to iterate safely
    for (size_t cycle_id : cycles_to_check) {
        if (active_cycles.count(cycle_id) == 0 || cycle_id >= cycle_pr_gadgets.size()) {
            continue;
        }

        auto const& pr_gadgets = cycle_pr_gadgets[cycle_id];
        if (pr_gadgets.size() != 1) {
            continue;
        }

        size_t const gadget_idx = *pr_gadgets.begin();
        if (gadget_idx < vertices.size() && !vertices[gadget_idx].removed &&
            vertices[gadget_idx].type == VertexType::GADGET) {
            disconnect_pr_edges_and_cycles(gadget_idx);
        }
    }
    
    // Greedy: disconnect PR edges on the non-removed gadget in the most cycles.
    while (!active_cycles.empty()) {
        // Find the non-removed gadget that appears in the most active cycles
        size_t max_count = 0;
        size_t vertex_to_remove = std::numeric_limits<size_t>::max();
        
        for (auto const& [gadget_idx, cycle_set] : gadget_to_cycles) {
            if (gadget_idx >= vertices.size() || vertices[gadget_idx].removed ||
                vertices[gadget_idx].type != VertexType::GADGET) {
                continue;
            }
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
        
        // Disconnect PR<->gadget edges for this gadget
        disconnect_pr_edges_and_cycles(vertex_to_remove);
    }

    if (num_cycles == 0) {
        break;
    }
    }  // end of outer iteration loop

    spdlog::info("break_cycles: {} gadgets disconnected from PR constraints", total_removed);
    return total_removed;
}

/** Build gadget/PR constraint graph from tableau and eligibility set. */
ConstraintGraph build_constraint_graph(
    Tableau const& tableau,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    std::unordered_set<size_t> const& degadgetizable_gadget_indices) {
    ConstraintGraph graph;

    std::unordered_map<size_t, std::vector<size_t>> reference_gadgets;

    // Phase 1: gadget vertices and gadget-gadget ordering edges only.
    // Gadget vertex IDs match gadget indices (0..G-1).
    for (size_t g_idx = 0; g_idx < gadgets.size(); ++g_idx) {
        auto const& gadget = gadgets[g_idx];
        graph.create_vertex(gadget);
        if (!degadgetizable_gadget_indices.contains(g_idx)) {
            graph.vertices[g_idx].removed = true;
        }
        if (gadget.reference_qubit.has_value()) {
            reference_gadgets[gadget.reference_qubit.value()].push_back(g_idx);
        }
    }

    for (size_t i = 0; i < gadgets.size(); ++i) {
        for (size_t j = 0; j < gadgets.size(); ++j) {
            if (i == j) {
                continue;
            }
            if (gadgets[i].ancilla_qubit < gadgets[j].ancilla_qubit) {
                graph.add_edge(i, j);
            }
        }
    }

    for (auto& [ref_qubit, gadget_indices] : reference_gadgets) {
        (void)ref_qubit;
        std::sort(gadget_indices.begin(), gadget_indices.end(), [&](size_t a, size_t b) {
            return gadgets[a].ccc_index < gadgets[b].ccc_index;
        });
        for (size_t i = 0; i + 1 < gadget_indices.size(); ++i) {
            graph.add_edge(gadget_indices[i], gadget_indices[i + 1]);
        }
    }

    // Phase 2: PR vertices and PR<->gadget edges for degadgetizable gadgets only.
    // PR vertex IDs are num_gadgets + global_pr_index.
    size_t const num_gadgets = gadgets.size();
    std::vector<ConstraintGraph::PRInfo> all_prs;
    size_t global_pr_counter = 0;
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[idx]);
        if (pr_vec == nullptr) {
            continue;
        }
        for (size_t pr_idx = 0; pr_idx < pr_vec->size(); ++pr_idx) {
            all_prs.push_back({idx, pr_idx, global_pr_counter, &(*pr_vec)[pr_idx]});
            ++global_pr_counter;
        }
    }

    for (auto const& pr_info : all_prs) {
        graph.create_vertex(pr_info);
    }

    std::vector<std::vector<size_t>> gadget_x_qubits(gadgets.size());
    for (size_t g_idx : degadgetizable_gadget_indices) {
        if (g_idx >= gadgets.size()) {
            continue;
        }
        auto const& gadget = gadgets[g_idx];
        if (!gadget.reference_qubit.has_value()) {
            continue;
        }
        auto const pair_opt = find_gadget_pair(tableau, gadget.ancilla_qubit);
        if (!pair_opt.has_value()) {
            continue;
        }
        auto const* pmc = std::get_if<ClassicalControlTableau>(&tableau[pair_opt->pmc_index]);
        if (pmc == nullptr) {
            continue;
        }
        gadget_x_qubits[g_idx] = extract_pmc_x_qubits(*pmc);
    }

    auto const reverse_ops = extract_ops_for_pr_reverse_apply(tableau);

    for (auto const& pr_info : all_prs) {
        auto const& pr         = *pr_info.pr_ptr;
        size_t const pr_vertex = num_gadgets + pr_info.global_pr_index;
        std::vector<uint8_t> after_cccs_pr  = extract_z_bits(pr, tableau.n_qubits());
        std::vector<uint8_t> before_cccs_pr = after_cccs_pr;
        apply_ops_to_z_bits(before_cccs_pr, reverse_ops);

        for (size_t g_idx : degadgetizable_gadget_indices) {
            if (g_idx >= gadgets.size()) {
                continue;
            }
            auto const& gadget       = gadgets[g_idx];
            size_t const ancilla_qubit = gadget.ancilla_qubit;

            for (size_t const x_qubit : gadget_x_qubits[g_idx]) {
                if (x_qubit < pr.n_qubits() && pr.is_z(x_qubit)) {
                    graph.add_edge(g_idx, pr_vertex);
                    break;
                }
            }

            if (ancilla_qubit < before_cccs_pr.size() && before_cccs_pr[ancilla_qubit] == 1) {
                graph.add_edge(g_idx, pr_vertex);
            }
            if (ancilla_qubit < after_cccs_pr.size() && after_cccs_pr[ancilla_qubit] == 1) {
                graph.add_edge(pr_vertex, g_idx);
            }
        }
    }


    size_t edge_count = 0;
    for (auto const& out : graph.outgoing_edges) {
        edge_count += out.size();
    }
    size_t const pr_blocked = static_cast<size_t>(std::count_if(
        graph.vertices.begin(),
        graph.vertices.end(),
        [](ConstraintGraph::Vertex const& v) {
            return v.type == ConstraintGraph::VertexType::GADGET && v.removed;
        }));
    spdlog::info(
        "build_constraint_graph: {} gadgets ({} pr-blocked), {} PR vertices, {} edges",
        gadgets.size(),
        pr_blocked,
        all_prs.size(),
        edge_count);
    return graph;
}

namespace {

std::optional<size_t> find_unified_pr_index(Tableau const& tableau) {
    size_t idx = 1;
    while (idx < tableau.size()) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct != nullptr && cct->is_gadget()) {
            ++idx;
            continue;
        }
        if (std::holds_alternative<StabilizerTableau>(tableau[idx])) {
            ++idx;
            continue;
        }
        break;
    }
    if (idx < tableau.size() && std::holds_alternative<std::vector<PauliRotation>>(tableau[idx])) {
        return idx;
    }
    return std::nullopt;
}

/** Collect ancilla qubits for graph-active (!removed) gadgets after break_cycles. */
std::vector<size_t> collect_degadgetize_targets_from_graph(
    ConstraintGraph const& graph,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets) {
    std::vector<size_t> targets;
    targets.reserve(gadgets.size());

    for (size_t g_idx = 0; g_idx < gadgets.size() && g_idx < graph.vertices.size(); ++g_idx) {
        auto const& v = graph.vertices[g_idx];
        if (v.type != ConstraintGraph::VertexType::GADGET) {
            continue;
        }
        if (v.removed) {
            continue;
        }
        targets.push_back(gadgets[g_idx].ancilla_qubit);
    }

    size_t const active_gadget_count = static_cast<size_t>(std::count_if(
        graph.vertices.begin(),
        graph.vertices.end(),
        [](ConstraintGraph::Vertex const& v) {
            return v.type == ConstraintGraph::VertexType::GADGET && !v.removed;
        }));
    spdlog::info(
        "collect_degadgetize_targets: {}/{} graph-active targets",
        targets.size(),
        active_gadget_count);
    return targets;
}

/** Pre-graph PR blocking check: eligible iff no PR has Z on ancilla and an PMC x-qubit. */
std::unordered_set<size_t> analyze_gadget_degadgetization_eligibility(
    Tableau const& tableau,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets) {
    std::unordered_set<size_t> eligible_gadget_indices;

    auto const pr_idx_opt = find_unified_pr_index(tableau);
    if (!pr_idx_opt.has_value()) {
        spdlog::error("analyze_gadget_degadgetization_eligibility: unified PR block not found");
        return eligible_gadget_indices;
    }

    auto const& unified_pr = std::get<std::vector<PauliRotation>>(tableau[*pr_idx_opt]);

    for (size_t g_idx = 0; g_idx < gadgets.size(); ++g_idx) {
        auto const& gadget = gadgets[g_idx];
        if (!gadget.reference_qubit.has_value()) {
            spdlog::debug(
                "Gadget g_idx={} ancilla={}: missing reference qubit, skipping",
                g_idx,
                gadget.ancilla_qubit);
            continue;
        }

        auto const pair_opt = find_gadget_pair(tableau, gadget.ancilla_qubit);
        if (!pair_opt.has_value()) {
            spdlog::debug(
                "Gadget g_idx={} PMC({},{}) not found, skipping",
                g_idx,
                gadget.reference_qubit.value(),
                gadget.ancilla_qubit);
            continue;
        }
        auto const* pmc = std::get_if<ClassicalControlTableau>(&tableau[pair_opt->pmc_index]);
        if (pmc == nullptr) {
            spdlog::debug(
                "Gadget g_idx={} PMC({},{}) not found, skipping",
                g_idx,
                gadget.reference_qubit.value(),
                gadget.ancilla_qubit);
            continue;
        }

        auto const analysis = analyze_pmc_pr_blocking(*pmc, unified_pr);
        if (!analysis.is_degadgetizable) {
            continue;
        }

        eligible_gadget_indices.insert(g_idx);
    }

    spdlog::info(
        "analyze_gadget_degadgetization_eligibility: {}/{} gadgets eligible ({} PR columns)",
        eligible_gadget_indices.size(),
        gadgets.size(),
        unified_pr.size());
    return eligible_gadget_indices;
}

/** Pad every sub-tableau to tableau.n_qubits() (front/back ST may stay data-width after gadgetization). */
void pad_subtableaux_to_n_qubits(Tableau& tableau) {
    size_t const n = tableau.n_qubits();
    for (auto& sub : tableau) {
        std::visit(
            dvlab::overloaded{
                [&](StabilizerTableau& st) {
                    while (st.n_qubits() < n) {
                        st.add_ancilla_qubit();
                    }
                },
                [&](std::vector<PauliRotation>& pr) {
                    for (auto& rotation : pr) {
                        while (rotation.n_qubits() < n) {
                            rotation.add_ancilla_qubit();
                        }
                    }
                },
                [&](ClassicalControlTableau& cct) {
                    while (cct.operations().n_qubits() < n) {
                        cct.add_ancilla_qubit();
                    }
                }},
            sub);
    }
}

}  // namespace



void reorder_n_degadgetize(Tableau& tableau) {
    if (!has_gadget_ancillae(tableau)) {
        spdlog::info("reorder_n_degadgetize: skipped (no gadget ancilla)");
        return;
    }
    auto const structure = inspect_degadgetization_structure(tableau);
    if (!structure.is_valid) {
        spdlog::error("reorder_n_degadgetize: invalid degadgetization structure");
        return;
    }
    spdlog::info(
        "inspect_degadgetization_structure: {} gadgets, {} PR columns",
        structure.ccc_count,
        structure.pr_column_count);

    auto gadgets = export_hadamard_gadget_pairs(tableau);
    auto const degadgetizable_gadget_indices =
        analyze_gadget_degadgetization_eligibility(tableau, gadgets);

    ConstraintGraph graph = build_constraint_graph(tableau, gadgets, degadgetizable_gadget_indices);

    graph.break_cycles();

    auto degadgetize_targets = collect_degadgetize_targets_from_graph(graph, gadgets);
    size_t const degadgetize_target_count = degadgetize_targets.size();
    std::ranges::sort(degadgetize_targets);

    auto const reorder_plan_opt = graph.topological_sort();
    
    if (!reorder_plan_opt.has_value()) {
        spdlog::error("Topological sort failed - PR slot assignment infeasible");
        return;
    }
    ConstraintGraph::ReorderPlan const& reorder_plan = reorder_plan_opt.value();
    size_t const num_gadgets = gadgets.size();

    auto const initial_unified_pr_idx_opt = find_unified_pr_index(tableau);
    if (!initial_unified_pr_idx_opt.has_value()) {
        spdlog::error("reorder_n_degadgetize: unified PR block not found before reorder");
        return;
    }
    size_t const initial_unified_pr_idx = initial_unified_pr_idx_opt.value();
    size_t slots_processed               = 0;

    // Iterate slots 0..G-1: extract slot's rotations from the residual unified PR,
    // insert as a SubTableau at that block's current index, then swap_along leftward
    // to right before that slot's gadget. Slot G's rotations remain in the residual block.
    for (size_t slot = 0; slot < num_gadgets; ++slot) {
        size_t const gadget_vertex = reorder_plan.gadget_order[slot];
        auto const& gadget_pair    = gadgets[gadget_vertex];
        if (!gadget_pair.reference_qubit.has_value()) {
            spdlog::error("reorder_n_degadgetize: gadget {} has no reference qubit", gadget_vertex);
            return;
        }
        size_t const ancilla_qubit = gadget_pair.ancilla_qubit;

        auto const& slot_pr_vertices = reorder_plan.slot_to_prs[slot];
        if (slot_pr_vertices.empty()) {
            continue;
        }

        size_t const unified_pr_idx = initial_unified_pr_idx + slots_processed;
        if (unified_pr_idx >= tableau.size()) {
            spdlog::error(
                "reorder_n_degadgetize: residual PR index {} out of range (tableau size {})",
                unified_pr_idx,
                tableau.size());
            return;
        }

        auto* unified_pr = std::get_if<std::vector<PauliRotation>>(&tableau[unified_pr_idx]);
        if (unified_pr == nullptr) {
            spdlog::error(
                "reorder_n_degadgetize: expected residual PR block at index {}, slot {}",
                unified_pr_idx,
                slot);
            return;
        }

        // PR vertex IDs to extract -> indices within the (shrinking) unified PR block.
        // PR vertex ID = num_gadgets + remaining_global_index, where remaining_global_index
        // is the position within the *current* unified_pr (we extract earlier slots first,
        // so remaining vertex IDs map to consecutive positions in the residual block).
        std::vector<PauliRotation> slot_rotations;
        slot_rotations.reserve(slot_pr_vertices.size());
        std::vector<size_t> indices_to_remove;
        indices_to_remove.reserve(slot_pr_vertices.size());
        for (size_t const pr_vertex : slot_pr_vertices) {
            if (pr_vertex < num_gadgets) {
                continue;
            }
            indices_to_remove.push_back(pr_vertex - num_gadgets);
        }
        std::ranges::sort(indices_to_remove);
        for (auto it = indices_to_remove.rbegin(); it != indices_to_remove.rend(); ++it) {
            size_t const original_idx = *it;
            // Compute current position by counting how many earlier-slot rotations
            // had original_idx < this one. Earlier slots already removed those.
            size_t current_pos = original_idx;
            for (size_t prior_slot = 0; prior_slot < slot; ++prior_slot) {
                for (size_t const prior_pr_vertex : reorder_plan.slot_to_prs[prior_slot]) {
                    if (prior_pr_vertex < num_gadgets) {
                        continue;
                    }
                    size_t const prior_original_idx = prior_pr_vertex - num_gadgets;
                    if (prior_original_idx < original_idx) {
                        --current_pos;
                    }
                }
            }
            if (current_pos >= unified_pr->size()) {
                spdlog::error(
                    "reorder_n_degadgetize: PR index {} out of range (unified size {})",
                    current_pos,
                    unified_pr->size());
                return;
            }
            slot_rotations.push_back(std::move((*unified_pr)[current_pos]));
            unified_pr->erase(unified_pr->begin() + static_cast<std::ptrdiff_t>(current_pos));
        }
        std::ranges::reverse(slot_rotations);  // restore vertex-ID ascending order

        tableau.insert(
            tableau.begin() + static_cast<std::ptrdiff_t>(unified_pr_idx),
            SubTableau{std::move(slot_rotations)});

        auto const target_idx_opt = find_gadget_pair(tableau, ancilla_qubit);
        if (!target_idx_opt.has_value()) {
            spdlog::error(
                "reorder_n_degadgetize: gadget anc={} missing while placing slot {}",
                ancilla_qubit,
                slot);
            return;
        }
        size_t const target_idx = target_idx_opt->gadget_index;
        if (unified_pr_idx <= target_idx) {
            spdlog::error(
                "reorder_n_degadgetize: slot {} has unified PR at {} but gadget at {} (must be left of PR)",
                slot,
                unified_pr_idx,
                target_idx);
            return;
        }

        swap_along(tableau, unified_pr_idx, target_idx);
        ++slots_processed;
    }

    // Slot G (after all gadgets): rotations remain in the residual unified PR block.

    spdlog::info(
        "PR reorder: {} slot insertions for {} gadgets",
        slots_processed,
        num_gadgets);

    size_t const degadgetized_count = hadamard_degadgetize(tableau, degadgetize_targets);
    spdlog::info(
        "degadgetization: {}/{} targets processed ({} qubits, {} ancillae remaining)",
        degadgetized_count,
        degadgetize_target_count,
        tableau.n_qubits(),
        tableau.n_ancilla());

    tableau.clear_ancilla_metadata();
    if (tableau.n_ancilla() > 0) {
        size_t const n_ancilla = tableau.n_ancilla();
        size_t const n_qubits  = tableau.n_qubits();
        for (size_t anc_idx = n_qubits - n_ancilla; anc_idx < n_qubits; ++anc_idx) {
            tableau.add_ancilla_state(anc_idx, AncillaInitialState::PLUS);
            tableau.set_ancilla_measurement_type(anc_idx, MeasurementType::X);
        }
    }

    pad_subtableaux_to_n_qubits(tableau);
    remove_identities(tableau);
    
}


}  // namespace qsyn::experimental

