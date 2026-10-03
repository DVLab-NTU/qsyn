#include <algorithm>
#include <chrono>
#include <gsl/narrow>
#include <ranges>
#include <stack>
#include <tl/adjacent.hpp>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>

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

namespace detail::mst {

bool is_valid(PauliRotation const& rotation) {
    if (!rotation.is_diagonal()) {
        return false;
    }

    auto num_z = 0ul;
    for (auto i : std::views::iota(0ul, rotation.n_qubits())) {
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

// get the index of the rotation with the minimum number of 1s in Zs
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

size_t row_hamming_weight(
    std::vector<PauliRotation> const& rotations,
    size_t q_idx, bool is_Z) {
    return std::ranges::count_if(rotations, [&](auto const& rotation) {
        return is_Z ? rotation.pauli_product().is_z_set(q_idx) : rotation.pauli_product().is_x_set(q_idx);
    });
}

size_t row_hamming_weight(
    StabilizerTableau const& st,
    size_t q_idx, bool is_Z) {
    size_t stab_part   = std::ranges::count_if(std::views::iota(0ul, st.n_qubits()), [&](auto i) {
        return is_Z ? st.stabilizer(i).is_z_set(q_idx) : st.stabilizer(i).is_x_set(q_idx);
    });
    size_t destab_part = std::ranges::count_if(std::views::iota(0ul, st.n_qubits()), [&](auto i) {
        return is_Z ? st.destabilizer(i).is_z_set(q_idx) : st.destabilizer(i).is_x_set(q_idx);
    });
    return stab_part + destab_part;
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

size_t cx_distance(
    std::vector<PauliRotation> const& rotations,
    size_t q1_idx,
    size_t q2_idx) {
    size_t x_distance = std::ranges::count_if(rotations, [&](auto const& rotation) {
        return rotation.pauli_product().is_x_set(q1_idx) != rotation.pauli_product().is_x_set(q2_idx);
    });

    return x_distance + hamming_distance(rotations, q1_idx, q2_idx);
}

dvlab::Digraph<size_t, int> get_parity_graph(
    std::vector<PauliRotation> const& rotations,
    PauliRotation const& target_rotation,
    bool consider_X) {
    auto const num_qubits = rotations.front().n_qubits();

    auto g         = dvlab::Digraph<size_t, int>{};
    auto qubit_vec = std::vector<size_t>{};

    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (target_rotation.pauli_product().is_z_set(i)) {
            g.add_vertex_with_id(i);
            qubit_vec.push_back(i);
        }
    }
    // get the weight of the edge i if consider_X is true
    // otherwise, get the weight of the cx operation between i and j
    auto const get_weight = [&](size_t i, size_t j) {
        return consider_X
                   ? row_hamming_weight(rotations, i, true) + row_hamming_weight(rotations, j, false)
                   : row_hamming_weight(rotations, i, true);
    };

    for (auto const& [i, j] : dvlab::combinations<2>(qubit_vec)) {
        auto const dist     = (consider_X)
                                  ? gsl::narrow_cast<int>(cx_distance(rotations, i, j))
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
                   StabilizerTableau& final_clifford, bool backward) {
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

}  // namespace detail::mst

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
    while (!copy_rotations.empty()) {
        auto const best_rotation_idx = detail::mst::get_best_rotation_idx(copy_rotations);
        std::swap(copy_rotations[best_rotation_idx], copy_rotations.back());
        auto const best_rotation = std::move(copy_rotations.back());
        copy_rotations.pop_back();

        auto const parity_graph =
            detail::mst::get_parity_graph(copy_rotations, best_rotation);

        auto const [mst, root] =
            dvlab::minimum_spanning_arborescence(parity_graph);

        detail::mst::apply_mst_cxs(mst, root, copy_rotations, qcir, final_clifford, false);

        // add the rotation at the root
        qcir.append(qcir::PZGate(best_rotation.phase()), {root});
    }

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

}  // namespace qsyn::tableau
