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

/**
 * @brief convert a stabilizer tableau to a QCir.
 *
 * @param clifford - pass by value on purpose
 * @return std::optional<qcir::QCir>
 */
qcir::QCir to_qcir(StabilizerTableau clifford, StabilizerTableauExtractor const& extractor) {
    auto const clifford_ops = extract_clifford_operators(clifford, extractor);
    qcir::QCir qcir{clifford.n_qubits()};

    for (auto const& [type, qubits] : clifford_ops) {
        using COT = CliffordOperatorType;
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

    return qcir;
}

/**
 * @brief convert a Pauli rotation to a QCir. This is a naive implementation.
 *
 * @param pauli_rotation
 * @return qcir::QCir
 */
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

    qcir.add_gate("pz", {gsl::narrow<QubitIdType>(non_I_qubits.back())}, pauli_rotation.phase(), true);

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

/**
 * @brief convert a stabilizer tableau and a list of Pauli rotations to a QCir.
 *
 * @param clifford
 * @param pauli_rotations
 * @return qcir::QCir
 */
qcir::QCir to_qcir(StabilizerTableau const& clifford, std::vector<PauliRotation> const& pauli_rotations, StabilizerTableauExtractor const& extractor) {
    auto qcir = to_qcir(clifford, extractor);

    for (auto const& pauli_rotation : pauli_rotations) {
        qcir.compose(to_qcir(pauli_rotation));
    }

    return qcir;
}

}  // namespace qsyn::experimental
