#include <gsl/narrow>
#include <stack>
#include <tl/adjacent.hpp>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>
#include <chrono>

#include "convert/tableau_to_qcir.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/graph/digraph.hpp"
#include "util/graph/minimum_spanning_arborescence.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::tableau {

namespace {

bool is_valid(PauliRotation const& rotation) {
    if (!rotation.is_diagonal()) {
        return false;
    }

    auto num_z = 0ul;
    for (auto i: std::views::iota(0ul, rotation.n_qubits())) {
        if (rotation.pauli_product().is_z_set(i)) {
            num_z++;
        }
    }
    return num_z == 1;
}

size_t hamming_weight(
    PauliRotation const& rotation) {
    auto const num_qubits = rotation.n_qubits();
    auto num_ones         = 0ul;
    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (rotation.pauli_product().is_z_set(i)) {
            num_ones++;
        }
    }
    return num_ones;
}

size_t qubit_hamming_weight(
    PauliRotation const& rotation) {
    auto const num_qubits = rotation.n_qubits();
    auto num_qubit_has_ones         = 0ul;
    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (rotation.pauli_product().is_z_set(i) || rotation.pauli_product().is_x_set(i)) {
            num_qubit_has_ones++;
        }
    }
    return num_qubit_has_ones;
}

// get the index of the rotation with the minimum number of 1s
// A term of k ones can always be synthesized with k-1 CNOTs
size_t get_best_rotation_idx(std::vector<PauliRotation> const& rotations) {
    auto min_ones = SIZE_MAX;
    auto best_idx = SIZE_MAX;
    for (auto const& [idx, rotation] : tl::views::enumerate(rotations)) {
        auto const num_ones = hamming_weight(rotation);
        if (num_ones < min_ones) {
            min_ones = num_ones;
            best_idx = idx;
        }
    }
    return best_idx;
}

// get the index of the rotation with the minimum number of qubit with 1s
size_t get_best_rotation_idx(std::vector<PauliRotation> const& rotations, std::vector<size_t> const& first_layer) {
    auto min_ones = SIZE_MAX;
    auto best_idx = SIZE_MAX;
    for (auto const& idx : first_layer) {
        auto const num_ones = qubit_hamming_weight(rotations[idx]);
        if (num_ones < min_ones) {
            min_ones = num_ones;
            best_idx = idx;
        }
    }
    return best_idx;
}

size_t hamming_weight(
    std::vector<PauliRotation> const& rotations,
    size_t q_idx, bool is_Z = true) {
    return std::ranges::count_if(rotations, [&](auto const& rotation) {
        return is_Z ? rotation.pauli_product().is_z_set(q_idx) : rotation.pauli_product().is_x_set(q_idx);
    });
}

size_t hamming_distance(
    std::vector<PauliRotation> const& rotations,
    size_t q1_idx,
    size_t q2_idx) {
    return std::ranges::count_if(rotations, [&](auto const& rotation) {
        return rotation.pauli_product().is_z_set(q1_idx) !=
               rotation.pauli_product().is_z_set(q2_idx);
    });
}

size_t cx_weight(
    std::vector<PauliRotation> const& rotations,
    size_t q1_idx,
    size_t q2_idx) {
    
    size_t x_distance = std::ranges::count_if(rotations, [&](auto const& rotation){
        return rotation.pauli_product().is_x_set(q1_idx) != rotation.pauli_product().is_x_set(q2_idx);
    });

    return x_distance + hamming_distance(rotations, q1_idx, q2_idx);
}

// build the dependency graph according to the commutation relation
dvlab::Digraph<size_t, int> get_dependency_graph(std::vector<PauliRotation> const& rotations, long long& total_is_commute, long long& total_add_edge) {
    size_t const num_rotations = rotations.size();
    dvlab::Digraph<size_t, int> dag{num_rotations};
    // Timer for is_commutative and add_edge
    for (auto i : std::views::iota(0ul, num_rotations)) {   
        for (auto j : std::views::iota(i+1, num_rotations)) {
            // Timing is_commutative
            auto t_commute_start = std::chrono::high_resolution_clock::now();
            bool not_commute = !is_commutative(rotations[i], rotations[j]);
            auto t_commute_end = std::chrono::high_resolution_clock::now();
            total_is_commute += std::chrono::duration_cast<std::chrono::microseconds>(t_commute_end - t_commute_start).count();
            if (not_commute) {
                // Timing add_edge
                auto t_addedge_start = std::chrono::high_resolution_clock::now();
                dag.add_edge(i, j);
                auto t_addedge_end = std::chrono::high_resolution_clock::now();
                total_add_edge += std::chrono::duration_cast<std::chrono::microseconds>(t_addedge_end - t_addedge_start).count();
            }
        }
    }
    return dag;
}

