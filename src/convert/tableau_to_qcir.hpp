/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/qcir.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"

namespace qsyn {

namespace tableau {

using PauliRotationTableau = std::vector<PauliRotation>;

namespace detail {
void add_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op);
void prepend_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op);
void add_clifford_gate(PauliRotationTableau& rotations, CliffordOperator const& op);
void add_clifford_gate(StabilizerTableau& tableau, CliffordOperator const& op);
void prepend_clifford_gate(StabilizerTableau& tableau, CliffordOperator const& op);
}  // namespace detail


struct PauliRotationsSynthesisStrategy {
public:
    virtual ~PauliRotationsSynthesisStrategy() = default;

    virtual std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const = 0;
};

struct PartialSynthesisResult {
    qcir::QCir qcir;
    StabilizerTableau final_clifford;
};

/**
 * @brief Partial synthesis strategy for Pauli rotations.
 * This strategy is used to synthesize a subset of Pauli rotations.
 * The synthesized result is a partial result,
 * which is a pair of a QCir and a StabilizerTableau.
 * The QCir is the synthesized quantum circuit,
 * and the StabilizerTableau is the final Clifford.
 */
struct PartialPauliRotationsSynthesisStrategy
    : public PauliRotationsSynthesisStrategy {
    ~PartialPauliRotationsSynthesisStrategy() override = default;

    virtual std::optional<PartialSynthesisResult>
    partial_synthesize(PauliRotationTableau const& rotations) const = 0;
};

struct BackwardPartialPauliRotationsSynthesisStrategy
    : public PartialPauliRotationsSynthesisStrategy {
    ~BackwardPartialPauliRotationsSynthesisStrategy() override = default;

    virtual std::optional<qcir::QCir>
    backward_synthesize(PauliRotationTableau const& rotations, StabilizerTableau &initial_clifford) const = 0;
};

struct NaivePauliRotationsSynthesisStrategy
    : public PauliRotationsSynthesisStrategy {
    std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const override;
};

struct BasicPauliRotationsSynthesisStrategy
    : public BackwardPartialPauliRotationsSynthesisStrategy {
    
    std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const override;
    
    std::optional<PartialSynthesisResult>
    partial_synthesize(PauliRotationTableau const& rotations) const {
        StabilizerTableau final_clifford = StabilizerTableau{rotations.front().n_qubits()};
        auto qcir = _partial_synthesize(rotations, final_clifford, false);
        if (!qcir.has_value()) {
            return std::nullopt;
        }
        return PartialSynthesisResult{
            std::move(qcir.value()),
            std::move(final_clifford)};
    }
    
    std::optional<qcir::QCir>
    backward_synthesize(PauliRotationTableau const& rotations, StabilizerTableau &initial_clifford) const override{
        return _partial_synthesize(rotations, initial_clifford, true);
    }

    private:
        std::optional<qcir::QCir>
        _partial_synthesize(PauliRotationTableau const& rotations, StabilizerTableau& residual_clifford, bool backward) const;
};

struct GraySynthStrategy : public PartialPauliRotationsSynthesisStrategy {
    enum class Mode { star,
                      staircase };
    Mode mode;
    GraySynthStrategy(Mode mode = Mode::star) : mode(mode) {}
    std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const override;

    std::optional<PartialSynthesisResult>
    partial_synthesize(PauliRotationTableau const& rotations) const override;
};

/**
 * @brief Synthesize Pauli rotations using minimum spanning arborescence.
 * This method is based on the following paper by Vandaele et al.:
 * https://arxiv.org/abs/2104.00934
 *
 */
struct MstSynthesisStrategy : public PartialPauliRotationsSynthesisStrategy {
    std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const override;

    std::optional<PartialSynthesisResult>
    partial_synthesize(PauliRotationTableau const& rotations) const override;
};

struct GeneralizedMstSynthesisStrategy: public BackwardPartialPauliRotationsSynthesisStrategy {
    std::optional<qcir::QCir>
    synthesize(PauliRotationTableau const& rotations) const override;

    std::optional<PartialSynthesisResult>
    partial_synthesize(PauliRotationTableau const& rotations) const override{
        StabilizerTableau final_clifford = StabilizerTableau{rotations.front().n_qubits()};
        auto qcir = _partial_synthesize(rotations, final_clifford, false);
        if (!qcir.has_value()) {
            return std::nullopt;
        }
        return PartialSynthesisResult{
            std::move(qcir.value()),
            std::move(final_clifford)};
    }

    std::optional<qcir::QCir>
    backward_synthesize(PauliRotationTableau const& rotations, StabilizerTableau &initial_clifford) const override{
        return _partial_synthesize(rotations, initial_clifford, true);
    }

    private:
        std::optional<qcir::QCir>
        _partial_synthesize(PauliRotationTableau const& rotations, StabilizerTableau& residual_clifford, bool backward) const;
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
    PauliRotationsSynthesisStrategy const& pr_strategy,
    bool lazy = false, bool backward = false);

}  // namespace tableau

}  // namespace qsyn
