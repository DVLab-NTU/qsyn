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
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
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
qcir::QCir to_qcir(StabilizerTableau clifford, StabilizerTableauExtractor const& extractor) {
    qcir::QCir qcir{clifford.n_qubits()};
    for (auto const& op : extract_clifford_operators(clifford, extractor)) {
        add_clifford_gate(qcir, op);
    }

    return qcir;
}

/**
 * @brief convert a Pauli rotation to a QCir. This is a naive implementation.
 *
 * @param pauli_rotation
 * @return qcir::QCir
 */
qcir::QCir to_qcir(PauliRotation const& pauli_rotation) {
    auto [ops, qubit] = extract_clifford_operators(pauli_rotation);

    qcir::QCir qcir{pauli_rotation.n_qubits()};

    for (auto const& op : ops) {
        add_clifford_gate(qcir, op);
    }

    qcir.add_gate("pz", {gsl::narrow<QubitIdType>(qubit)}, pauli_rotation.phase(), true);

    adjoint_inplace(ops);

    for (auto const& op : ops) {
        add_clifford_gate(qcir, op);
    }

    return qcir;
}

/**
 * @brief convert a stabilizer tableau and a list of Pauli rotations to a QCir.
 *
 * @param clifford
 * @param pauli_rotations
 * @return qcir::QCir
 */
qcir::QCir to_qcir(Tableau const& tableau, StabilizerTableauExtractor const& extractor) {
    qcir::QCir qcir{tableau.front().clifford.n_qubits()};

    std::ranges::for_each(tableau, [&qcir, &extractor](auto const& sub_tableau) {
        qcir.compose(to_qcir(sub_tableau.clifford, extractor));
        for (auto const& pauli_rotation : sub_tableau.pauli_rotations) {
            qcir.compose(to_qcir(pauli_rotation));
        }
    });

    return qcir;
}

}  // namespace qsyn::experimental
