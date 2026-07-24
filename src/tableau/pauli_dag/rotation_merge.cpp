/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ staq-compatible rotation merge primitives. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./rotation_merge.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

namespace {

[[nodiscard]] PauliProduct propagate_through(PauliProduct const& p,
                                             StabilizerTableau const& c) {
    auto result    = p;
    auto const ops = extract_clifford_operators(c);
    result.apply(ops);
    return result;
}

}  // namespace

std::optional<std::pair<dvlab::Phase, PauliRotation>>
try_merge_into_earlier(PauliRotation const& later, PauliRotation const& earlier) {
    auto const& p_l = later.pauli_product();
    auto const& p_e = earlier.pauli_product();

    if (p_l == p_e) {
        return std::make_pair(dvlab::Phase{0},
                              PauliRotation{p_e, earlier.phase() + later.phase()});
    }

    auto neg_e = p_e;
    neg_e.negate();
    if (p_l == neg_e) {
        return std::make_pair(later.phase(),
                              PauliRotation{p_e, earlier.phase() - later.phase()});
    }

    return std::nullopt;
}

bool rotations_commute(PauliRotation const& a, PauliRotation const& b) {
    return is_commutative(a.pauli_product(), b.pauli_product());
}

PauliRotation commute_left(PauliRotation const& r, StabilizerTableau const& c) {
    if (c.is_identity()) return r;
    auto p = propagate_through(r.pauli_product(), c);
    return PauliRotation{std::move(p), r.phase()};
}

}  // namespace qsyn::experimental::cpf::pauli_dag
