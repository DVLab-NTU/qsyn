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
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <fmt/core.h>
#include <fmt/format.h>

#include "./stabilizer_tableau.hpp"
#include "./classical_tableau.hpp"
#include "./optimize/reorder_smt.hpp"
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
    Tableau(size_t n_qubits) : _subtableaux{StabilizerTableau{n_qubits}}, _n_qubits{n_qubits}, _n_ancilla{0} {}
    Tableau(std::initializer_list<SubTableau> subtableaux)
        : _subtableaux{subtableaux},
          _n_qubits(
              dvlab::match(
                  _subtableaux.front(),
                  [](StabilizerTableau const& st) { return st.n_qubits(); },
                  [](std::vector<PauliRotation> const& pr) { return pr.front().n_qubits(); },
                  [](ClassicalControlTableau const& cct) { return cct.operations().n_qubits(); })),
          _n_ancilla{0} {}

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
    void set_n_qubits(size_t n_qubits) {
        _n_qubits = n_qubits;
    }
    void set_n_ancilla(size_t n_ancilla) {
        _n_ancilla = n_ancilla;
    }
    
    auto n_ancilla() const {
        return _n_ancilla;
    }

    void set_export_ancilla_count(size_t count) {
        _export_ancilla_count = count;
    }
    size_t export_ancilla_count() const {
        return _export_ancilla_count.value_or(_n_ancilla);
    }

    void set_export_ancilla_depth(size_t depth) {
        _export_ancilla_depth = depth;
    }
    size_t export_ancilla_depth() const {
        return _export_ancilla_depth.value_or(_n_ancilla);
    }

    void set_export_classical_bit_count(size_t count) {
        _export_classical_bit_count = count;
    }
    size_t export_classical_bit_count() const {
        return _export_classical_bit_count.value_or(export_ancilla_count());
    }

    void set_export_reset_placements(std::vector<ResetPlacement> placements) {
        _export_reset_placements = std::move(placements);
    }
    std::vector<ResetPlacement> const& export_reset_placements() const {
        return _export_reset_placements;
    }
    void clear_export_reset_placements() {
        _export_reset_placements.clear();
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

    /** Clear ancilla initial states and measurement types (e.g. before rebuilding after degadgetization). */
    void clear_ancilla_metadata() {
        _ancilla_initial_states.clear();
        _ancilla_measurement_types.clear();
        clear_export_reset_placements();
    }

    // ── Per-ancilla measurement type ──────────────────────────────────────────
    // Tracks which basis each ancilla is measured in (Z, X, or none).
    // Initially none for every ancilla; set explicitly by the gadgetization pass.

    /**
     * @brief Set the measurement type for ancilla qubit `ancilla_index`.
     *
     * @param ancilla_index  index of the ancilla qubit
     * @param mtype          Z, X, or none
     */
    void set_ancilla_measurement_type(size_t ancilla_index, MeasurementType mtype) {
        _ancilla_measurement_types[ancilla_index] = mtype;
    }

    /**
     * @brief Get the measurement type for ancilla qubit `ancilla_index`.
     *        Returns MeasurementType::none if not explicitly set.
     */
    MeasurementType get_ancilla_measurement_type(size_t ancilla_index) const {
        auto it = _ancilla_measurement_types.find(ancilla_index);
        return it != _ancilla_measurement_types.end() ? it->second : MeasurementType::none;
    }

    std::unordered_map<size_t, MeasurementType> const& ancilla_measurement_types() const {
        return _ancilla_measurement_types;
    }

    /**
     * @brief Get the CCT pairing vector (ccc_index, pmc_index pairs)
     * 
     * @return const reference to the pairing vector
     */
    std::vector<std::pair<size_t, size_t>> const& cct_pairing() const {
        return _cct_pairing;
    }
    
    /**
     * @brief Get the CCT pairing vector (ccc_index, pmc_index pairs)
     * 
     * @return reference to the pairing vector
     */
    std::vector<std::pair<size_t, size_t>>& cct_pairing() {
        return _cct_pairing;
    }
    
    /**
     * @brief Set the CCT pairing vector
     * 
     * @param pairing Vector of (ccc_index, pmc_index) pairs
     */
    void set_cct_pairing(std::vector<std::pair<size_t, size_t>> const& pairing) {
        _cct_pairing = pairing;
    }
    
    /**
     * @brief Clear the CCT pairing vector
     */
    void clear_cct_pairing() {
        _cct_pairing.clear();
    }
    
    /**
     * @brief Find the PMC index paired with a given CCC index
     * 
     * @param ccc_index Index of the CCC in the tableau
     * @return Optional PMC index if found, std::nullopt otherwise
     */
    std::optional<size_t> find_pmc_index(size_t ccc_index) const {
        for (auto const& [ccc_idx, pmc_idx] : _cct_pairing) {
            if (ccc_idx == ccc_index) {
                return pmc_idx;
            }
        }
        return std::nullopt;
    }
    
    /**
     * @brief Find the CCC index paired with a given PMC index
     * 
     * @param pmc_index Index of the PMC in the tableau
     * @return Optional CCC index if found, std::nullopt otherwise
     */
    std::optional<size_t> find_ccc_index(size_t pmc_index) const {
        for (auto const& [ccc_idx, pmc_idx] : _cct_pairing) {
            if (pmc_idx == pmc_index) {
                return ccc_idx;
            }
        }
        return std::nullopt;
    }

    Tableau& h(size_t qubit) noexcept override;
    Tableau& s(size_t qubit) noexcept override;
    Tableau& cx(size_t control, size_t target) noexcept override;

private:
    std::vector<SubTableau> _subtableaux;
    std::size_t _n_qubits;
    std::size_t _n_ancilla;  // Number of ancilla qubits (last _n_ancilla qubits are ancillae)
    std::string _filename;
    std::vector<std::string> _procedures;
    std::vector<std::pair<size_t, AncillaInitialState>> _ancilla_initial_states;
    std::unordered_map<size_t, MeasurementType> _ancilla_measurement_types;  // ancilla_index → Z/X/none
    std::vector<std::pair<size_t, size_t>> _cct_pairing;  // CCT pairing structure - stores (ccc_index, pmc_index) pairs
    std::optional<size_t> _export_ancilla_count;
    std::optional<size_t> _export_ancilla_depth;
    std::optional<size_t> _export_classical_bit_count;
    std::vector<ResetPlacement> _export_reset_placements;
};

