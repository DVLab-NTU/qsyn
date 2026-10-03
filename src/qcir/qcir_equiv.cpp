/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define QCir equivalence checking functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/qcir_equiv.hpp"

#include "convert/qcir_to_tableau.hpp"
#include "convert/qcir_to_tensor.hpp"
#include "convert/tableau_to_qcir.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau_optimization.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn::qcir {

bool is_equivalent(QCir const& qcir1, QCir const& qcir2) {
    if (qcir1.get_num_qubits() != qcir2.get_num_qubits()) {
        spdlog::info("The two circuits have different numbers of qubits.");
        return false;
    }

    spdlog::info("Trying to verify equivalence via tableau optimization...");

    auto adjoint_composed = qcir1;
    adjoint_composed.adjoint_inplace();
    adjoint_composed.compose(qcir2);

    auto tableau = qsyn::tableau::to_tableau(adjoint_composed);
    if (!tableau) {
        spdlog::error("Failed to convert adjoint composed QCir to tableau.");
        return false;
    }
    qsyn::tableau::full_optimize(*tableau);

    if (tableau->is_empty()) {
        return true;
    }

    if (adjoint_composed.get_num_qubits() > 7) {
        spdlog::warn("The number of qubits is too large to check equivalence via tensor contraction.");
        spdlog::warn("Please note that this may be a false negative.");
        return false;
    }

    spdlog::info("Cannot prove equivalence via tableau optimization.");
    spdlog::info("Trying to verify equivalence via tensor contraction...");

    auto const optimized_qcir = qsyn::tableau::to_qcir(*tableau, tableau::HOptSynthesisStrategy{}, tableau::NaivePauliRotationsSynthesisStrategy{});

    if (!optimized_qcir) {
        spdlog::error("Failed to convert optimized tableau to QCir.");
        return false;
    }
    auto const tensor1 = qsyn::to_tensor(*optimized_qcir);

    if (!tensor1) {
        spdlog::error("Failed to convert optimized QCir to tensor.");
        return false;
    }

    // checks if the optimized_qcir is close enough to the identity circuit
    return tensor::is_equivalent(*tensor1, tensor::QTensor<double>::identity(optimized_qcir->get_num_qubits()));
}

}  // namespace qsyn::qcir
