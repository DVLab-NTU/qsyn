/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_to_qcir.hpp"
#include <spdlog/spdlog.h>

#include <functional>
#include <gsl/narrow>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>
#include <unordered_map>

#include "qcir/gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/phase.hpp"
#include "util/boolean_matrix.hpp"

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
        case COT::ecr:
            qcir.add_gate("ecr", {gsl::narrow<QubitIdType>(qubits[0]), gsl::narrow<QubitIdType>(qubits[1])}, {}, true);
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
std::optional<qcir::QCir> to_qcir(StabilizerTableau const& clifford, StabilizerTableauSynthesisStrategy const& strategy) {
    qcir::QCir qcir{clifford.n_qubits()};
    for (auto const& op : extract_clifford_operators(clifford, strategy)) {
        add_clifford_gate(qcir, op);
    }

    return qcir;
}

std::optional<qcir::QCir> NaivePauliRotationsSynthesisStrategy::synthesize(std::vector<PauliRotation> const& rotations) const {
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

std::optional<qcir::QCir> TParPauliRotationsSynthesisStrategy::synthesize(std::vector<PauliRotation> const& rotations ) const {
    
    if (rotations.empty()) {
        return qcir::QCir{0};
    }

    qcir::QCir qcir{rotations.front().n_qubits()};
    
    // assert it is a valid partition
    // assert all 0f its type are z axis

    std::unordered_map<size_t, size_t> permutation_map;
    CliffordOperatorString clifford_ops;

    auto const add_cx = [&](size_t ctrl, size_t targ) {;
        clifford_ops.push_back({CliffordOperatorType::cx, {ctrl, targ}});
    };

    // load to the uncomplete_matrix
    size_t row = rotations.size();
    size_t col = rotations[0].n_qubits();
    dvlab::BooleanMatrix matrix(row, col);
    for (size_t i = 0; i < row; ++i) {
        for (size_t j = 0; j < col; ++j) {
            matrix[i][j] = rotations[i].pauli_product().is_z_set(j);
        }
    }

    // synthesis concept:
    // Id matrix --(first_row_ops)--> completed matrix --(second_row_ops)--> uncompleted matrix + unchanged qubits
    // ex:
    // 1000      1100     1011
    // 0100  ->  0110 ->  0110
    // 0010      0010     0010 (unchanged)
    // 0001      0001     1010
    matrix.gaussian_elimination_skip(rotations.size(), true, true);
    auto second_row_ops = matrix.get_row_operations();

    
    // build first_row_ops
    for(size_t i=0; i< row; i++){
        bool first_one_in_the_row = true;
        for(size_t j=0; j< col; j++){
            if(matrix[i][j]==0) continue;

            if(first_one_in_the_row){
                // assert i not in permutation map
                permutation_map[i] = j;
                first_one_in_the_row = false;
                continue;
            }

            add_cx(j, i);
        }
    }

    for(auto& [c, t]: second_row_ops){
        add_cx(permutation_map[c], permutation_map[t]);
    }

    // synthesis process
    for (auto const& op : clifford_ops) {
            add_clifford_gate(qcir, op);
    }

    for(size_t i=0; i<row; i++){

        qcir.add_gate("pz", {gsl::narrow<QubitIdType>(permutation_map[i])}, rotations[i].phase(), true);
    }
    

    adjoint_inplace(clifford_ops);

    for (auto const& op : clifford_ops) {
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
std::optional<qcir::QCir> to_qcir(std::vector<PauliRotation> const& pauli_rotations, PauliRotationsSynthesisStrategy const& strategy) {
    return strategy.synthesize(pauli_rotations);
}

/**
 * @brief convert a stabilizer tableau and a list of Pauli rotations to a QCir.
 *
 * @param clifford
 * @param pauli_rotations
 * @return qcir::QCir
 */
std::optional<qcir::QCir> to_qcir(Tableau const& tableau, StabilizerTableauSynthesisStrategy const& st_strategy, PauliRotationsSynthesisStrategy const& pr_strategy) {
    qcir::QCir qcir{tableau.n_qubits()};

    for (auto const& subtableau : tableau) {
        auto const qc_fragment =
            std::visit(
                dvlab::overloaded{
                    [&st_strategy](StabilizerTableau const& st) { return to_qcir(st, st_strategy); },
                    [&pr_strategy](std::vector<PauliRotation> const& pr) { return to_qcir(pr, pr_strategy); }},
                subtableau);
        if (!qc_fragment) {
            return std::nullopt;
        }
        qcir.compose(qc_fragment.value());
    }

    return qcir;
}

}  // namespace qsyn::experimental
