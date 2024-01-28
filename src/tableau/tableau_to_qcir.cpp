/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_to_qcir.hpp"

#include <gsl/narrow>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>

#include "qcir/gate_type.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/tableau.hpp"
#include "util/phase.hpp"

namespace qsyn::experimental {
/**
 * @brief convert a stabilizer tableau to a QCir.
 *        The current implementation is due to [Phys. Rev. A 70, 052328 (2004) - Improved simulation of stabilizer circuits](https://journals.aps.org/pra/abstract/10.1103/PhysRevA.70.052328)
 *
 * @param clifford - pass by value on purpose
 * @return std::optional<qcir::QCir>
 */
qcir::QCir to_qcir(StabilizerTableau clifford) {
    qcir::QCir qcir{clifford.n_qubits()};

    // [ Zs | Xs | rs ]
    // [ Zd | Xd | rd ]

    // fmt::println("Initial Clifford:\n{:b}", clifford);

    auto const add_h = [&clifford, &qcir](size_t qubit) {
        auto gate =
            qcir.add_gate("h", {gsl::narrow<int>(qubit)}, {}, true);
        assert(gate->get_phase() == dvlab::Phase(1));
        clifford.h(qubit);
    };

    auto const add_s = [&clifford, &qcir](size_t qubit) {
        auto gate = qcir.add_gate("s", {gsl::narrow<int>(qubit)}, {}, true);
        assert(gate->get_phase() == dvlab::Phase(1, 2));
        clifford.s(qubit);
    };

    auto const add_cx = [&clifford, &qcir](size_t ctrl, size_t targ) {
        auto gate = qcir.add_gate("cx", {gsl::narrow<int>(ctrl), gsl::narrow<int>(targ)}, {}, true);
        assert(gate->get_phase() == dvlab::Phase(1));
        clifford.cx(ctrl, targ);
    };

    auto const add_z = [&clifford, &qcir](size_t qubit) {
        auto gate = qcir.add_gate("z", {gsl::narrow<int>(qubit)}, {}, true);
        assert(gate->get_phase() == dvlab::Phase(1));
        clifford.z(qubit);
    };

    auto const add_x = [&clifford, &qcir](size_t qubit) {
        auto gate =
            qcir.add_gate("x", {gsl::narrow<int>(qubit)}, {}, true);
        assert(gate->get_phase() == dvlab::Phase(1));
        clifford.x(qubit);
    };

    auto const make_destab_x_main_diag_1 = [&](size_t qubit) {
        if (clifford.destabilizer(qubit).is_x_set(qubit)) return;

        auto const search_idx_range =
            std::views::iota(qubit + 1, clifford.n_qubits());
        auto const ctrl =
            std::ranges::find_if(
                search_idx_range, [&clifford, qubit](size_t t) {
                    if (clifford.destabilizer(qubit).is_x_set(t)) {
                        return true;
                    }
                    return false;
                }) -
            search_idx_range.begin() + qubit + 1;

        if (ctrl < clifford.n_qubits()) {
            add_cx(ctrl, qubit);
            return;
        }

        for (size_t ctrl = qubit; ctrl < clifford.n_qubits(); ++ctrl) {
            if (clifford.destabilizer(qubit).is_z_set(ctrl)) {
                add_h(ctrl);
                if (ctrl != qubit) {
                    add_cx(ctrl, qubit);
                }
                break;
            }
        }
    };

    auto const make_destab_x_off_diag_0 = [&](size_t qubit) {
        for (size_t targ = qubit + 1; targ < clifford.n_qubits(); ++targ) {
            if (clifford.destabilizer(qubit).is_x_set(targ)) {
                add_cx(qubit, targ);
            }
        }

        bool const some_z_set = std::ranges::any_of(
            std::views::iota(qubit, clifford.n_qubits()),
            [&clifford, qubit](size_t t) {
                return clifford.destabilizer(qubit).is_z_set(t);
            });

        if (some_z_set) {
            if (!clifford.destabilizer(qubit).is_z_set(qubit)) {
                add_s(qubit);
            }

            for (size_t ctrl = qubit + 1; ctrl < clifford.n_qubits(); ++ctrl) {
                if (clifford.destabilizer(qubit).is_z_set(ctrl)) {
                    add_cx(ctrl, qubit);
                }
            }
            add_s(qubit);
        }
    };

    auto const make_stab_z_off_diag_0 = [&](size_t qubit) {
        for (size_t ctrl = qubit + 1; ctrl < clifford.n_qubits(); ++ctrl) {
            if (clifford.stabilizer(qubit).is_z_set(ctrl)) {
                add_cx(ctrl, qubit);
            }
        }

        bool const some_x_set = std::ranges::any_of(
            std::views::iota(qubit, clifford.n_qubits()),
            [&clifford, qubit](size_t t) {
                return clifford.stabilizer(qubit).is_x_set(t);
            });

        if (some_x_set) {
            add_h(qubit);

            for (size_t targ = qubit + 1; targ < clifford.n_qubits(); ++targ) {
                if (clifford.stabilizer(qubit).is_x_set(targ)) {
                    add_cx(qubit, targ);
                }
            }

            if (clifford.stabilizer(qubit).is_z_set(qubit)) {
                add_s(qubit);
            }

            add_h(qubit);
        }
    };

    for (size_t qubit = 0; qubit < clifford.n_qubits(); ++qubit) {
        make_destab_x_main_diag_1(qubit);
        make_destab_x_off_diag_0(qubit);
        make_stab_z_off_diag_0(qubit);
    }

    for (size_t qubit = 0; qubit < clifford.n_qubits(); ++qubit) {
        if (clifford.stabilizer(qubit).is_neg()) {
            add_x(qubit);
        }
        if (clifford.destabilizer(qubit).is_neg()) {
            add_z(qubit);
        }
    }

    assert(clifford == StabilizerTableau(qcir.get_num_qubits()));

    qcir.adjoint();

    return qcir;
}

qcir::QCir to_qcir(PauliRotation const& pauli_rotation) {
    qcir::QCir qcir{pauli_rotation.n_qubits()};

    for (size_t i = 0; i < pauli_rotation.n_qubits(); ++i) {
        if (pauli_rotation.get_pauli_type(i) == Pauli::X) {
            qcir.add_gate("h", {gsl::narrow<QubitIdType>(i)}, {}, true);
        } else if (pauli_rotation.get_pauli_type(i) == Pauli::Y) {
            qcir.add_gate("sx", {gsl::narrow<QubitIdType>(i)}, {}, true);
        }
    }

    // get all the qubits that are not I
    auto const non_I_qubits = std::views::iota(0ul, pauli_rotation.n_qubits()) |
                              std::views::filter([&pauli_rotation](auto i) {
                                  return pauli_rotation.get_pauli_type(i) != Pauli::I;
                              }) |
                              tl::to<std::vector>();

    for (auto const& [c, t] : tl::views::adjacent<2>(non_I_qubits)) {
        qcir.add_gate("cx", {gsl::narrow<QubitIdType>(c), gsl::narrow<QubitIdType>(t)}, {}, true);
    }

    qcir.add_gate("pz", {gsl::narrow<QubitIdType>(*(non_I_qubits.end() - 1))}, pauli_rotation.phase(), true);

    for (auto const& [t, c] : tl::views::adjacent<2>(non_I_qubits | std::views::reverse)) {
        qcir.add_gate("cx", {gsl::narrow<QubitIdType>(c), gsl::narrow<QubitIdType>(t)}, {}, true);
    }

    for (size_t i = 0; i < pauli_rotation.n_qubits(); ++i) {
        if (pauli_rotation.get_pauli_type(i) == Pauli::X) {
            qcir.add_gate("h", {gsl::narrow<QubitIdType>(i)}, {}, true);
        } else if (pauli_rotation.get_pauli_type(i) == Pauli::Y) {
            qcir.add_gate("sxdg", {gsl::narrow<QubitIdType>(i)}, {}, true);
        }
    }

    return qcir;
}

qcir::QCir to_qcir(StabilizerTableau const& clifford, std::vector<PauliRotation> const& pauli_rotations) {
    auto qcir = to_qcir(clifford);

    for (auto const& pauli_rotation : pauli_rotations) {
        qcir.compose(to_qcir(pauli_rotation));
    }

    return qcir;
}

}  // namespace qsyn::experimental
