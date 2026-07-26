/**
 * @file tableau.cpp
 * @brief define tableau class for quantum circuit representation
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <tl/fold.hpp>
#include <optional>
#include <variant>
#include <vector>
#include <string>

#include "./stabilizer_tableau.hpp"

namespace qsyn {

namespace experimental {

using SubTableau = std::variant<StabilizerTableau, std::vector<PauliRotation>>;

class Tableau : public PauliProductTrait<Tableau> {
public:
    Tableau(size_t n_qubits)
        : _subtableaux{StabilizerTableau{n_qubits}},
          _n_qubits{n_qubits},
          _subtableau_labels(1, ""),
          _subtableau_ops(1, std::nullopt) {}
    Tableau(std::initializer_list<SubTableau> subtableaux)
        : _subtableaux{subtableaux},
          _n_qubits(
              dvlab::match(
                  _subtableaux.front(),
                  [](StabilizerTableau const& st) { return st.n_qubits(); },
                  [](std::vector<PauliRotation> const& pr) { return pr.front().n_qubits(); })),
          _subtableau_labels(subtableaux.size(), ""),
          _subtableau_ops(subtableaux.size(), std::nullopt) {}

    auto begin() const {
        return _subtableaux.begin();
    }
    auto end() const {
        return _subtableaux.end();
    }
    auto begin() {
        return _subtableaux.begin();
    }
    auto end() {
        return _subtableaux.end();
    }

    auto size() const {
        return _subtableaux.size();
    }

    auto const& front() const {
        return _subtableaux.front();
    }
    auto const& back() const {
        return _subtableaux.back();
    }
    auto& front() {
        return _subtableaux.front();
    }
    auto& back() {
        return _subtableaux.back();
    }

    auto n_qubits() const {
        return _n_qubits;
    }
    auto n_cliffords() const {
        return std::ranges::count_if(_subtableaux, [](auto const& subtableau) { return std::holds_alternative<StabilizerTableau>(subtableau); });
    }
    auto n_pauli_rotations() const {
        return tl::fold_left(_subtableaux, size_t{0}, [](size_t acc, auto const& subtableau) {
            return acc + dvlab::match(
                             subtableau,
                             [](StabilizerTableau const&) { return 0ul; },
                             [](std::vector<PauliRotation> const& rotations) {
                                 return rotations.size();
                             });
        });
    }

    auto is_empty() const {
        return _subtableaux.empty();
    }

    auto insert(std::vector<SubTableau>::iterator pos, std::vector<SubTableau>::iterator first, std::vector<SubTableau>::iterator last) {
        // FIXME - check if the subtableaux have the same number of qubits
        auto const idx = static_cast<size_t>(std::distance(_subtableaux.begin(), pos));
        auto const n   = static_cast<size_t>(std::distance(first, last));
        _subtableau_labels.insert(_subtableau_labels.begin() + idx, n, "");
        _subtableau_ops.insert(_subtableau_ops.begin() + idx, n, std::nullopt);
        return _subtableaux.insert(pos, first, last);
    }

    auto insert(std::vector<SubTableau>::iterator pos, SubTableau const& subtableau) {
        // FIXME - check if the subtableau has the same number of qubits
        auto const idx = static_cast<size_t>(std::distance(_subtableaux.begin(), pos));
        _subtableau_labels.insert(_subtableau_labels.begin() + idx, 1, "");
        _subtableau_ops.insert(_subtableau_ops.begin() + idx, 1, std::nullopt);
        return _subtableaux.insert(pos, subtableau);
    }

    auto erase(std::vector<SubTableau>::iterator first, std::vector<SubTableau>::iterator last) {
        auto const idx_first = static_cast<size_t>(std::distance(_subtableaux.begin(), first));
        auto const idx_last  = static_cast<size_t>(std::distance(_subtableaux.begin(), last));
        auto const ret       = _subtableaux.erase(first, last);
        _subtableau_labels.erase(_subtableau_labels.begin() + idx_first, _subtableau_labels.begin() + idx_last);
        _subtableau_ops.erase(_subtableau_ops.begin() + idx_first, _subtableau_ops.begin() + idx_last);
        return ret;
    }

    auto erase(std::ranges::range auto const& range) {
        return erase(std::ranges::begin(range), std::ranges::end(range));
    }

    auto push_back(SubTableau const& subtableau) {
        // FIXME - check if the subtableau has the same number of qubits
        _subtableau_labels.push_back("");
        _subtableau_ops.push_back(std::nullopt);
        _subtableaux.push_back(subtableau);
    }

    template <typename... Args>
    auto emplace_back(Args&&... args) {
        // FIXME - check if the subtableau has the same number of qubits
        _subtableau_labels.push_back("");
        _subtableau_ops.push_back(std::nullopt);
        return _subtableaux.emplace_back(std::forward<Args>(args)...);
    }

    std::string const& get_block_label(size_t idx) const {
        return _subtableau_labels[idx];
    }
    void set_block_label(size_t idx, std::string label) {
        _subtableau_labels[idx] = std::move(label);
    }

    // Optional per-block explicit Clifford operator string (used by NCF direct ops emission).
    std::optional<CliffordOperatorString> const& get_block_ops(size_t idx) const {
        return _subtableau_ops[idx];
    }
    void set_block_ops(size_t idx, CliffordOperatorString ops) {
        _subtableau_ops[idx] = std::move(ops);
    }
    void clear_block_ops(size_t idx) {
        _subtableau_ops[idx].reset();
    }

    auto& operator[](size_t idx) {
        return _subtableaux[idx];
    }
    auto const& operator[](size_t idx) const {
        return _subtableaux[idx];
    }

    auto get_filename() const {
        return _filename;
    }
    auto set_filename(std::string const& filename) {
        _filename = filename;
    }

    auto get_procedures() const {
        return _procedures;
    }
    auto add_procedure(std::string const& procedure) {
        _procedures.push_back(procedure);
    }
    auto add_procedures(std::vector<std::string> const& procedures) {
        _procedures.insert(_procedures.end(), procedures.begin(), procedures.end());
    }

    Tableau& h(size_t qubit) noexcept override;
    Tableau& s(size_t qubit) noexcept override;
    Tableau& cx(size_t control, size_t target) noexcept override;

private:
    std::vector<SubTableau> _subtableaux;
    std::size_t _n_qubits;
    std::string _filename;
    std::vector<std::string> _procedures;
    std::vector<std::string> _subtableau_labels;  // optional label per block (e.g. NCF "original #0, #2")
    std::vector<std::optional<CliffordOperatorString>> _subtableau_ops;  // optional explicit Clifford ops per block

    friend void adjoint_inplace(Tableau& tableau);
};

/**
 * @brief Construct a Tableau consisting of an identity Clifford and a list of Pauli rotations
 *        defined by Pauli strings with a common phase.
 *
 * This is useful for initializing a tableau representation directly from a set of Pauli
 * operators, e.g., for Hamiltonian simulation or optimization routines that operate on
 * PauliRotation lists.
 *
 * @param pauli_strings A list of Pauli strings such as "ZIII", "XXIZ", ...
 * @param phase         The rotation phase for each Pauli string.
 * @return Tableau      A tableau whose first sub-tableau is an identity StabilizerTableau
 *                      and second sub-tableau is a std::vector<PauliRotation>.
 */
