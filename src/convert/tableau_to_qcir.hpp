/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/qcir.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/tableau.hpp"

namespace qsyn {

namespace experimental {

using PauliRotationTableau = std::vector<PauliRotation>;

struct PartialSynthesisResult {
    qcir::QCir qcir;
    StabilizerTableau final_clifford;
};

struct PauliRotationsSynthesisStrategy {
public:
    virtual ~PauliRotationsSynthesisStrategy() = default;

    virtual std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const = 0;
};

struct NaivePauliRotationsSynthesisStrategy
    : public PauliRotationsSynthesisStrategy {
    std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const override;
};

struct GraySynthStrategy : public PauliRotationsSynthesisStrategy {
    enum class Mode { star,
                      staircase };
    Mode mode;
    GraySynthStrategy(Mode mode = Mode::star) : mode(mode) {}
    std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const override;
};

/**
 * @brief Synthesize Pauli rotations using minimum spanning arborescence.
 * This method is based on the following paper by Vandaele et al.:
 * https://arxiv.org/abs/2104.00934
 *
 */
struct MstSynthesisStrategy : public PauliRotationsSynthesisStrategy {
    std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const override;
};

std::optional<qcir::QCir> to_qcir(
    StabilizerTableau const& clifford,
    StabilizerTableauSynthesisStrategy const& strategy);
std::optional<qcir::QCir> to_qcir(
    PauliRotationTableau const& rotations,
    PauliRotationsSynthesisStrategy const& strategy);
std::optional<qcir::QCir> to_qcir(
    Tableau const& tableau,
    StabilizerTableauSynthesisStrategy const& st_strategy,
    PauliRotationsSynthesisStrategy const& pr_strategy);

}  // namespace experimental

}  // namespace qsyn