// // New version get_dep_graph
// dvlab::Digraph<size_t, int> get_dependency_graph(std::vector<PauliRotation> const& rotations, long long& total_is_commute, long long& total_add_edge, long long& total_or_set) {
//     auto const num_rotations = rotations.size();
//     auto dag = dvlab::Digraph<size_t, int>{num_rotations};
//     std::vector<std::unordered_set<size_t>> predecessors(num_rotations);

//     for (auto i : std::views::iota(0ul, num_rotations)) {
//         // for j = i-1 to 0 in reverse order, check if rotations[i] and rotations[j] are commutative
//         for (size_t j = i - 1; j != SIZE_MAX; --j) {
//             size_t num_pred_to_reach = j;
//             if (predecessors[i].contains(j)) {
//                 continue;
//             }
//             auto t_is_commute_start = std::chrono::high_resolution_clock::now();
//             bool is_commute = is_commutative(rotations[i], rotations[j]);
//             auto t_is_commute_end = std::chrono::high_resolution_clock::now();
//             total_is_commute += std::chrono::duration_cast<std::chrono::microseconds>(t_is_commute_end - t_is_commute_start).count();
//             if (is_commute) {
//                 auto t_or_set_start = std::chrono::high_resolution_clock::now();
//                 predecessors[i].insert(predecessors[j].begin(), predecessors[j].end());
//                 auto t_or_set_end = std::chrono::high_resolution_clock::now();
//                 total_or_set += std::chrono::duration_cast<std::chrono::microseconds>(t_or_set_end - t_or_set_start).count();
//                 auto t_add_edge_start = std::chrono::high_resolution_clock::now();
//                 dag.add_edge(i, j);
//                 auto t_add_edge_end = std::chrono::high_resolution_clock::now();
//                 total_add_edge += std::chrono::duration_cast<std::chrono::microseconds>(t_add_edge_end - t_add_edge_start).count();
//             } else {
//                 num_pred_to_reach--;
//             }
//             if (predecessors[i].size() == num_pred_to_reach) {
//                 break;
//             }
//         }
//     }
//     return dag;
// }

dvlab::Digraph<size_t, int> get_parity_graph(
    std::vector<PauliRotation> const& rotations,
    PauliRotation const& target_rotation,
    std::string const& strategy = "hamming_weight") {
    auto const num_qubits = rotations.front().n_qubits();

    auto g = dvlab::Digraph<size_t, int>{};
    auto qubit_vec = std::vector<size_t>{};

    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (target_rotation.pauli_product().is_z_set(i)) {
            g.add_vertex_with_id(i);
            qubit_vec.push_back(i);
        }
    }
    // get the weight of the edge i if strategy is "hamming_weight"
    // otherwise, get the weight of the cx operation between i and j
    auto const get_weight = [&](size_t i, size_t j) {
        return strategy == "qubit_hamming_weight" 
            ? hamming_weight(rotations, i, true) + hamming_weight(rotations, j, false)
            : hamming_weight(rotations, i, true);
    };

    for (auto const& [i, j] : dvlab::combinations<2>(qubit_vec)) {
        auto const dist = (strategy == "qubit_hamming_weight" ) 
            ? gsl::narrow_cast<int>(cx_weight(rotations, i, j))
            : gsl::narrow_cast<int>(hamming_distance(rotations, i, j));
        auto const weight_i = gsl::narrow_cast<int>(get_weight(i, j));
        auto const weight_j = gsl::narrow_cast<int>(get_weight(j, i));
        g.add_edge(i, j, dist - weight_j - 1);
        g.add_edge(j, i, dist - weight_i - 1);
    }

    return g;
}