inline Tableau make_tableau_from_pauli_strings(std::vector<std::string> const& pauli_strings, dvlab::Phase const& phase) {
    if (pauli_strings.empty()) {
        return Tableau{StabilizerTableau{0}, std::vector<PauliRotation>{}};
    }

    auto const n_qubits = pauli_strings.front().size();
    std::vector<PauliRotation> rotations;
    rotations.reserve(pauli_strings.size());

    for (auto const& pstr : pauli_strings) {
        DVLAB_ASSERT(pstr.size() == n_qubits, "All Pauli strings must have the same length.");
        rotations.emplace_back(std::string_view{pstr}, phase);
    }

    return Tableau{StabilizerTableau{n_qubits}, std::move(rotations)};
}

/**
 * @brief Construct a Tableau from Pauli strings with individual phases.
 *
 * @param terms A list of (Pauli string, phase) pairs.
 * @return Tableau A tableau with identity Clifford and corresponding PauliRotation list.
 */
inline Tableau make_tableau_from_pauli_terms(std::vector<std::pair<std::string, dvlab::Phase>> const& terms) {
    if (terms.empty()) {
        return Tableau{StabilizerTableau{0}, std::vector<PauliRotation>{}};
    }

    auto const n_qubits = terms.front().first.size();
    std::vector<PauliRotation> rotations;
    rotations.reserve(terms.size());

    for (auto const& [pstr, phase] : terms) {
        DVLAB_ASSERT(pstr.size() == n_qubits, "All Pauli strings must have the same length.");
        rotations.emplace_back(std::string_view{pstr}, phase);
    }

    return Tableau{StabilizerTableau{n_qubits}, std::move(rotations)};
}

void adjoint_inplace(SubTableau& subtableau);
[[nodiscard]] SubTableau adjoint(SubTableau const& subtableau);

void adjoint_inplace(Tableau& tableau);
[[nodiscard]] Tableau adjoint(Tableau const& tableau);

/** @brief True if tableau is [Stab][C†][R][C]… (post-NCF shape). */
[[nodiscard]] bool has_ncf_canonical_shape(Tableau const& tableau);

}  // namespace experimental

}  // namespace qsyn
template <>
struct fmt::formatter<qsyn::experimental::SubTableau> {
    char presentation = 'c';
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::SubTableau const& subtableau, FormatContext& ctx) const -> format_context::iterator {
        // NOTE - cannot use run-time formatting to choose between 'c' and 'b'
        //        because the format function may be called in compile-time
        return std::visit(
            dvlab::overloaded(
                [&](qsyn::experimental::StabilizerTableau const& st) -> format_context::iterator {
                    return fmt::format_to(ctx.out(), "Clifford:\n{}\n", presentation == 'c' ? st.to_string() : st.to_bit_string());
                },
                [&](std::vector<qsyn::experimental::PauliRotation> const& pr) -> format_context::iterator {
                    if (presentation == 'c')
                        return fmt::format_to(ctx.out(), "Pauli Rotations:\n{:c}\n", fmt::join(pr, "\n"));
                    else
                        return fmt::format_to(ctx.out(), "Pauli Rotations:\n{:b}\n", fmt::join(pr, "\n"));
                }),
            subtableau);
    }
};

template <>
struct fmt::formatter<qsyn::experimental::Tableau> {
    char presentation = 'c';
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::Tableau const& tableau, FormatContext& ctx) const {
        auto out = ctx.out();
        for (size_t i = 0; i < tableau.size(); ++i) {
            if (i > 0) out = fmt::format_to(out, "\n\n");
            auto const& label = tableau.get_block_label(i);
            if (!label.empty()) out = fmt::format_to(out, "[{}]\n", label);
            out = presentation == 'c' ? fmt::format_to(out, "{:c}", tableau[i]) : fmt::format_to(out, "{:b}", tableau[i]);
        }
        return out;
    }
};
