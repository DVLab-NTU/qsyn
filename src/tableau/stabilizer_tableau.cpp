/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau base class and its derived classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./stabilizer_tableau.hpp"

#include <ranges>
#include <tl/to.hpp>

#include "tableau/pauli_rotation.hpp"

namespace qsyn::experimental {

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

StabilizerTableau& StabilizerTableau::h(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    std::ranges::for_each(_stabilizers, [qubit](PauliProduct& p) { p.h(qubit); });
    return *this;
}

StabilizerTableau& StabilizerTableau::s(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    std::ranges::for_each(_stabilizers, [qubit](PauliProduct& p) { p.s(qubit); });
    return *this;
}

StabilizerTableau& StabilizerTableau::cx(size_t ctrl, size_t targ) {
    if (ctrl >= n_qubits() || targ >= n_qubits()) return *this;
    std::ranges::for_each(_stabilizers, [ctrl, targ](PauliProduct& p) { p.cx(ctrl, targ); });
    return *this;
}

StabilizerTableau adjoint(StabilizerTableau const& tableau) {
    auto ops = extract_clifford_operators(tableau);

    adjoint_inplace(ops);

    StabilizerTableau ret{tableau.n_qubits()};
    std::ranges::for_each(ops, [&ret](auto const& op) { ret.apply(op); });

    return ret;
}

CliffordOperatorString extract_clifford_operators(StabilizerTableau copy, StabilizerTableauExtractor const& extractor) {
    return extractor.extract(copy);
}

CliffordOperatorString AGExtractor::extract(StabilizerTableau copy) const {
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
                    if (copy.destabilizer(qubit).is_x_set(t)) {
                        return true;
                    }
                    return false;
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
        make_destab_x_main_diag_1(qubit);
        make_destab_x_off_diag_0(qubit);
        make_stab_z_off_diag_0(qubit);
    }

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

CliffordOperatorString HOptExtractor::extract(StabilizerTableau copy) const {
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
        auto const qubit_range = std::views::iota(0ul, copy.n_qubits());
        auto qubit_it          = std::ranges::find_if(
            qubit_range,
            [&, i](size_t t) {
                return copy.stabilizer(i).is_x_set(t);
            });

        if (qubit_it == qubit_range.end()) continue;

        auto const ctrl = gsl::narrow<size_t>(qubit_it - qubit_range.begin());

        for (size_t targ = ctrl + 1; targ < copy.n_qubits(); ++targ) {
            if (copy.stabilizer(i).is_x_set(targ)) {
                add_cx(ctrl, targ);
            }
        }

        if (copy.stabilizer(i).is_z_set(i)) {
            add_s(i);
        }

        add_h(ctrl);
    }

    // synthesize the now diagonal stabilizers with Aaronson-Gottesman method

    auto clifford_ops = extract_clifford_operators(copy, AGExtractor{});

    assert(std::ranges::none_of(clifford_ops, [](auto const& op) {
        return op.first == CliffordOperatorType::h;
    }));

    adjoint_inplace(clifford_ops);

    clifford_ops.insert(clifford_ops.end(), diag_ops.begin(), diag_ops.end());

    return clifford_ops;
}

}  // namespace qsyn::experimental