void apply_mst_cxs(dvlab::Digraph<size_t, int> const& mst, size_t root, 
                   std::vector<PauliRotation>& rotations, qcir::QCir& qcir, 
                   StabilizerTableau& final_clifford, size_t& num_cxs, bool backward) {
    
    auto const add_cx = [&](size_t ctrl, size_t targ) {
        for (auto& rot : rotations) {
            rot.cx(ctrl, targ);
        }
        if (backward) {
            qcir.prepend(qcir::CXGate(), {ctrl, targ});
            final_clifford.cx(ctrl, targ);
        } else {
            qcir.append(qcir::CXGate(), {ctrl, targ});
            final_clifford.prepend_cx(ctrl, targ);
        }
        num_cxs++;
    };
    // post-order traversal to add CXs
    std::stack<size_t> stack;
    std::vector<size_t> post_order_rev;

    stack.push(root);

    // First phase: collect nodes in post-order
    while (!stack.empty()) {
        auto const v = stack.top();
        stack.pop();
        post_order_rev.push_back(v);

        for (auto const& n : mst.out_neighbors(v)) {
            stack.push(n);
        }
    }

    // Second phase: apply CX gates in reverse post-order
    while (!post_order_rev.empty()) {
        auto const v = post_order_rev.back();
        post_order_rev.pop_back();
        if (mst.in_degree(v) == 1) {
            auto const pred = *mst.in_neighbors(v).begin();
            add_cx(v, pred);
        } else {
            DVLAB_ASSERT(
                mst.in_degree(v) == 0 && v == root,
                "The node with no incoming edges should be the root");
        }
    }
}


}  // namespace

std::optional<PartialSynthesisResult>
MstSynthesisStrategy::partial_synthesize(
    std::vector<PauliRotation> const& rotations) const {
    auto const num_qubits    = rotations.front().n_qubits();
    auto const num_rotations = rotations.size();

    if (num_qubits == 0) {
        return PartialSynthesisResult{
            qcir::QCir{0},
            StabilizerTableau{num_qubits}};
    }

    if (num_rotations == 0) {
        return PartialSynthesisResult{
            qcir::QCir{num_qubits},
            StabilizerTableau{num_qubits}};
    }

    // checks if all rotations are diagonal
    if (!std::ranges::all_of(rotations, &PauliRotation::is_diagonal)) {
        spdlog::error("MST only supports diagonal rotations");
        return std::nullopt;
    }

    auto copy_rotations = rotations;

    auto qcir = qcir::QCir{copy_rotations.front().n_qubits()};

    StabilizerTableau final_clifford{num_qubits};
    size_t num_cxs = 0;
    while (!copy_rotations.empty()) {
        auto const best_rotation_idx = get_best_rotation_idx(copy_rotations);
        std::swap(copy_rotations[best_rotation_idx], copy_rotations.back());
        auto const best_rotation = std::move(copy_rotations.back());
        copy_rotations.pop_back();

        auto const parity_graph =
            get_parity_graph(copy_rotations, best_rotation);

        auto const [mst, root] =
            dvlab::minimum_spanning_arborescence(parity_graph);

        apply_mst_cxs(mst, root, copy_rotations, qcir, final_clifford, num_cxs, false);

        // add the rotation at the root
        qcir.append(qcir::PZGate(best_rotation.phase()), {root});
    }

    spdlog::info("Number of CXs for row operations : {}", num_cxs);
    
    return PartialSynthesisResult{
        std::move(qcir),
        std::move(final_clifford)};
}

std::optional<qcir::QCir>
MstSynthesisStrategy::synthesize(
    std::vector<PauliRotation> const& rotations) const {
    auto partial_result = partial_synthesize(rotations);
    if (!partial_result) {
        return std::nullopt;
    }

    auto [qcir, final_clifford] = std::move(*partial_result);

    // it seems like gaussian is the best
    auto const final_cxs = synthesize_cx_gaussian(final_clifford);

    for (auto const& cx : final_cxs) {
        detail::add_clifford_gate(qcir, cx);
    }

    return qcir;
}

