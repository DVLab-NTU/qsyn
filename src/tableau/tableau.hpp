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
#include "util/util.hpp"

namespace qsyn {

namespace experimental {

using SubTableau = std::variant<StabilizerTableau, std::vector<PauliRotation>>;

/**
 * @brief Enumeration for ancilla qubit initial states
 */
enum class AncillaInitialState {
    ZERO = 0,    // |0⟩ state (default, no gate needed)
    ONE = 1,     // |1⟩ state (requires X gate)
    PLUS = 2,    // |+⟩ state (requires H gate)
    MINUS = 3    // |-⟩ state (requires H then X gate)
};

class Tableau : public PauliProductTrait<Tableau> {
public:
    Tableau(size_t n_qubits) : _subtableaux{StabilizerTableau{n_qubits}}, _n_qubits{n_qubits} {}
    Tableau(std::initializer_list<SubTableau> subtableaux)
        : _subtableaux{subtableaux},
          _n_qubits(
              dvlab::match(
                  _subtableaux.front(),
                  [](StabilizerTableau const& st) { return st.n_qubits(); },
                  [](std::vector<PauliRotation> const& pr) { return pr.front().n_qubits(); })) {}

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
                });
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

    // Measurement management
    auto const& get_measurements() const {
        return _measurements;
    }
    
    auto& get_measurements() {
        return _measurements;
    }
    
    void add_measurement(size_t qubit, size_t classical_bit) {
        _measurements.emplace_back(qubit, classical_bit);
    }
    
    void clear_measurements() {
        _measurements.clear();
    }
    
    // If-else operation management
    auto const& get_if_else_operations() const {
        return _if_else_operations;
    }
    
    auto& get_if_else_operations() {
        return _if_else_operations;
    }
    
    void add_if_else_operation(size_t classical_bit, size_t value, std::string const& operation, std::vector<size_t> const& qubits) {
        _if_else_operations.emplace_back(IfElseOperation{classical_bit, value, operation, qubits});
    }
    
    void clear_if_else_operations() {
        _if_else_operations.clear();
    }

    /**
     * @brief Add an ancilla qubit to the tableau
     * 
     * Adds a new qubit that is initialized to |0⟩ state to all sub-tableaux
     * For StabilizerTableau: adds Z stabilizer and X destabilizer for the ancilla
     * For PauliRotation vectors: extends all rotations to include the new qubit as I
     * 
     * @param initial_state the initial state for the ancilla qubit (defaults to ZERO)
     * @return the index of the newly added ancilla qubit
     */
    size_t add_ancilla_qubit(AncillaInitialState initial_state = AncillaInitialState::ZERO) {
        size_t new_qubit = _n_qubits;
        
        // Add ancilla qubit to all sub-tableaux
        for (auto& subtableau : _subtableaux) {
            std::visit(
                dvlab::overloaded{
                    [](StabilizerTableau& st) {
                        st.add_ancilla_qubit();
                    },
                    [](std::vector<PauliRotation>& rotations) {
                        for (auto& rotation : rotations) {
                            rotation.add_ancilla_qubit();
                        }
                    }},
                subtableau);
        }
        
        // Update the total number of qubits
        _n_qubits++;
        
        // Track the ancilla qubit
        _ancilla_qubits.insert(new_qubit);
        _ancilla_dirty_state[new_qubit] = false;  // Initially clean
        _ancilla_initial_states[new_qubit] = initial_state;  // Store initial state
        
        return new_qubit;
    }

    /**
     * @brief Check if a qubit is an ancilla qubit
     * 
     * @param qubit the qubit index to check
     * @return true if the qubit is an ancilla, false otherwise
     */
    bool is_ancilla_qubit(size_t qubit) const {
        return _ancilla_qubits.find(qubit) != _ancilla_qubits.end();
    }

    /**
     * @brief Check if an ancilla qubit is dirty (has been used)
     * 
     * @param ancilla_qubit the ancilla qubit index
     * @return true if the ancilla is dirty, false if clean
     * @throws std::invalid_argument if the qubit is not an ancilla
     */
    bool is_ancilla_dirty(size_t ancilla_qubit) const {
        if (!is_ancilla_qubit(ancilla_qubit)) {
            throw std::invalid_argument("Qubit " + std::to_string(ancilla_qubit) + " is not an ancilla qubit");
        }
        return _ancilla_dirty_state.at(ancilla_qubit);
    }

