/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau base class and its derived classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>
#include <fmt/format.h>

#include <cstddef>
#include <string>

#include "tableau/pauli_rotation.hpp"

namespace qsyn {

namespace tableau {

class StabilizerTableau : public PauliProductTrait<StabilizerTableau> {
public:
    StabilizerTableau(size_t n_qubits)
        : _stabilizers(2 * n_qubits, PauliProduct(std::string(n_qubits, 'I'))) {
        for (size_t i = 0; i < n_qubits; ++i) {
            _stabilizers[stabilizer_idx(i)].set_pauli_type(i, Pauli::z);
            _stabilizers[destabilizer_idx(i)].set_pauli_type(i, Pauli::x);
        }
    }

    size_t n_qubits() const {
        return _stabilizers.size() / 2;
    }

    size_t stabilizer_idx(size_t qubit) const {
        return qubit;
    }
    size_t destabilizer_idx(size_t qubit) const {
        return qubit + n_qubits();
    }

    StabilizerTableau& h(size_t qubit) noexcept override;
    StabilizerTableau& s(size_t qubit) noexcept override;
    StabilizerTableau& cx(size_t ctrl, size_t targ) noexcept override;
    StabilizerTableau& sdg(size_t qubit) noexcept;
    StabilizerTableau& v(size_t qubit) noexcept;
    StabilizerTableau& vdg(size_t qubit) noexcept;

    // prepend operations
    // these operations are specific to the stabilizer tableau

    StabilizerTableau& prepend_h(size_t qubit);
    StabilizerTableau& prepend_s(size_t qubit);
    StabilizerTableau& prepend_cx(size_t ctrl, size_t targ);

    StabilizerTableau& prepend_sdg(size_t qubit);
    StabilizerTableau& prepend_v(size_t qubit);
    StabilizerTableau& prepend_vdg(size_t qubit);

    StabilizerTableau& prepend_x(size_t qubit);
    StabilizerTableau& prepend_y(size_t qubit);
    StabilizerTableau& prepend_z(size_t qubit);

    StabilizerTableau& prepend_cz(size_t ctrl, size_t targ);
    StabilizerTableau& prepend_swap(size_t a, size_t b);
    StabilizerTableau& prepend_ecr(size_t ctrl, size_t targ);

    StabilizerTableau& prepend(CliffordOperator const& op);
    StabilizerTableau& prepend(CliffordOperatorString const& ops);
    StabilizerTableau& prepend(StabilizerTableau const& tableau);

    std::string to_string() const;
    std::string to_bit_string() const;

    PauliProduct const& stabilizer(size_t qubit) const {
        return _stabilizers[stabilizer_idx(qubit)];
    }
    PauliProduct const& destabilizer(size_t qubit) const {
        return _stabilizers[destabilizer_idx(qubit)];
    }
    PauliProduct& stabilizer(size_t qubit) {
        return _stabilizers[stabilizer_idx(qubit)];
    }
    PauliProduct& destabilizer(size_t qubit) {
        return _stabilizers[destabilizer_idx(qubit)];
    }

    bool operator==(StabilizerTableau const& rhs) const {
        return _stabilizers == rhs._stabilizers;
    }
    bool operator!=(StabilizerTableau const& rhs) const {
        return !(*this == rhs);
    }

    bool is_identity() const { return *this == StabilizerTableau{n_qubits()}; }

    bool is_commutative(PauliProduct const& rhs) const {
        return std::ranges::all_of(
            _stabilizers | std::views::take(n_qubits()),
            [&rhs](PauliProduct const& stabilizer) {
                return stabilizer.is_commutative(rhs);
            });
    }

private:
    std::vector<PauliProduct> _stabilizers;
};

[[nodiscard]] StabilizerTableau adjoint(StabilizerTableau const& tableau);

inline void adjoint_inplace(StabilizerTableau& tableau) {
    tableau = adjoint(tableau);
}

class StabilizerTableauSynthesisStrategy {
public:
    virtual ~StabilizerTableauSynthesisStrategy() = default;

    virtual CliffordOperatorString synthesize(StabilizerTableau copy) const = 0;
};

/**
 * @brief An extractor based on the Aaronson-Gottesman method in the paper
 *        [Improved simulation of stabilizer circuits](https://journals.aps.org/pra/abstract/10.1103/PhysRevA.70.052328)
 *        and the [Qiskit implementation](https://github.com/Qiskit/qiskit/blob/main/qiskit/synthesis/clifford/clifford_decompose_ag.py).
 *
 */
class AGSynthesisStrategy : public StabilizerTableauSynthesisStrategy {
public:
    enum class Mode { ag,
                      ag_plus } mode;
    AGSynthesisStrategy(Mode mode = Mode::ag) : mode{mode} {}
    CliffordOperatorString synthesize(StabilizerTableau copy) const override;
};

CliffordOperatorString
synthesize_cx_pmh(StabilizerTableau tableau,
                  std::optional<size_t> chunk_size = std::nullopt);

CliffordOperatorString
synthesize_cx_pmh_exhaustive(StabilizerTableau const& tableau);

CliffordOperatorString
synthesize_cx_gaussian(StabilizerTableau const& tableau);

CliffordOperatorString
synthesize_h_free_mr(StabilizerTableau tableau);

/**
 * @brief An extractor based on the paper
 *        [Optimal Hadamard gate count for Clifford$+T$ synthesis of
 *        Pauli rotations sequences](https://arxiv.org/abs/2302.07040)
 *        by Vandaele, Martiel, Perdrix, and Vuillot.
 *        This method is guaranteed to produce the optimal number of Hadamard
 *        gates by first diagonalizing the stabilizers with provably optimal
 *        number of Hadamard gates and then applying the Aaronson-Gottesman
 *        method to the rest of the tableau.
 *
 */
class HOptSynthesisStrategy : public StabilizerTableauSynthesisStrategy {
public:
    enum class Mode { star,
                      staircase } mode;
    HOptSynthesisStrategy(Mode mode = Mode::star) : mode{mode} {}
    CliffordOperatorString
    partial_synthesize(StabilizerTableau& clifford) const;
    CliffordOperatorString
    synthesize(StabilizerTableau copy) const override;
};

CliffordOperatorString extract_clifford_operators(
    StabilizerTableau copy,
    StabilizerTableauSynthesisStrategy const& strategy =
        HOptSynthesisStrategy{});

}  // namespace tableau

}  // namespace qsyn

template <>
struct fmt::formatter<qsyn::tableau::StabilizerTableau> {
    char presentation = 'c';
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 'b'))
            presentation = *it++;
        if (it != end && *it != '}')
            detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::tableau::StabilizerTableau const& tableau,
                FormatContext& ctx) const {
        return presentation == 'c'
                   ? format_to(ctx.out(), "{}", tableau.to_string())
                   : format_to(ctx.out(), "{}", tableau.to_bit_string());
    }
};
