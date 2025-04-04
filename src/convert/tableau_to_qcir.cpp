/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_to_qcir.hpp"

#include <tl/adjacent.hpp>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>

#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::experimental {

namespace detail {

void add_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op) {
    using COT                  = CliffordOperatorType;
    auto const& [type, qubits] = op;

    switch (type) {
        case COT::h:
            qcir.append(qcir::HGate(), {qubits[0]});
            break;
        case COT::s:
            qcir.append(qcir::SGate(), {qubits[0]});
            break;
        case COT::cx:
            qcir.append(qcir::CXGate(), {qubits[0], qubits[1]});
            break;
        case COT::sdg:
            qcir.append(qcir::SdgGate(), {qubits[0]});
            break;
        case COT::v:
            qcir.append(qcir::SXGate(), {qubits[0]});
            break;
        case COT::vdg:
            qcir.append(qcir::SXdgGate(), {qubits[0]});
            break;
        case COT::x:
            qcir.append(qcir::XGate(), {qubits[0]});
            break;
        case COT::y:
            qcir.append(qcir::YGate(), {qubits[0]});
            break;
        case COT::z:
            qcir.append(qcir::ZGate(), {qubits[0]});
            break;
        case COT::cz:
            qcir.append(qcir::CZGate(), {qubits[0], qubits[1]});
            break;
        case COT::swap:
            qcir.append(qcir::SwapGate(), {qubits[0], qubits[1]});
            break;
        case COT::ecr:
            qcir.append(qcir::ECRGate(), {qubits[0], qubits[1]});
            break;
    }
}

}  // namespace detail

/**
 * @brief convert a stabilizer tableau to a QCir.
 *
 * @param clifford - pass by value on purpose
 * @return std::optional<qcir::QCir>
 */
std::optional<qcir::QCir> to_qcir(
    StabilizerTableau const& clifford,
    StabilizerTableauSynthesisStrategy const& strategy) {
    qcir::QCir qcir{clifford.n_qubits()};
    for (auto const& op : extract_clifford_operators(clifford, strategy)) {
        if (stop_requested()) {
            return std::nullopt;
        }
        detail::add_clifford_gate(qcir, op);
    }

    return qcir;
}

/**
 * @brief convert a Pauli rotation to a QCir. This is a naive implementation.
 *
 * @param pauli_rotation
 * @return qcir::QCir
 */
std::optional<qcir::QCir> to_qcir(
    std::vector<PauliRotation> const& pauli_rotations,
    PauliRotationsSynthesisStrategy const& strategy) {
    return strategy.synthesize(pauli_rotations);
}

/**
 * @brief convert a stabilizer tableau and a list of Pauli rotations to a QCir.
 *
 * @param clifford
 * @param pauli_rotations
 * @return qcir::QCir
 */
std::optional<qcir::QCir> to_qcir(
    Tableau const& tableau,
    StabilizerTableauSynthesisStrategy const& st_strategy,
    PauliRotationsSynthesisStrategy const& pr_strategy) {
    qcir::QCir qcir{tableau.n_qubits()};

    for (auto const& subtableau : tableau) {
        if (stop_requested()) {
            return std::nullopt;
        }
        auto const qc_fragment =
            std::visit(
                dvlab::overloaded{
                    [&st_strategy](StabilizerTableau const& st) {
                        return to_qcir(st, st_strategy);
                    },
                    [&pr_strategy](std::vector<PauliRotation> const& pr) {
                        return to_qcir(pr, pr_strategy);
                    }},
                subtableau);
        if (!qc_fragment) {
            return std::nullopt;
        }
        qcir.compose(*qc_fragment);
    }

    return qcir;
}

}  // namespace qsyn::experimental
