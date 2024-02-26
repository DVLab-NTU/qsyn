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
#include <initializer_list>
#include <iostream>
#include <iterator>
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

    StabilizerTableau& prepend(CliffordOperator const& op);
    StabilizerTableau& prepend(CliffordOperatorString const& ops);

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

    inline bool is_identity() const { return *this == StabilizerTableau{n_qubits()}; }

private:
    std::vector<PauliProduct> _stabilizers;
};

[[nodiscard]] StabilizerTableau adjoint(StabilizerTableau const& tableau);

inline void adjoint_inplace(StabilizerTableau& tableau) { tableau = adjoint(tableau); }

class StabilizerTableauExtractor {
public:
    virtual ~StabilizerTableauExtractor()                                = default;
    virtual CliffordOperatorString extract(StabilizerTableau copy) const = 0;
};

/**
 * @brief An extractor based on the Aaronson-Gottesman method in the paper
 *        [Improved simulation of stabilizer circuits](https://journals.aps.org/pra/abstract/10.1103/PhysRevA.70.052328)
 *        and the [Qiskit implementation](https://github.com/Qiskit/qiskit/blob/main/qiskit/synthesis/clifford/clifford_decompose_ag.py).
 *
 */
class AGExtractor : public StabilizerTableauExtractor {
public:
    CliffordOperatorString extract(StabilizerTableau copy) const override;
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
    CliffordOperatorString extract(StabilizerTableau copy) const override;
};

CliffordOperatorString extract_clifford_operators(StabilizerTableau copy, StabilizerTableauExtractor const& extractor = HOptExtractor{});

struct SubTableau : public PauliProductTrait<SubTableau> {
    SubTableau(StabilizerTableau const& clifford, std::vector<PauliRotation> const& pauli_rotations)
        : clifford{clifford}, pauli_rotations{pauli_rotations} {}
    SubTableau& h(size_t qubit) override;
    SubTableau& s(size_t qubit) override;
    SubTableau& cx(size_t control, size_t target) override;

    StabilizerTableau clifford;
    std::vector<PauliRotation> pauli_rotations;
};

class Tableau : public PauliProductTrait<Tableau> {
public:
    Tableau(size_t n_qubits) : _subtableaux{SubTableau{StabilizerTableau{n_qubits}, {}}} {}
    Tableau(std::initializer_list<SubTableau> subtableaux) : _subtableaux{subtableaux} {}

    auto begin() const { return _subtableaux.begin(); }
    auto end() const { return _subtableaux.end(); }
    auto begin() { return _subtableaux.begin(); }
    auto end() { return _subtableaux.end(); }

    auto size() const { return _subtableaux.size(); }

    auto const& front() const { return _subtableaux.front(); }
    auto const& back() const { return _subtableaux.back(); }
    auto& front() { return _subtableaux.front(); }
    auto& back() { return _subtableaux.back(); }

    auto n_qubits() const { return _subtableaux.front().clifford.n_qubits(); }

    auto insert(std::vector<SubTableau>::iterator pos, std::vector<SubTableau>::iterator first, std::vector<SubTableau>::iterator last) {
        return _subtableaux.insert(pos, first, last);
    }

    auto insert(std::vector<SubTableau>::iterator pos, SubTableau const& subtableau) { return _subtableaux.insert(pos, subtableau); }

    auto erase(std::vector<SubTableau>::iterator first, std::vector<SubTableau>::iterator last) {
        return _subtableaux.erase(first, last);
    }

    auto push_back(SubTableau const& subtableau) { return _subtableaux.push_back(subtableau); }

    template <typename... Args>
    auto emplace_back(Args&&... args) {
        return _subtableaux.emplace_back(std::forward<Args>(args)...);
    }

    auto erase(std::ranges::range auto const& range) { return _subtableaux.erase(range); }

    auto get_filename() const { return _filename; }
    auto set_filename(std::string const& filename) { _filename = filename; }

    auto get_procedures() const { return _procedures; }
    auto add_procedure(std::string const& procedure) { _procedures.push_back(procedure); }
    auto add_procedures(std::vector<std::string> const& procedures) { _procedures.insert(_procedures.end(), procedures.begin(), procedures.end()); }

    Tableau& h(size_t qubit) override {
        this->back().h(qubit);
        return *this;
    }

    Tableau& s(size_t qubit) override {
        this->back().s(qubit);
        return *this;
    }

    Tableau& cx(size_t control, size_t target) override {
        this->back().cx(control, target);
        return *this;
    }

private:
    std::vector<SubTableau> _subtableaux;
    std::string _filename;
    std::vector<std::string> _procedures;
};

void adjoint_inplace(SubTableau& subtableau);
[[nodiscard]] SubTableau adjoint(SubTableau const& subtableau);

void adjoint_inplace(Tableau& tableau);
[[nodiscard]] Tableau adjoint(Tableau const& tableau);

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
    auto format(qsyn::experimental::StabilizerTableau const& tableau, FormatContext& ctx) const {
        return presentation == 'c' ? format_to(ctx.out(), "{}", tableau.to_string())
                                   : format_to(ctx.out(), "{}", tableau.to_bit_string());
    }
};

template <>
struct fmt::formatter<qsyn::experimental::SubTableau> {
    char presentation = 'c';
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::SubTableau const& subtableau, FormatContext& ctx) const {
        return presentation == 'c'
                   ? format_to(
                         ctx.out(),
                         "Clifford:\n{:c}\nPauli Rotations:\n{:c}",
                         subtableau.clifford,
                         fmt::join(subtableau.pauli_rotations, "\n"))
                   : format_to(
                         ctx.out(),
                         "Clifford:\n{:b}\nPauli Rotations:\n{:b}",
                         subtableau.clifford,
                         fmt::join(subtableau.pauli_rotations, "\n"));
    }
};

template <>
struct fmt::formatter<qsyn::experimental::Tableau> {
    char presentation = 'c';
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::Tableau const& tableau, FormatContext& ctx) const {
        return presentation == 'c'
                   ? format_to(ctx.out(), "{:c}", fmt::join(tableau, "\n"))
                   : format_to(ctx.out(), "{:b}", fmt::join(tableau, "\n"));
    }
};
