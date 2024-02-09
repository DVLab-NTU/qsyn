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
#include <iostream>
#include <ranges>
#include <string>
#include <sul/dynamic_bitset.hpp>

#include "tableau/pauli_rotation.hpp"

namespace qsyn {

namespace experimental {

class StabilizerTableau : public PauliProductTrait<StabilizerTableau> {
public:
    StabilizerTableau(size_t n_qubits) : _stabilizers(2 * n_qubits, PauliProduct(std::string(n_qubits, 'I'))) {
        for (size_t i = 0; i < n_qubits; ++i) {
            _stabilizers[stabilizer_idx(i)].set_pauli_type(i, Pauli::Z);
            _stabilizers[destabilizer_idx(i)].set_pauli_type(i, Pauli::X);
        }
    }

    inline size_t n_qubits() const {
        return _stabilizers.size() / 2;
    }

    inline size_t stabilizer_idx(size_t qubit) const {
        return qubit;
    }
    inline size_t destabilizer_idx(size_t qubit) const {
        return qubit + n_qubits();
    }

    StabilizerTableau& h(size_t qubit) override;
    StabilizerTableau& s(size_t qubit) override;
    StabilizerTableau& cx(size_t ctrl, size_t targ) override;

    std::string to_string() const;
    std::string to_bit_string() const;

    inline PauliProduct const& stabilizer(size_t qubit) const {
        return _stabilizers[stabilizer_idx(qubit)];
    }
    inline PauliProduct const& destabilizer(size_t qubit) const {
        return _stabilizers[destabilizer_idx(qubit)];
    }
    inline PauliProduct& stabilizer(size_t qubit) {
        return _stabilizers[stabilizer_idx(qubit)];
    }
    inline PauliProduct& destabilizer(size_t qubit) {
        return _stabilizers[destabilizer_idx(qubit)];
    }

    bool operator==(StabilizerTableau const& rhs) const {
        return _stabilizers == rhs._stabilizers;
    }
    bool operator!=(StabilizerTableau const& rhs) const {
        return !(*this == rhs);
    }

private:
    std::vector<PauliProduct> _stabilizers;
};

class StabilizerTableauExtractor {
public:
    virtual ~StabilizerTableauExtractor()                                       = default;
    virtual std::vector<CliffordOperator> extract(StabilizerTableau copy) const = 0;
};

/**
 * @brief An extractor based on the Aaronson-Gottesman method in the paper
 *        [Improved simulation of stabilizer circuits](https://journals.aps.org/pra/abstract/10.1103/PhysRevA.70.052328)
 *        and the [Qiskit implementation](https://github.com/Qiskit/qiskit/blob/main/qiskit/synthesis/clifford/clifford_decompose_ag.py).
 *
 */
class AGExtractor : public StabilizerTableauExtractor {
public:
    std::vector<CliffordOperator> extract(StabilizerTableau copy) const override;
};

/**
 * @brief An extractor based on the paper
 *        [Optimal Hadamard gate count for Clifford$+T$ synthesis of Pauli rotations sequences](https://arxiv.org/abs/2302.07040)
 *        by Vandaele, Martiel, Perdrix, and Vuillot.
 *        This method is guaranteed to produce the optimal number of Hadamard gates by first diagonalizing the stabilizers with
 *        provably optimal number of Hadamard gates and then applying the Aaronson-Gottesman method to the rest of the tableau.
 *
 */
class HOptExtractor : public StabilizerTableauExtractor {
public:
    std::vector<CliffordOperator> extract(StabilizerTableau copy) const override;
};

std::vector<CliffordOperator> extract_clifford_operators(StabilizerTableau copy, StabilizerTableauExtractor const& extractor = AGExtractor{});

}  // namespace experimental

}  // namespace qsyn

template <>
struct fmt::formatter<qsyn::experimental::StabilizerTableau> {
    char presentation = 'c';
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(const qsyn::experimental::StabilizerTableau& tableau, FormatContext& ctx) {
        return presentation == 'c' ? format_to(ctx.out(), "{}", tableau.to_string())
                                   : format_to(ctx.out(), "{}", tableau.to_bit_string());
    }
};
