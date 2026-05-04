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
    enum class Mode { star,
                      staircase };
    Mode mode;
    GraySynthPauliRotationsSynthesisStrategy(Mode mode = Mode::star) : mode(mode) {}
    std::optional<qcir::QCir> synthesize(std::vector<PauliRotation> const& rotations) const override;
};

/**
 * @brief Synthesize Pauli rotations using minimum spanning arborescence.
 * This method is based on the following paper by Vandaele et al.:
 * https://arxiv.org/abs/2104.00934
 *
 */
struct MstSynthesisStrategy : public PauliRotationsSynthesisStrategy {
    std::optional<qcir::QCir> synthesize(std::vector<PauliRotation> const& rotations) const override;
};

/**
 * @brief NCF-style rotation emission: when all rotations in a block act on a single qubit
 *        (e.g. after `tableau optimize ncf`), emit the conjugated Pauli-string rotations
 *        explicitly without merging/decomposing:
 *          - Z: `rz(θ)`
 *          - X: `h; rz(θ); h`
 *          - Y: `sdg; h; rz(θ); h; s`
 *        This is intended to match the paper-style listing (Cliffords at ends, rotations in middle).
 *        See arXiv:2510.13573 (Non-Clifford Fusion).
 */
struct NcfMergePauliRotationsSynthesisStrategy : public PauliRotationsSynthesisStrategy {
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
