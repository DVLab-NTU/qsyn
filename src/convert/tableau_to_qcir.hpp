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

struct PauliRotationsSynthesisStrategy {
public:
    virtual ~PauliRotationsSynthesisStrategy() = default;

    virtual std::optional<qcir::QCir> synthesize(std::vector<PauliRotation> const& rotations) const = 0;
};

struct NaivePauliRotationsSynthesisStrategy : public PauliRotationsSynthesisStrategy {
    std::optional<qcir::QCir> synthesize(std::vector<PauliRotation> const& rotations) const override;
};

struct TParPauliRotationsSynthesisStrategy : public PauliRotationsSynthesisStrategy {
    std::optional<qcir::QCir> synthesize(std::vector<PauliRotation> const& rotations) const override;
};

struct GraySynthPauliRotationsSynthesisStrategy : public PauliRotationsSynthesisStrategy {
    std::optional<qcir::QCir> synthesize(std::vector<PauliRotation> const& rotations) const override;
};

std::optional<qcir::QCir> to_qcir(
    StabilizerTableau const& clifford,
    StabilizerTableauSynthesisStrategy const& strategy);
std::optional<qcir::QCir> to_qcir(
    std::vector<PauliRotation> const& rotations,
    PauliRotationsSynthesisStrategy const& strategy);
std::optional<qcir::QCir> to_qcir(
    Tableau const& tableau,
    StabilizerTableauSynthesisStrategy const& st_strategy,
    PauliRotationsSynthesisStrategy const& pr_strategy);

}  // namespace experimental

}  // namespace qsyn
