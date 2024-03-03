/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_to_qcir.hpp"

#include <functional>
#include <gsl/narrow>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>

#include "qcir/gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/phase.hpp"

namespace qsyn::experimental {

namespace {

void add_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op) {
    using COT                  = CliffordOperatorType;
    auto const& [type, qubits] = op;

    switch (type) {
        case COT::h:
            qcir.add_gate("h", {gsl::narrow<QubitIdType>(qubits[0])}, {}, true);
            break;
        case COT::s:
            qcir.add_gate("s", {gsl::narrow<QubitIdType>(qubits[0])}, {}, true);
            break;
        case COT::cx:
            qcir.add_gate("cx", {gsl::narrow<QubitIdType>(qubits[0]), gsl::narrow<QubitIdType>(qubits[1])}, {}, true);
            break;
        case COT::sdg:
            qcir.add_gate("sdg", {gsl::narrow<QubitIdType>(qubits[0])}, {}, true);
            break;
        case COT::v:
            qcir.add_gate("sx", {gsl::narrow<QubitIdType>(qubits[0])}, {}, true);
            break;
        case COT::vdg:
            qcir.add_gate("sxdg", {gsl::narrow<QubitIdType>(qubits[0])}, {}, true);
            break;
        case COT::x:
            qcir.add_gate("x", {gsl::narrow<QubitIdType>(qubits[0])}, {}, true);
            break;
        case COT::y:
            qcir.add_gate("y", {gsl::narrow<QubitIdType>(qubits[0])}, {}, true);
            break;
        case COT::z:
            qcir.add_gate("z", {gsl::narrow<QubitIdType>(qubits[0])}, {}, true);
            break;
        case COT::cz:
            qcir.add_gate("cz", {gsl::narrow<QubitIdType>(qubits[0]), gsl::narrow<QubitIdType>(qubits[1])}, {}, true);
            break;
        case COT::swap:
            qcir.add_gate("swap", {gsl::narrow<QubitIdType>(qubits[0]), gsl::narrow<QubitIdType>(qubits[1])}, {}, true);
            break;
    }
}
}  // namespace

/**
 * @brief convert a stabilizer tableau to a QCir.
 *
 * @param clifford - pass by value on purpose
 * @return std::optional<qcir::QCir>
 */
qcir::QCir to_qcir(StabilizerTableau clifford, StabilizerTableauSynthesisStrategy const& strategy) {
    qcir::QCir qcir{clifford.n_qubits()};
    for (auto const& op : extract_clifford_operators(clifford, strategy)) {
        add_clifford_gate(qcir, op);
    }

    return qcir;
}

qcir::QCir NaivePauliRotationsSynthesisStrategy::synthesize(std::vector<PauliRotation> const& rotations) const {
    if (rotations.empty()) {
        return qcir::QCir{0};
    }

    qcir::QCir qcir{rotations.front().n_qubits()};

    for (auto const& rotation : rotations) {
        auto [ops, qubit] = extract_clifford_operators(rotation);

        for (auto const& op : ops) {
            add_clifford_gate(qcir, op);
        }

        qcir.add_gate("pz", {gsl::narrow<QubitIdType>(qubit)}, rotation.phase(), true);

        adjoint_inplace(ops);

        for (auto const& op : ops) {
            add_clifford_gate(qcir, op);
        }
    }

    return qcir;
}

/**
 * @brief convert a Pauli rotation to a QCir. This is a naive implementation.
 *
 * @param pauli_rotation
 * @return qcir::QCir
 */
qcir::QCir to_qcir(std::vector<PauliRotation> const& pauli_rotations, PauliRotationsSynthesisStrategy const& strategy) {
    return strategy.synthesize(pauli_rotations);
}

/**
 * @brief convert a stabilizer tableau and a list of Pauli rotations to a QCir.
 *
 * @param clifford
 * @param pauli_rotations
 * @return qcir::QCir
 */
qcir::QCir to_qcir(Tableau const& tableau, StabilizerTableauSynthesisStrategy const& st_strategy, PauliRotationsSynthesisStrategy const& pr_strategy) {
    qcir::QCir qcir{tableau.n_qubits()};

    std::ranges::for_each(tableau, [&](auto const& sub_tableau) {
        std::visit(
            dvlab::overloaded{
                [&qcir, &st_strategy](StabilizerTableau const& st) {
                    qcir.compose(to_qcir(st, st_strategy));
                },
                [&qcir, &pr_strategy](std::vector<PauliRotation> const& pr) {
                    qcir.compose(to_qcir(pr, pr_strategy));
                }},
            sub_tableau);
    });

    return qcir;
}

}  // namespace qsyn::experimental
