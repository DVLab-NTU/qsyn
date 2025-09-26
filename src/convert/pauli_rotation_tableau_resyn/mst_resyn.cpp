#include <gsl/narrow>
#include <stack>
#include <tl/adjacent.hpp>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>
#include <chrono>

#include "convert/tableau_to_qcir.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "tableau/stabilizer_tableau.hpp"
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

size_t hamming_weight(
    StabilizerTableau const& st,
    size_t q_idx, bool is_Z = true) {
    auto const num_qubits = st.n_qubits();
    auto num_ones = 0ul;
    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (is_Z ? st.stabilizer(i).is_z_set(q_idx) : st.stabilizer(i).is_x_set(q_idx)) {
            num_ones++;
        }
        if (is_Z ? st.destabilizer(i).is_z_set(q_idx) : st.destabilizer(i).is_x_set(q_idx)) {
            num_ones++;
        }
    }
    return num_ones;
}

size_t dist2idx_Z(StabilizerTableau const& st,size_t q_idx) {
    auto const num_qubits = st.n_qubits();
    auto dist = 0ul;
    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (st.stabilizer(i).is_z_set(q_idx)) {
            dist++;
        }
        if (st.destabilizer(i).is_z_set(q_idx)) {
            dist++;
        }
    }
    if (st.stabilizer(q_idx).is_z_set(q_idx)) {
        dist--;
    }
    return dist;
}

size_t dist2idx_X(StabilizerTableau const& st,size_t q_idx) {
    auto const num_qubits = st.n_qubits();
    auto dist = 0ul;
    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (st.stabilizer(i).is_x_set(q_idx)) {
            dist++;
        }
        if (st.destabilizer(i).is_x_set(q_idx)) {
            dist++;
        }
    }
    if (st.destabilizer(q_idx).is_x_set(q_idx)) {
        dist--;
    }
    return dist;
}

size_t dist2idx(
    StabilizerTableau const& st) {
    auto const num_qubits = st.n_qubits();
    auto dist = 0ul;
    for (auto i : std::views::iota(0ul, num_qubits)) {
        dist += dist2idx_Z(st, i);
        dist += dist2idx_X(st, i);
    }
    return dist;
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

size_t cx_weight(
    StabilizerTableau const& st,
    size_t q1_idx,
    size_t q2_idx) {
    auto w = 0ul;
    for (auto i : std::views::iota(0ul, st.n_qubits())) {
        if (st.stabilizer(i).is_z_set(q1_idx) != st.stabilizer(i).is_z_set(q2_idx)) {
            w++;
        }
        if (st.destabilizer(i).is_z_set(q1_idx) != st.destabilizer(i).is_z_set(q2_idx)) {
            w++;
        }
        if (st.stabilizer(i).is_x_set(q1_idx) != st.stabilizer(i).is_x_set(q2_idx)) {
            w++;
        }
        if (st.destabilizer(i).is_x_set(q1_idx) != st.destabilizer(i).is_x_set(q2_idx)) {
            w++;
        }
    }
    return w;
}

// build the dependency graph according to the commutation relation
dvlab::Digraph<size_t, int> get_dependency_graph(std::vector<PauliRotation> const& rotations) {
    size_t const num_rotations = rotations.size();
    dvlab::Digraph<size_t, int> dag{num_rotations};
    // Timer for is_commutative and add_edge
    for (auto i : std::views::iota(0ul, num_rotations)) {   
        for (auto j : std::views::iota(i+1, num_rotations)) {
            bool not_commute = !is_commutative(rotations[i], rotations[j]);
            if (not_commute) {
                dag.add_edge(i, j);
            }
        }
    }
    return dag;
}

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

dvlab::Digraph<size_t, int> get_parity_graph(
    std::vector<PauliRotation> const& rotations,
    StabilizerTableau const& residual_clifford,
    PauliRotation const& target_rotation) {
        
    assert(target_rotation.is_diagonal());
    auto const num_qubits = rotations.front().n_qubits();

    auto g = dvlab::Digraph<size_t, int>{};
    auto qubit_vec = std::vector<size_t>{};

    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (target_rotation.pauli_product().is_z_set(i)) {
            g.add_vertex_with_id(i);
            qubit_vec.push_back(i);
        }
    }

    auto get_weight = [&](size_t i, size_t j) {
        // w = ith Z + jth X
        auto const w_i = hamming_weight(rotations, i, true) + hamming_weight(residual_clifford, i, true);
        auto const w_j = hamming_weight(rotations, j, false) + hamming_weight(residual_clifford, j, false);
        return w_i + w_j;
    };

    auto get_weight_with_distance = [&](size_t i, size_t j) {
        auto const w_i = hamming_weight(rotations, i, true) + dist2idx_Z(residual_clifford, i);
        auto const w_j = hamming_weight(rotations, j, false) + dist2idx_X(residual_clifford, j);
        return w_i + w_j;
    };

    auto get_cx_weight = [&](size_t i, size_t j) {
        return cx_weight(rotations, i, j) + cx_weight(residual_clifford, i, j);
    };

    for (auto const& [i, j] : dvlab::combinations<2>(qubit_vec)) {
        auto const dist = get_cx_weight(i, j);
        auto const weight_i = get_weight_with_distance(i, j);
        auto const weight_j = get_weight_with_distance(j, i);
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
    
    auto const num_qubits    = residual_clifford.n_qubits();
    auto const num_rotations = rotations.size();

    
    if (num_qubits == 0) {
        return qcir::QCir{0};
    }

    if (num_rotations == 0) {
        return qcir::QCir{num_qubits};
    }

    auto copy_rotations = rotations;
    auto qcir = qcir::QCir{num_qubits};
    auto dag = get_dependency_graph(rotations);
    // create the index mapping
    std::vector<size_t> index_mapping(num_rotations);  // col_idx -> vertex_idx
    for (size_t i = 0; i < num_rotations; ++i) {
        index_mapping[i] = i;
    }
    size_t num_cxs = 0;
    while (!copy_rotations.empty()) {
        if (stop_requested()) break;
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
        auto const best_rotation_idx = get_best_rotation_idx(copy_rotations, first_layer_rotations);
        size_t best_vid = index_mapping[best_rotation_idx];
        auto best_rotation = copy_rotations[best_rotation_idx];
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
        dag.remove_vertex(best_vid);
        index_mapping.erase(index_mapping.begin() + best_rotation_idx);
        
        // auto const parity_graph = get_parity_graph(copy_rotations, best_rotation, "qubit_hamming_weight");
        auto const parity_graph = get_parity_graph(copy_rotations, residual_clifford, best_rotation);
        
        auto const [mst, root] = dvlab::minimum_spanning_arborescence(parity_graph);
        
        apply_mst_cxs(mst, root, copy_rotations, qcir, residual_clifford, num_cxs, backward);
        
        assert(is_valid(copy_rotations[best_rotation_idx]));
        
        copy_rotations.erase(copy_rotations.begin() + best_rotation_idx);
        if (backward) {
            qcir.prepend(qcir::PZGate(best_rotation.phase()), {root});
        } else {
            qcir.append(qcir::PZGate(best_rotation.phase()), {root});
        }
    }

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