std::optional<qcir::QCir>
GeneralizedMstSynthesisStrategy::_partial_synthesize(
    PauliRotationTableau const& rotations, StabilizerTableau& residual_clifford, bool backward) const {
    
    auto append_s = [&](size_t qubit, qcir::QCir& qcir, StabilizerTableau& st) {
        qcir.append(qcir::SGate(), {qubit});
        st.prepend_sdg(qubit);
    };

    auto append_h = [&](size_t qubit, qcir::QCir& qcir, StabilizerTableau& st) {
        qcir.append(qcir::HGate(), {qubit});
        st.prepend_h(qubit);
    };

    auto prepend_s = [&](size_t qubit, qcir::QCir& qcir, StabilizerTableau& st) {
        qcir.prepend(qcir::SdgGate(), {qubit});
        st.s(qubit);
    };

    auto prepend_h = [&](size_t qubit, qcir::QCir& qcir, StabilizerTableau& st) {
        qcir.prepend(qcir::HGate(), {qubit});
        st.h(qubit);
    };

    auto add_s = [&](size_t qubit, std::vector<PauliRotation>& pr, qcir::QCir& qcir, StabilizerTableau& st, bool backward) {
        for(auto& rot: pr) {
            rot.s(qubit);
        }
        if (backward) {
            prepend_s(qubit, qcir, st);
        } else {
            append_s(qubit, qcir, st);
        }
    };

    auto add_h = [&](size_t qubit, std::vector<PauliRotation>& pr, qcir::QCir& qcir, StabilizerTableau& st, bool backward) {
        for(auto& rot: pr) {
            rot.h(qubit);
        }
        if (backward) {
            prepend_h(qubit, qcir, st);
        } else {
            append_h(qubit, qcir, st);
        }
    };
    
    auto const num_qubits    = rotations.front().n_qubits();
    auto const num_rotations = rotations.size();

    
    if (num_qubits == 0) {
        return qcir::QCir{0};
    }

    if (num_rotations == 0) {
        return qcir::QCir{num_qubits};
    }

    auto copy_rotations = rotations;
    auto qcir = qcir::QCir{num_qubits};
    // Timing: dependency graph construction
    auto t_depgraph_start = std::chrono::high_resolution_clock::now();
    long long total_is_commute = 0;
    long long total_add_edge = 0;
    auto dag = get_dependency_graph(rotations, total_is_commute, total_add_edge);
    auto t_depgraph_end = std::chrono::high_resolution_clock::now();
    spdlog::info("[TIMER] Dependency graph construction: {} us", std::chrono::duration_cast<std::chrono::microseconds>(t_depgraph_end - t_depgraph_start).count());
    spdlog::info("[TIMER]        is_commutative total {} us", total_is_commute);
    spdlog::info("[TIMER]        add_edge total {} us", total_add_edge);
    // create the index mapping
    std::vector<size_t> index_mapping(num_rotations);  // col_idx -> vertex_idx
    for (size_t i = 0; i < num_rotations; ++i) {
        index_mapping[i] = i;
    }
    size_t num_cxs = 0;
    // --- Timing ---
    long long total_firstlayer = 0;
    long long total_bestrot = 0;
    long long total_hs = 0;
    long long total_remove = 0;
    long long total_erase = 0;
    long long total_parity = 0;
    long long total_cal_mst = 0;
    long long total_mstcxs = 0;
    long long total_pz = 0;
    long long total_iter = 0;
    // ---
    while (!copy_rotations.empty()) {
        if (stop_requested()) break;
        // Timing: while loop iteration
        auto t_iter_start = std::chrono::high_resolution_clock::now();
        // Timing: find first layer rotations
        auto t_firstlayer_start = std::chrono::high_resolution_clock::now();
        std::vector<size_t> first_layer_rotations;
        for (auto i : std::views::iota(0ul, copy_rotations.size())) {
            if (backward) {
                if (dag.out_degree(index_mapping[i]) == 0) {
                    first_layer_rotations.push_back(i);
                }
            } else {
                if (dag.in_degree(index_mapping[i]) == 0) {
                    first_layer_rotations.push_back(i);
                }
            }
        }
        auto t_firstlayer_end = std::chrono::high_resolution_clock::now();
        total_firstlayer += std::chrono::duration_cast<std::chrono::microseconds>(t_firstlayer_end - t_firstlayer_start).count();
        // Timing: find best rotation
        auto t_bestrot_start = std::chrono::high_resolution_clock::now();
        auto const best_rotation_idx = get_best_rotation_idx(copy_rotations, first_layer_rotations);
        size_t best_vid = index_mapping[best_rotation_idx];
        auto best_rotation = copy_rotations[best_rotation_idx];
        auto t_bestrot_end = std::chrono::high_resolution_clock::now();
        total_bestrot += std::chrono::duration_cast<std::chrono::microseconds>(t_bestrot_end - t_bestrot_start).count();
        // Timing: handle H/S gates
        auto t_hs_start = std::chrono::high_resolution_clock::now();
        for (auto i: std::views::iota(0ul, num_qubits)) {
            if (best_rotation.pauli_product().is_x_set(i)) {
                if (best_rotation.pauli_product().is_z_set(i)) {
                    add_s(i, copy_rotations, qcir, residual_clifford, backward);
                }
                add_h(i, copy_rotations, qcir, residual_clifford, backward);
            }
        }
        // Update the best rotation
        best_rotation = copy_rotations[best_rotation_idx];
        assert(best_rotation.is_diagonal());
        auto t_hs_end = std::chrono::high_resolution_clock::now();
        total_hs += std::chrono::duration_cast<std::chrono::microseconds>(t_hs_end - t_hs_start).count();
        // Timing: remove vertex and update mapping
        auto t_remove_start = std::chrono::high_resolution_clock::now();
        dag.remove_vertex(best_vid);
        auto t_remove_end = std::chrono::high_resolution_clock::now();
        auto t_erase_start = std::chrono::high_resolution_clock::now();
        index_mapping.erase(index_mapping.begin() + best_rotation_idx);
        auto t_erase_end = std::chrono::high_resolution_clock::now();
        total_erase += std::chrono::duration_cast<std::chrono::microseconds>(t_erase_end - t_erase_start).count();
        total_remove += std::chrono::duration_cast<std::chrono::microseconds>(t_remove_end - t_remove_start).count();
        // Timing: parity graph and minimum spanning arborescence
        auto t_parity_start = std::chrono::high_resolution_clock::now();
        auto const parity_graph = get_parity_graph(copy_rotations, best_rotation, "qubit_hamming_weight");
        auto t_parity_end = std::chrono::high_resolution_clock::now();
        total_parity += std::chrono::duration_cast<std::chrono::microseconds>(t_parity_end - t_parity_start).count();
        auto t_cal_mst_start = std::chrono::high_resolution_clock::now();
        auto const [mst, root] = dvlab::minimum_spanning_arborescence(parity_graph);
        auto t_cal_mst_end = std::chrono::high_resolution_clock::now();
        total_cal_mst += std::chrono::duration_cast<std::chrono::microseconds>(t_cal_mst_end - t_cal_mst_start).count();
        // Timing: apply_mst_cxs
        auto t_mstcxs_start = std::chrono::high_resolution_clock::now();
        apply_mst_cxs(mst, root, copy_rotations, qcir, residual_clifford, num_cxs, backward);
        auto t_mstcxs_end = std::chrono::high_resolution_clock::now();
        total_mstcxs += std::chrono::duration_cast<std::chrono::microseconds>(t_mstcxs_end - t_mstcxs_start).count();
        assert(is_valid(copy_rotations[best_rotation_idx]));
        // Timing: erase rotation and add PZGate
        auto t_pz_start = std::chrono::high_resolution_clock::now();
        copy_rotations.erase(copy_rotations.begin() + best_rotation_idx);
        if (backward) {
            qcir.prepend(qcir::PZGate(best_rotation.phase()), {root});
        } else {
            qcir.append(qcir::PZGate(best_rotation.phase()), {root});
        }
        auto t_pz_end = std::chrono::high_resolution_clock::now();
        total_pz += std::chrono::duration_cast<std::chrono::microseconds>(t_pz_end - t_pz_start).count();
        auto t_iter_end = std::chrono::high_resolution_clock::now();
        total_iter += std::chrono::duration_cast<std::chrono::microseconds>(t_iter_end - t_iter_start).count();
    }
    // --- while結束後統一輸出 ---
    spdlog::info("[TIMER] Find first layer rotations: {} us", total_firstlayer);
    spdlog::info("[TIMER] Find best rotation: {} us", total_bestrot);
    spdlog::info("[TIMER] Handle H/S gates: {} us", total_hs);
    spdlog::info("[TIMER] Remove vertex: {} us", total_remove);
    spdlog::info("[TIMER] Erase index mapping: {} us", total_erase);
    spdlog::info("[TIMER] Parity graph + min spanning arborescence: {} us", total_parity);
    spdlog::info("[TIMER] apply_mst_cxs: {} us", total_mstcxs);
    spdlog::info("[TIMER] Erase rotation + add PZGate: {} us", total_pz);
    spdlog::info("[TIMER] Total while-iteration: {} us", total_iter);

    spdlog::info("Number of CXs for row operations : {}", num_cxs);
    
    return qcir;
}

std::optional<qcir::QCir>
GeneralizedMstSynthesisStrategy::synthesize(
    PauliRotationTableau const& rotations) const {

    auto partial_result = partial_synthesize(rotations);
    if (!partial_result) {
        return std::nullopt;
    }

    auto [qcir, final_clifford] = std::move(*partial_result);

    auto const final_clifford_circ = to_qcir(final_clifford, AGSynthesisStrategy{});
    if (!final_clifford_circ) {
        return std::nullopt;
    }
    qcir.compose(*final_clifford_circ);

    return qcir;
}

}  // namespace qsyn::tableau
