/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau base class and its derived classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./stabilizer_tableau.hpp"

#include <ranges>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>

#include "tableau/pauli_rotation.hpp"

bool stop_requested();

namespace qsyn::tableau {

std::string StabilizerTableau::to_string() const {
    std::string ret;
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("S{}  {:+c}\n", i, _stabilizers[stabilizer_idx(i)]);
    }
    ret += '\n';
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("D{}  {:+c}\n", i, _stabilizers[destabilizer_idx(i)]);
    }
    return ret;
}

std::string StabilizerTableau::to_bit_string() const {
    std::string ret;
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("S{}  {:b}\n", i, _stabilizers[stabilizer_idx(i)]);
    }
    ret += '\n';
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("D{}  {:b}\n", i, _stabilizers[destabilizer_idx(i)]);
    }
    return ret;
}

StabilizerTableau& StabilizerTableau::h(size_t qubit) noexcept {
    if (qubit >= n_qubits()) return *this;
    std::ranges::for_each(_stabilizers, [qubit](PauliProduct& p) { p.h(qubit); });
    return *this;
}

StabilizerTableau& StabilizerTableau::s(size_t qubit) noexcept {
    if (qubit >= n_qubits()) return *this;
    std::ranges::for_each(_stabilizers, [qubit](PauliProduct& p) { p.s(qubit); });
    return *this;
}

