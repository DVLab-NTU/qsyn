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

namespace experimental {

class StabilizerTableau : public PauliProductTrait<StabilizerTableau> {
public:
    StabilizerTableau(size_t n_qubits) : _stabilizers(2 * n_qubits, PauliProduct(std::string(n_qubits, 'I'))) {
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
        return std::ranges::all_of(_stabilizers | std::views::take(n_qubits()), [&rhs](PauliProduct const& stabilizer) { return stabilizer.is_commutative(rhs); });
    }

    /**
     * @brief Add an ancilla qubit to the tableau (extends from former circuit)
     * 
     * Adds a new qubit that extends from the former circuit.
     * The ancilla qubit will have all-zero entries in existing stabilizers.
     * 
     * @return the index of the newly added ancilla qubit
     */
    size_t add_ancilla_qubit() {
        size_t new_qubit = n_qubits();
        
        // Resize all existing stabilizers to include the new qubit (initialized to I)
        for (auto& stabilizer : _stabilizers) {
            // Extend the PauliProduct to include one more qubit (I)
            // This is done by creating a new PauliProduct with the extended size
            std::string extended_str = stabilizer.to_string();
            extended_str += 'I';  // Add I for the new ancilla qubit
            stabilizer = PauliProduct(extended_str);
        }
        
        // Insert Z stabilizer for the ancilla qubit at position new_qubit
        // Z stabilizer: all I except Z at the ancilla position
        std::string z_stabilizer_str(n_qubits() + 1, 'I');
        z_stabilizer_str[new_qubit] = 'Z';
        _stabilizers.insert(_stabilizers.begin() + new_qubit, PauliProduct(z_stabilizer_str));
        
        // Insert X destabilizer for the ancilla qubit at position new_qubit + n_qubits()
        // X destabilizer: all I except X at the ancilla position  
        std::string x_destabilizer_str(n_qubits() + 1, 'I');
        x_destabilizer_str[new_qubit] = 'X';
        _stabilizers.emplace_back(PauliProduct(x_destabilizer_str));
        return new_qubit;
    }
    
private:
    std::vector<PauliProduct> _stabilizers;
};

[[nodiscard]] StabilizerTableau adjoint(StabilizerTableau const& tableau);

inline void adjoint_inplace(StabilizerTableau& tableau) { tableau = adjoint(tableau); }

class StabilizerTableauSynthesisStrategy {
public:
    virtual ~StabilizerTableauSynthesisStrategy()                           = default;
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
    CliffordOperatorString synthesize(StabilizerTableau copy) const override;
};

/**
 * @brief An extractor based on the paper
 *        [Optimal Hadamard gate count for Clifford$+T$ synthesis of Pauli rotations sequences](https://arxiv.org/abs/2302.07040)
 *        by Vandaele, Martiel, Perdrix, and Vuillot.
 *        This method is guaranteed to produce the optimal number of Hadamard gates by first diagonalizing the stabilizers with
 *        provably optimal number of Hadamard gates and then applying the Aaronson-Gottesman method to the rest of the tableau.
 *
 */
class HOptSynthesisStrategy : public StabilizerTableauSynthesisStrategy {
public:
    enum class Mode { star,
                      staircase } mode;
    HOptSynthesisStrategy(Mode mode = Mode::star) : mode{mode} {}
    CliffordOperatorString synthesize(StabilizerTableau copy) const override;
};

CliffordOperatorString extract_clifford_operators(
    StabilizerTableau copy,
    StabilizerTableauSynthesisStrategy const& strategy = HOptSynthesisStrategy{});


void print_clifford_operator_string(CliffordOperatorString const& operations);

}  // namespace experimental

}  // namespace qsyn

template <>
struct fmt::formatter<qsyn::experimental::StabilizerTableau> {
    char presentation = 'c';
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::StabilizerTableau const& tableau, FormatContext& ctx) const {
        return presentation == 'c' ? format_to(ctx.out(), "{}", tableau.to_string())
                                   : format_to(ctx.out(), "{}", tableau.to_bit_string());
    }
};
