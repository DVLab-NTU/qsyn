/**
 * @file tableau.cpp
 * @brief define tableau class for quantum circuit representation
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <tl/fold.hpp>
#include <variant>
#include <unordered_set>
#include <unordered_map>
#include <ranges>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <fmt/core.h>
#include <fmt/format.h>

#include "./stabilizer_tableau.hpp"
#include "./classical_tableau.hpp"
#include "util/util.hpp"

namespace qsyn {

namespace experimental {

using SubTableau = std::variant<
    StabilizerTableau,           // Clifford operations (non-conditional)
    std::vector<PauliRotation>,  // Non-Clifford rotations
    ClassicalControlTableau      // Classical control operations
>;

class Tableau : public PauliProductTrait<Tableau> {
public:
    Tableau(size_t n_qubits) : _subtableaux{StabilizerTableau{n_qubits}}, _n_qubits{n_qubits} {}
    Tableau(std::initializer_list<SubTableau> subtableaux)
        : _subtableaux{subtableaux},
          _n_qubits(
              dvlab::match(
                  _subtableaux.front(),
                  [](StabilizerTableau const& st) { return st.n_qubits(); },
                  [](std::vector<PauliRotation> const& pr) { return pr.front().n_qubits(); },
                  [](ClassicalControlTableau const& cct) { return cct.operations().n_qubits(); })) {}

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
        return std::count_if(_subtableaux.begin(), _subtableaux.end(), [](auto const& subtableau) { return std::holds_alternative<StabilizerTableau>(subtableau); });
    }
    auto n_pauli_rotations() const {
        size_t count = 0;
        for (auto const& subtableau : _subtableaux) {
            count += dvlab::match(
                subtableau,
                [](StabilizerTableau const&) { return 0ul; },
                [](std::vector<PauliRotation> const& rotations) {
                    return rotations.size();
                },
                [](ClassicalControlTableau const&) { return 0ul; });
        }
        return count;
    }

    auto is_empty() const {
        return _subtableaux.empty();
    }

    auto insert(std::vector<SubTableau>::iterator pos, std::vector<SubTableau>::iterator first, std::vector<SubTableau>::iterator last) {
        // FIXME - check if the subtableaux have the same number of qubits
        return _subtableaux.insert(pos, first, last);
    }

    auto insert(std::vector<SubTableau>::iterator pos, SubTableau const& subtableau) {
        // FIXME - check if the subtableau has the same number of qubits
        return _subtableaux.insert(pos, subtableau);
    }

    auto erase(std::vector<SubTableau>::iterator first, std::vector<SubTableau>::iterator last) {
        return _subtableaux.erase(first, last);
    }

    template<typename Range>
    auto erase(Range const& range) {
        return _subtableaux.erase(range);
    }

    auto push_back(SubTableau const& subtableau) {
        // FIXME - check if the subtableau has the same number of qubits
        _subtableaux.push_back(subtableau);
    }

    template <typename... Args>
    auto emplace_back(Args&&... args) {
        // FIXME - check if the subtableau has the same number of qubits
        return _subtableaux.emplace_back(std::forward<Args>(args)...);
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

    /**
     * @brief Collect all ancilla initial states from all subtableaux.
     * Iterates through all subtableaux and collects initial states from StabilizerTableau and PauliRotation.
     */
    void commute_classical();
    
    /**
     * @brief Add an ancilla initial state to the tableau
     * 
     * @param ancilla_index The index of the ancilla qubit
     * @param state The initial state of the ancilla qubit
     */
    void add_ancilla_state(size_t ancilla_index, AncillaInitialState state) {
        _ancilla_initial_states.push_back({ancilla_index, state});
    }
    
    /**
     * @brief Get the vector of ancilla initial states (as pairs of <ancilla_index, state>)
     * 
     * @return const reference to the vector of pairs
     */
    std::vector<std::pair<size_t, AncillaInitialState>> const& ancilla_initial_states() const {
        return _ancilla_initial_states;
    }

    Tableau& h(size_t qubit) noexcept override;
    Tableau& s(size_t qubit) noexcept override;
    Tableau& cx(size_t control, size_t target) noexcept override;

private:
    std::vector<SubTableau> _subtableaux;
    std::size_t _n_qubits;
    std::string _filename;
    std::vector<std::string> _procedures;
    std::vector<std::pair<size_t, AncillaInitialState>> _ancilla_initial_states;  // Track initial states for all ancilla qubits as pairs <ancilla_index, state>
};

void adjoint_inplace(SubTableau& subtableau);
[[nodiscard]] SubTableau adjoint(SubTableau const& subtableau);

void adjoint_inplace(Tableau& tableau);
[[nodiscard]] Tableau adjoint(Tableau const& tableau);

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
            dvlab::overloaded{
                [&](qsyn::experimental::StabilizerTableau const& st) -> format_context::iterator {
                    return fmt::format_to(ctx.out(), "Clifford:\n{}\n", presentation == 'c' ? st.to_string() : st.to_bit_string());
                },
                [&](std::vector<qsyn::experimental::PauliRotation> const& pr) -> format_context::iterator {
                    if (presentation == 'c')
                        return fmt::format_to(ctx.out(), "Pauli Rotations:\n{:c}\n", fmt::join(pr, "\n"));
                    else
                        return fmt::format_to(ctx.out(), "Pauli Rotations:\n{:b}\n", fmt::join(pr, "\n"));
                },
                [&](qsyn::experimental::ClassicalControlTableau const& cct) -> format_context::iterator {
                    auto result = fmt::format_to(ctx.out(), "Classical Control (qubit[{}] controls):\n", cct.control_qubit());
                    result = fmt::format_to(result, "  Operations:\n");
                    result = fmt::format_to(result, "  {}\n", 
                        presentation == 'c' ? cct.operations().to_string() : cct.operations().to_bit_string());
                    return result;
                }},
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
        return presentation == 'c'
                   ? fmt::format_to(ctx.out(), "{:c}", fmt::join(tableau, "\n"))
                   : fmt::format_to(ctx.out(), "{:b}", fmt::join(tableau, "\n"));
    }
};
