/**
 * @file
 * @brief all functions for todd algorithms
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#pragma once
#include <unordered_set>

#include "tableau/pauli_rotation.hpp"
#include "util/boolean_matrix.hpp"

namespace qsyn::tableau {
using qsyn::tableau::CliffordOperatorString;
using qsyn::tableau::CliffordOperatorType;
using qsyn::tableau::PauliRotation;
namespace todd {
using Polynomial = std::vector<PauliRotation>;
dvlab::BooleanMatrix get_row_products(dvlab::BooleanMatrix const& matrix);
dvlab::BooleanMatrix get_nullspace_transposed(dvlab::BooleanMatrix const& matrix);
dvlab::BooleanMatrix load_phase_poly_matrix(Polynomial const& polynomial);
std::vector<PauliRotation> from_boolean_matrix(dvlab::BooleanMatrix const& matrix);
}  // namespace todd
namespace signature {
class MultiLinearPolynomial {
public:
    void add_rotation(PauliRotation const& rotation, bool subtract = false) {
        if (!rotation.is_diagonal()) {
            return;
        }
        if (4 % rotation.phase().denominator() != 0) {
            return;
        }

        auto const ones =
            std::views::iota(0ul, rotation.n_qubits()) |
            std::views::filter([&rotation](size_t i) { return rotation.pauli_product().is_z_set(i); }) |
            tl::to<std::vector>();

        if (ones.empty()) {
            // Identity-axis phase rotations are global phase terms.
            // Keep them explicit so optimization does not silently change exact-unitary semantics.
            subtract ? _constant_term += 7 : _constant_term += 1;
            _constant_term %= 8;
            return;
        }

        for (auto const& i : ones) {
            if (!_linear_terms.contains(i)) {
                _linear_terms.emplace(i, 0);
            }
            subtract ? _linear_terms[i] += 7 : _linear_terms[i] += 1;
            _linear_terms[i] %= 8;
        }

        for (auto const& [i, j] : dvlab::combinations<2>(ones)) {
            if (!_quadratic_terms.contains({i, j})) {
                _quadratic_terms.emplace(std::make_pair(i, j), 0);
            }
            subtract ? _quadratic_terms[{i, j}] += 3 : _quadratic_terms[{i, j}] += 1;
            _quadratic_terms[{i, j}] %= 4;
        }

        for (auto const& [i, j, k] : dvlab::combinations<3>(ones)) {
            if (!_cubic_terms.contains({i, j, k})) {
                _cubic_terms.emplace(i, j, k);
            } else {
                _cubic_terms.erase({i, j, k});
            }
        }
    }

    void add_rotations(std::vector<PauliRotation> const& rotations, bool subtract = false) {
        for (auto const& rotation : rotations) {
            add_rotation(rotation, subtract);
        }
    }

    bool has_same_signature_tensor(MultiLinearPolynomial const& other) const {
        return _cubic_terms == other._cubic_terms;
    }

    bool is_clifford() const {
        return _constant_term == 0 &&
               std::ranges::all_of(_linear_terms | std::views::values, [](size_t const& count) { return count % 2 == 0; }) &&
               std::ranges::all_of(_quadratic_terms | std::views::values, [](size_t const& count) { return count % 2 == 0; }) &&
               _cubic_terms.empty();
    }

    std::optional<CliffordOperatorString> extract_clifford_operators() const {
        if (!is_clifford()) {
            return std::nullopt;
        }

        auto clifford = CliffordOperatorString();
        for (auto const& [i, count] : _linear_terms) {
            switch (count) {
                case 0:
                    break;
                case 2:
                    clifford.emplace_back(CliffordOperatorType::s, std::array<size_t, 2>{i, 0});
                    break;
                case 4:
                    clifford.emplace_back(CliffordOperatorType::z, std::array<size_t, 2>{i, 0});
                    break;
                case 6:
                    clifford.emplace_back(CliffordOperatorType::sdg, std::array<size_t, 2>{i, 0});
                    break;
                default:
                    DVLAB_UNREACHABLE("The count should be 0, 2, 4, or 6");
            }
        }

        for (auto const& [pair, count] : _quadratic_terms) {
            auto const& [i, j] = pair;
            switch (count) {
                case 0:
                    break;
                case 2:
                    clifford.emplace_back(CliffordOperatorType::cz, std::array<size_t, 2>{i, j});
                    break;
                default:
                    DVLAB_UNREACHABLE("The count should be 0 or 2");
            }
        }

        return clifford;
    }

private:
    struct PairHash {
        size_t operator()(std::pair<size_t, size_t> const& pair) const {
            return std::hash<size_t>{}(pair.first) ^ std::hash<size_t>{}(pair.second);
        }
    };

    struct TripleHash {
        size_t operator()(std::tuple<size_t, size_t, size_t> const& triple) const {
            return std::hash<size_t>{}(std::get<0>(triple)) ^ std::hash<size_t>{}(std::get<1>(triple)) ^ std::hash<size_t>{}(std::get<2>(triple));
        }
    };

    size_t _constant_term = 0;
    std::unordered_map<size_t, size_t> _linear_terms;
    std::unordered_map<std::pair<size_t, size_t>, size_t, PairHash> _quadratic_terms;
    std::unordered_set<std::tuple<size_t, size_t, size_t>, TripleHash> _cubic_terms;
};
}  // namespace signature
}  // namespace qsyn::tableau
