/**
 * @file
 * @brief implementation of the todd phase polynomial optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include <unordered_set>

#include "../tableau_optimization.hpp"
#include "tableau/pauli_rotation.hpp"
#include "util/boolean_matrix.hpp"
#include "util/phase.hpp"

namespace qsyn::experimental {

namespace {
using Polynomial = std::vector<PauliRotation>;

dvlab::BooleanMatrix get_chi_matrix(dvlab::BooleanMatrix const& a_prime, dvlab::BooleanMatrix::Row const& z) {
    auto chi_matrix = dvlab::BooleanMatrix();

    auto seen_rows = std::unordered_set<dvlab::BooleanMatrix::Row, dvlab::BooleanMatrixRowHash>();

    for (size_t a = 0; a < a_prime.num_cols(); ++a) {
        for (size_t b = a + 1; b < a_prime.num_cols(); ++b) {
            for (size_t c = b + 1; c < a_prime.num_cols(); ++c) {
                auto const& row_a = a_prime[a];
                auto const& row_b = a_prime[b];
                auto const& row_c = a_prime[c];
                auto new_row      = z[a] * row_b * row_c + z[b] * row_a * row_c + z[c] * row_a * row_b;

                if (seen_rows.contains(new_row)) {
                    continue;
                }

                if (new_row.is_zeros()) {
                    continue;
                }
                seen_rows.insert(new_row);

                chi_matrix.push_row(new_row);
            }
        }
    }

    return chi_matrix;
}

/**
 * @brief Get the nullspace of a matrix.
 *
 * @param matrix
 * @return dvlab::BooleanMatrix
 */
dvlab::BooleanMatrix get_nullspace_transposed(dvlab::BooleanMatrix const& matrix) {
    auto augmented = hstack(dvlab::transpose(matrix), dvlab::identity(matrix.num_cols()));

    auto n_rows = matrix.num_cols();
    auto n_cols = matrix.num_rows();

    auto curr_pivot = size_t{0};

    for (auto const col : std::views::iota(0ul, n_cols)) {
        if (augmented[curr_pivot][col] == 0) {
            for (auto const row : std::views::iota(curr_pivot + 1, n_rows)) {
                if (augmented[row][col] != 0) {
                    augmented[curr_pivot] += augmented[row];
                    break;
                }
            }
        }

        if (augmented[curr_pivot][col] == 0) {
            continue;
        }

        for (auto const row : std::views::iota(curr_pivot + 1, n_rows)) {
            if (augmented[row][col] != 0) {
                augmented[row] += augmented[curr_pivot];
            }
        }

        ++curr_pivot;

        if (curr_pivot == n_rows) {
            break;
        }
    }

    auto nullspace = dvlab::BooleanMatrix(augmented.num_rows() - curr_pivot, augmented.num_cols() - n_cols);

    for (auto const row : std::views::iota(curr_pivot, augmented.num_rows())) {
        for (auto const col : std::views::iota(n_cols, augmented.num_cols())) {
            nullspace[row - curr_pivot][col - n_cols] = augmented[row][col];
        }
    }

    return nullspace;
}

dvlab::BooleanMatrix load_phase_poly_matrix(Polynomial const& polynomial) {
    auto const n_qubits = polynomial.front().n_qubits();

    auto phase_poly_matrix = dvlab::BooleanMatrix(n_qubits, polynomial.size());

    for (size_t i = 0; i < n_qubits; ++i) {
        for (size_t j = 0; j < polynomial.size(); ++j) {
            phase_poly_matrix[i][j] = polynomial[j].pauli_product().is_z_set(i);
        }
    }

    return phase_poly_matrix;
}

std::vector<PauliRotation> from_boolean_matrix(dvlab::BooleanMatrix const& matrix) {
    return matrix |
           std::views::transform([](auto const& row) -> PauliRotation {
               auto const pauli_vec = row |
                                      std::views::transform([](auto const x) { return x == 1 ? Pauli::z : Pauli::i; });
               return {pauli_vec, dvlab::Phase(1, 4)};
           }) |
           tl::to<std::vector>();
}

Polynomial todd_once(Polynomial const& polynomial) {
    if (polynomial.empty()) {
        return polynomial;
    }

    auto const n_qubits = polynomial.front().n_qubits();

    // Each column represents a term in the phase polynomial, and each row represents a qubit.
    auto const phase_poly_matrix = load_phase_poly_matrix(polynomial);

    auto seen_z = std::unordered_set<dvlab::BooleanMatrix::Row, dvlab::BooleanMatrixRowHash>();

    for (size_t a = 0; a < polynomial.size(); ++a) {
        for (size_t b = a + 1; b < polynomial.size(); ++b) {
            dvlab::BooleanMatrix::Row z(n_qubits);
            for (size_t k = 0; k < n_qubits; ++k) {
                z[k] = phase_poly_matrix[a][k] ^ phase_poly_matrix[b][k];
            }
            if (seen_z.contains(z)) {
                continue;
            }

            seen_z.insert(z);

            auto const chi_matrix       = get_chi_matrix(phase_poly_matrix, z);
            auto const augmented_matrix = vstack(phase_poly_matrix, chi_matrix);

            auto const nullspace = get_nullspace_transposed(augmented_matrix);

            if (nullspace.num_rows() == 0) {
                continue;
            }

            for (auto y : nullspace) {
                if (y[a] == y[b]) continue;
                auto phase_poly_matrix_prime = phase_poly_matrix;
                if (y.sum() % 2 == 1) {
                    phase_poly_matrix_prime.push_zeros_column();
                    y.emplace_back(0);
                }

                for (auto const i : std::views::iota(0ul, phase_poly_matrix_prime.num_rows())) {
                    if (z[i] == 1) {
                        phase_poly_matrix_prime[i] += y;
                    }
                }

                return from_boolean_matrix(phase_poly_matrix_prime);
            }
        }
    }

    return polynomial;
}

}  // namespace

void ToddPhasePolynomialOptimizationStrategy::optimize(StabilizerTableau& clifford, Polynomial& polynomial) const {
    if (polynomial.empty()) {
        return;
    }

    if (std::ranges::any_of(polynomial, [](PauliRotation const& rotation) { return rotation.phase().denominator() != 4; })) {
        return;
    }

    properize(clifford, polynomial);

    while (true) {
        auto const num_terms = polynomial.size();
        todd_once(polynomial);
        properize(clifford, polynomial);
        if (polynomial.empty() || polynomial.size() == num_terms) {
            return;
        }
    }
}

}  // namespace qsyn::experimental