StabilizerTableau& StabilizerTableau::cx(size_t ctrl, size_t targ) noexcept {
    if (ctrl >= n_qubits() || targ >= n_qubits()) return *this;
    std::ranges::for_each(_stabilizers, [ctrl, targ](PauliProduct& p) { p.cx(ctrl, targ); });
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend_h(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    std::swap(stabilizer(qubit), destabilizer(qubit));
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend_s(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    destabilizer(qubit) = stabilizer(qubit) * destabilizer(qubit);
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend_cx(size_t ctrl, size_t targ) {
    if (ctrl >= n_qubits() || targ >= n_qubits()) return *this;
    stabilizer(targ)   = stabilizer(ctrl) * stabilizer(targ);
    destabilizer(ctrl) = destabilizer(targ) * destabilizer(ctrl);
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend_sdg(size_t qubit) {
    return prepend_s(qubit).prepend_s(qubit).prepend_s(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_v(size_t qubit) {
    return prepend_h(qubit).prepend_s(qubit).prepend_h(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_vdg(size_t qubit) {
    return prepend_h(qubit).prepend_sdg(qubit).prepend_h(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_x(size_t qubit) {
    return prepend_h(qubit).prepend_z(qubit).prepend_h(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_y(size_t qubit) {
    return prepend_x(qubit).prepend_z(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_z(size_t qubit) {
    return prepend_s(qubit).prepend_s(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_cz(size_t ctrl, size_t targ) {
    return prepend_h(targ).prepend_cx(ctrl, targ).prepend_h(targ);
}

StabilizerTableau& StabilizerTableau::prepend_swap(size_t a, size_t b) {
    return prepend_cx(a, b).prepend_cx(b, a).prepend_cx(a, b);
}

StabilizerTableau& StabilizerTableau::prepend_ecr(size_t ctrl, size_t targ) {
    return prepend_x(ctrl).prepend_s(ctrl).prepend_v(targ).prepend_cx(ctrl, targ);
}

StabilizerTableau& StabilizerTableau::prepend(CliffordOperator const& op) {
    auto const& [type, qubits] = op;
    switch (type) {
        case CliffordOperatorType::h:
            return prepend_h(qubits[0]);
        case CliffordOperatorType::s:
            return prepend_s(qubits[0]);
        case CliffordOperatorType::cx:
            return prepend_cx(qubits[0], qubits[1]);
        case CliffordOperatorType::sdg:
            return prepend_sdg(qubits[0]);
        case CliffordOperatorType::v:
            return prepend_v(qubits[0]);
        case CliffordOperatorType::vdg:
            return prepend_vdg(qubits[0]);
        case CliffordOperatorType::x:
            return prepend_x(qubits[0]);
        case CliffordOperatorType::y:
            return prepend_y(qubits[0]);
        case CliffordOperatorType::z:
            return prepend_z(qubits[0]);
        case CliffordOperatorType::cz:
            return prepend_cz(qubits[0], qubits[1]);
        case CliffordOperatorType::swap:
            return prepend_swap(qubits[0], qubits[1]);
        case CliffordOperatorType::ecr:
            return prepend_ecr(qubits[0], qubits[1]);
    }
    DVLAB_UNREACHABLE("Every Clifford type should be handled in the switch-case");
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend(CliffordOperatorString const& ops) {
    for (auto const& op : ops) {
        prepend(op);
    }
    return *this;
}

StabilizerTableau adjoint(StabilizerTableau const& tableau) {
    return StabilizerTableau{tableau.n_qubits()}.apply(adjoint(extract_clifford_operators(tableau)));
}

CliffordOperatorString extract_clifford_operators(StabilizerTableau copy, StabilizerTableauSynthesisStrategy const& strategy) {
    return strategy.synthesize(std::move(copy));
}
CliffordOperatorString AGSynthesisStrategy::synthesize(StabilizerTableau copy) const {
    CliffordOperatorString clifford_ops;

    auto const add_cx = [&](size_t ctrl, size_t targ) {
        copy.cx(ctrl, targ);
        clifford_ops.push_back({CliffordOperatorType::cx, {ctrl, targ}});
    };

    auto const add_h = [&](size_t qubit) {
        copy.h(qubit);
        clifford_ops.push_back({CliffordOperatorType::h, {qubit, 0}});
    };

    auto const add_s = [&](size_t qubit) {
        copy.s(qubit);
        clifford_ops.push_back({CliffordOperatorType::s, {qubit, 0}});
    };

    auto const add_x = [&](size_t qubit) {
        copy.x(qubit);
        clifford_ops.push_back({CliffordOperatorType::x, {qubit, 0}});
    };

    auto const add_z = [&](size_t qubit) {
        copy.z(qubit);
        clifford_ops.push_back({CliffordOperatorType::z, {qubit, 0}});
    };

    auto const make_destab_x_main_diag_1 = [&](size_t qubit) {
        if (copy.destabilizer(qubit).is_x_set(qubit)) return;

        auto const search_idx_range =
            std::views::iota(qubit + 1, copy.n_qubits());
        auto const ctrl = gsl::narrow<size_t>(
            std::ranges::find_if(
                search_idx_range, [&, qubit](size_t t) {
                    return copy.destabilizer(qubit).is_x_set(t);
                }) -
            search_idx_range.begin() + qubit + 1);

        if (ctrl < copy.n_qubits()) {
            add_cx(ctrl, qubit);
            return;
        }

        for (size_t ctrl = qubit; ctrl < copy.n_qubits(); ++ctrl) {
            if (copy.destabilizer(qubit).is_z_set(ctrl)) {
                add_h(ctrl);
                if (ctrl != qubit) {
                    add_cx(ctrl, qubit);
                }
                break;
            }
        }
    };

    auto const make_destab_x_off_diag_0 = [&](size_t qubit) {
        for (size_t targ = qubit + 1; targ < copy.n_qubits(); ++targ) {
            if (copy.destabilizer(qubit).is_x_set(targ)) {
                add_cx(qubit, targ);
            }
        }

        bool const some_z_set = std::ranges::any_of(
            std::views::iota(qubit, copy.n_qubits()),
            [&, qubit](size_t t) {
                return copy.destabilizer(qubit).is_z_set(t);
            });

        if (some_z_set) {
            if (!copy.destabilizer(qubit).is_z_set(qubit)) {
                add_s(qubit);
            }

            for (size_t ctrl = qubit + 1; ctrl < copy.n_qubits(); ++ctrl) {
                if (copy.destabilizer(qubit).is_z_set(ctrl)) {
                    add_cx(ctrl, qubit);
                }
            }
            add_s(qubit);
        }
    };

    auto const make_stab_z_off_diag_0 = [&](size_t qubit) {
        for (size_t ctrl = qubit + 1; ctrl < copy.n_qubits(); ++ctrl) {
            if (copy.stabilizer(qubit).is_z_set(ctrl)) {
                add_cx(ctrl, qubit);
            }
        }

        bool const some_x_set = std::ranges::any_of(
            std::views::iota(qubit, copy.n_qubits()),
            [&, qubit](size_t t) {
                return copy.stabilizer(qubit).is_x_set(t);
            });

        if (some_x_set) {
            add_h(qubit);

            for (size_t targ = qubit + 1; targ < copy.n_qubits(); ++targ) {
                if (copy.stabilizer(qubit).is_x_set(targ)) {
                    add_cx(qubit, targ);
                }
            }

            if (copy.stabilizer(qubit).is_z_set(qubit)) {
                add_s(qubit);
            }

            add_h(qubit);
        }
    };

    for (size_t qubit = 0; qubit < copy.n_qubits(); ++qubit) {
        if (stop_requested()) break;
        make_destab_x_main_diag_1(qubit);
        make_destab_x_off_diag_0(qubit);
        make_stab_z_off_diag_0(qubit);
    }

    if (stop_requested()) return {};

    for (size_t qubit = 0; qubit < copy.n_qubits(); ++qubit) {
        if (copy.stabilizer(qubit).is_neg()) {
            add_x(qubit);
        }
        if (copy.destabilizer(qubit).is_neg()) {
            add_z(qubit);
        }
    }

    adjoint_inplace(clifford_ops);

    return clifford_ops;
}

namespace {
std::vector<size_t> get_qubits_with_stabilizer_set(StabilizerTableau const& tableau, size_t qubit) {
    std::vector<size_t> qubits_with_stabilizer_set;
    for (size_t i = 0; i < tableau.n_qubits(); ++i) {
        if (tableau.stabilizer(qubit).is_x_set(i)) {
            qubits_with_stabilizer_set.push_back(i);
        }
    }
    return qubits_with_stabilizer_set;
}
}  // namespace

CliffordOperatorString HOptSynthesisStrategy::synthesize(StabilizerTableau copy) const {
    CliffordOperatorString diag_ops;

    auto const add_cx = [&](size_t ctrl, size_t targ) {
        copy.cx(ctrl, targ);
        diag_ops.push_back({CliffordOperatorType::cx, {ctrl, targ}});
    };

    auto const add_h = [&](size_t qubit) {
        copy.h(qubit);
        diag_ops.push_back({CliffordOperatorType::h, {qubit, 0}});
    };

    auto const add_s = [&](size_t qubit) {
        copy.s(qubit);
        diag_ops.push_back({CliffordOperatorType::s, {qubit, 0}});
    };

    // diagonalize all stabilizers
    for (size_t i = 0; i < copy.n_qubits(); ++i) {
        if (stop_requested()) break;
        auto const qubit_range = std::views::iota(0ul, copy.n_qubits());
        auto const qubits_with_stabilizer_set =
            get_qubits_with_stabilizer_set(copy, i);

        if (qubits_with_stabilizer_set.empty()) continue;

        auto const ctrl = qubits_with_stabilizer_set.front();
        if (mode == Mode::star) {
            for (auto const targ :
                 qubits_with_stabilizer_set | std::views::drop(1)) {
                add_cx(ctrl, targ);
            }

        } else /* staircase */ {
            for (auto&& [t, c] :
                 qubits_with_stabilizer_set |
                     std::views::reverse |
                     tl::views::pairwise) {
                add_cx(c, t);
            }
        }

        if (copy.stabilizer(i).is_z_set(ctrl)) {
            add_s(ctrl);
        }

        add_h(ctrl);
    }

    if (stop_requested()) return {};

    // synthesize the now diagonal stabilizers with Aaronson-Gottesman method

    auto clifford_ops = extract_clifford_operators(copy, AGSynthesisStrategy{});

    assert(std::ranges::none_of(clifford_ops, [](auto const& op) {
        return op.first == CliffordOperatorType::h;
    }));

    adjoint_inplace(diag_ops);

    clifford_ops.insert(clifford_ops.end(), diag_ops.begin(), diag_ops.end());

    return clifford_ops;
}

}  // namespace qsyn::tableau