    /**
     * @brief Mark an ancilla qubit as dirty (used)
     * 
     * @param ancilla_qubit the ancilla qubit index
     * @throws std::invalid_argument if the qubit is not an ancilla
     */
    void mark_ancilla_dirty(size_t ancilla_qubit) {
        if (!is_ancilla_qubit(ancilla_qubit)) {
            throw std::invalid_argument("Qubit " + std::to_string(ancilla_qubit) + " is not an ancilla qubit");
        }
        _ancilla_dirty_state[ancilla_qubit] = true;
    }

    /**
     * @brief Mark an ancilla qubit as clean (unused)
     * 
     * @param ancilla_qubit the ancilla qubit index
     * @throws std::invalid_argument if the qubit is not an ancilla
     */
    void mark_ancilla_clean(size_t ancilla_qubit) {
        if (!is_ancilla_qubit(ancilla_qubit)) {
            throw std::invalid_argument("Qubit " + std::to_string(ancilla_qubit) + " is not an ancilla qubit");
        }
        _ancilla_dirty_state[ancilla_qubit] = false;
    }


    /**
     * @brief Get all ancilla qubits
     * 
     * @return const reference to the set of ancilla qubit indices
     */
    std::unordered_set<size_t> const& get_ancilla_qubits() const {
        return _ancilla_qubits;
    }

    /**
     * @brief Get all dirty ancilla qubits
     * 
     * @return vector of dirty ancilla qubit indices
     */
    std::vector<size_t> get_dirty_ancilla_qubits() const {
        std::vector<size_t> dirty_ancillas;
        for (auto const& [qubit, is_dirty] : _ancilla_dirty_state) {
            if (is_dirty) {
                dirty_ancillas.push_back(qubit);
            }
        }
        return dirty_ancillas;
    }

    /**
     * @brief Get the initial state of an ancilla qubit
     * 
     * @param ancilla_qubit the ancilla qubit index
     * @return the initial state of the ancilla qubit
     * @throws std::invalid_argument if the qubit is not an ancilla
     */
    AncillaInitialState get_ancilla_initial_state(size_t ancilla_qubit) const {
        if (!is_ancilla_qubit(ancilla_qubit)) {
            throw std::invalid_argument("Qubit " + std::to_string(ancilla_qubit) + " is not an ancilla qubit");
        }
        return _ancilla_initial_states.at(ancilla_qubit);
    }

    /**
     * @brief Set the initial state of an ancilla qubit
     * 
     * @param ancilla_qubit the ancilla qubit index
     * @param initial_state the new initial state
     * @throws std::invalid_argument if the qubit is not an ancilla
     */
    void set_ancilla_initial_state(size_t ancilla_qubit, AncillaInitialState initial_state) {
        if (!is_ancilla_qubit(ancilla_qubit)) {
            throw std::invalid_argument("Qubit " + std::to_string(ancilla_qubit) + " is not an ancilla qubit");
        }
        _ancilla_initial_states[ancilla_qubit] = initial_state;
    }

    /**
     * @brief Get all ancilla qubits with their initial states
     * 
     * @return vector of pairs (qubit_index, initial_state)
     */
    std::vector<std::pair<size_t, AncillaInitialState>> get_ancilla_initial_states() const {
        std::vector<std::pair<size_t, AncillaInitialState>> result;
        for (auto const& [qubit, state] : _ancilla_initial_states) {
            result.emplace_back(qubit, state);
        }
        return result;
    }

    Tableau& h(size_t qubit) noexcept override;
    Tableau& s(size_t qubit) noexcept override;
    Tableau& cx(size_t control, size_t target) noexcept override;

private:
    std::vector<SubTableau> _subtableaux;
    std::size_t _n_qubits;
    std::string _filename;
    std::vector<std::string> _procedures;
    
    // Measurement tracking: stores (qubit, classical_bit) pairs
    std::vector<std::pair<size_t, size_t>> _measurements;
    
    // If-else tracking: stores (classical_bit, value, operation, qubits) for "if(c==value) operation"
    struct IfElseOperation {
        size_t classical_bit;
        size_t value;
        std::string operation;
        std::vector<size_t> qubits;
    };
    std::vector<IfElseOperation> _if_else_operations;
    
    // Ancilla qubit tracking
    std::unordered_set<size_t> _ancilla_qubits;  // Track which qubits are ancilla
    std::unordered_map<size_t, bool> _ancilla_dirty_state;  // Track if ancilla is dirty (true) or clean (false)
    std::unordered_map<size_t, AncillaInitialState> _ancilla_initial_states;  // Track initial state for each ancilla
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