/** Sub-tableau indices for a matched Hadamard-gadget CCC/PMC pair. */
struct GadgetPairIndices {
    size_t gadget_index;
    size_t pmc_index;
};

/** Find CCC/PMC indices for the gadget on ancilla_qubit (unique per pair). */
[[nodiscard]] std::optional<GadgetPairIndices> find_gadget_pair(
    std::vector<SubTableau> const& subtableaux,
    size_t ancilla_qubit);

[[nodiscard]] std::optional<GadgetPairIndices> find_gadget_pair(
    Tableau const& tableau,
    size_t ancilla_qubit);

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
        if (it != end && (*it == 'c' || *it == 'b' || *it == 'g')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::SubTableau const& subtableau, FormatContext& ctx) const -> format_context::iterator {
        // NOTE - cannot use run-time formatting to choose between 'c', 'b', and 'g'
        //        because the format function may be called in compile-time
        return std::visit(
            dvlab::overloaded{
                [&](qsyn::experimental::StabilizerTableau const& st) -> format_context::iterator {
                    if (presentation == 'g') {
                        auto const ops = qsyn::experimental::extract_clifford_operators(
                            st, qsyn::experimental::HOptSynthesisStrategy{qsyn::experimental::HOptSynthesisStrategy::Mode::staircase});
                        return fmt::format_to(ctx.out(), "Clifford:\n{}", qsyn::experimental::clifford_ops_to_string(ops));
                    }
                    return fmt::format_to(ctx.out(), "Clifford:\n{}\n", presentation == 'c' ? st.to_string() : st.to_bit_string());
                },
                [&](std::vector<qsyn::experimental::PauliRotation> const& pr) -> format_context::iterator {
                    if (presentation == 'c') {
                        return fmt::format_to(ctx.out(), "Pauli Rotations:\n{:c}\n", fmt::join(pr, "\n"));
                    }
                    return fmt::format_to(ctx.out(), "Pauli Rotations:\n{:b}\n", fmt::join(pr, "\n"));
                },
                [&](qsyn::experimental::ClassicalControlTableau const& cct) -> format_context::iterator {
                    if (presentation == 'g') {
                        auto const ops = qsyn::experimental::extract_clifford_operators(cct.operations());
                        if (cct.is_gadget()) {
                            auto result = fmt::format_to(
                                ctx.out(),
                                "Gadget (ancilla qubit[{}], reference qubit[{}]):\n",
                                cct.ancilla_qubit(),
                                cct.reference_qubit());
                            return fmt::format_to(result, "  Operations:\n{}", qsyn::experimental::clifford_ops_to_string(ops));
                        }
                        auto result = fmt::format_to(
                            ctx.out(), "Classical Control (ancilla qubit[{}] controls):\n", cct.ancilla_qubit());
                        return fmt::format_to(result, "  Operations:\n{}", qsyn::experimental::clifford_ops_to_string(ops));
                    }
                    if (cct.is_gadget()) {
                        auto result = fmt::format_to(
                            ctx.out(),
                            "Gadget (ancilla qubit[{}], reference qubit[{}]):\n",
                            cct.ancilla_qubit(),
                            cct.reference_qubit());
                        result = fmt::format_to(result, "  Operations:\n");
                        result = fmt::format_to(result, "  {}\n",
                            presentation == 'c' ? cct.operations().to_string() : cct.operations().to_bit_string());
                        return result;
                    }
                    auto result =
                        fmt::format_to(ctx.out(), "Classical Control (ancilla qubit[{}] controls):\n", cct.ancilla_qubit());
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
        if (it != end && (*it == 'c' || *it == 'b' || *it == 'g')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::Tableau const& tableau, FormatContext& ctx) const {
        auto out = ctx.out();
        bool first = true;
        for (auto const& subtableau : tableau) {
            if (!first) out = fmt::format_to(out, "\n");
            first = false;
            if (presentation == 'g')
                out = fmt::format_to(out, "{:g}", subtableau);
            else if (presentation == 'c')
                out = fmt::format_to(out, "{:c}", subtableau);
            else
                out = fmt::format_to(out, "{:b}", subtableau);
        }
        return out;
    }
};
