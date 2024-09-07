/**
 * @file
 * @brief implementation of the todd phase polynomial optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include <limits>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include "../tableau_optimization.hpp"
#include "./todd.cpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "util/boolean_matrix.hpp"
#include "util/ordered_hashmap.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::experimental {

namespace auxiliary_func {

dvlab::BooleanMatrix get_l_matrix(dvlab::BooleanMatrix const& phase_poly_matrix, dvlab::BooleanMatrix const& row_products) {
    auto l_matrix       = phase_poly_matrix;
    auto const num_rows = static_cast<size_t>(std::ceil(std::sqrt(row_products.num_rows() * 2)));

    auto const get_row_product_idx = [&num_rows](size_t a, size_t b) {
        if (a > b) {
            std::swap(a, b);
        }
        return a * num_rows - (a * (a + 1) / 2) + b - a - 1;
    };

    auto const id_vec = std::views::iota(0ul, num_rows) | tl::to<std::vector>();

    for (auto const& [a, b] : dvlab::combinations<2>(id_vec)) {
        auto const new_row = row_products[get_row_product_idx(a, b)];
        if (new_row.is_zeros()) {
            continue;
        }
        l_matrix.push_row(new_row);
    }

    return l_matrix;
}

dvlab::BooleanMatrix get_z_matrix(dvlab::BooleanMatrix const& phase_poly_matrix) {
    auto z_matrix_transposed = dvlab::transpose(phase_poly_matrix);
    auto const num_cols      = static_cast<size_t>(phase_poly_matrix.num_cols() * (phase_poly_matrix.num_cols() - 1) / 2);
    auto const n_qubits      = phase_poly_matrix.num_rows();

    auto const id_vec = std::views::iota(0ul, num_cols) | tl::to<std::vector>();
    auto seen_z       = std::unordered_set<dvlab::BooleanMatrix::Row, dvlab::BooleanMatrixRowHash>();

    if (phase_poly_matrix.num_cols() < 2) {
        return z_matrix_transposed;
    }

    for (auto const& [a, b] : dvlab::combinations<2>(id_vec)) {
        if (stop_requested()) {
            return phase_poly_matrix;
        }
        dvlab::BooleanMatrix::Row z(n_qubits);
        for (size_t k = 0; k < n_qubits; ++k) {
            z[k] = phase_poly_matrix[k][a] ^ phase_poly_matrix[k][b];
        }

        if (seen_z.contains(z)) {
            continue;
        }
        seen_z.insert(z);

        z_matrix_transposed.push_row(z);
    }
    return z_matrix_transposed;
}

int calculate_score(dvlab::BooleanMatrix::Row const& y, std::vector<std::pair<int, int>> const& s_matrix) {
    const int abs_y = static_cast<int>(y.sum() % 2);
    int ret         = -1 * abs_y;

    for (auto& indexes : s_matrix) {
        if (indexes.first == indexes.second) {
            ret += y[indexes.first] + 2 * (y[indexes.first] == 0) * (abs_y);
        } else {
            ret += 2 * (y[indexes.first] ^ y[indexes.second]);
        }
    }
    return ret;
}

std::vector<std::vector<std::pair<int, int>>> get_s_matrices(dvlab::BooleanMatrix const& phase_poly_matrix, dvlab::BooleanMatrix const& z_matrix) {
    std::vector<std::vector<std::pair<int, int>>> s;
    auto phase_poly_matrix_transposed = dvlab::transpose(phase_poly_matrix);
    auto const n_qubits               = phase_poly_matrix.num_cols();
    auto const id_vec                 = std::views::iota(0ul, n_qubits) | tl::to<std::vector>();

    for (auto& z : z_matrix.get_matrix()) {
        std::vector<std::pair<int, int>> s_z;
        // a != b
        for (auto const& [a, b] : dvlab::combinations<2>(id_vec)) {
            dvlab::BooleanMatrix::Row tmp(n_qubits);
            for (size_t k = 0; k < n_qubits; ++k) {
                tmp[k] = phase_poly_matrix[k][a] ^ phase_poly_matrix[k][b];
            }
            if (tmp == z) {
                s_z.push_back(std::make_pair(a, b));
            }
        }
        // a == b
        for (auto a : std::views::iota(0ul, n_qubits)) {
            if (phase_poly_matrix_transposed.get_row(a) == z) {
                s_z.push_back(std::make_pair(a, a));
            }
        }
        s.push_back(s_z);
    }

    return s;
}

Polynomial tohpe_once(Polynomial const& polynomial) {
    if (polynomial.empty()) {
        return polynomial;
    }

    // auto const n_qubits = polynomial.front().n_qubits();

    // Each column represents a term in the phase polynomial, and each row represents a qubit.
    auto const phase_poly_matrix = load_phase_poly_matrix(polynomial);

    auto const idx_vec = std::views::iota(0ul, polynomial.size()) | tl::to<std::vector>();

    auto const row_products = get_row_products(phase_poly_matrix);

    auto const l_matrix             = get_l_matrix(phase_poly_matrix, row_products);
    auto const z_matrix             = get_z_matrix(phase_poly_matrix);
    auto const s_matrices           = get_s_matrices(phase_poly_matrix, z_matrix);
    auto const nullspace_transposed = get_nullspace_transposed(l_matrix);

    // check if y satisfies tohpe condition
    if (nullspace_transposed.is_empty()) {
        return polynomial;
    }

    auto phase_poly_matrix_copy = phase_poly_matrix;
    for (auto const& y : nullspace_transposed) {
        if (y.is_zeros()) {
            continue;
        } else if (y.sum() != y.size() && y.sum() % 2 == 0) {
            // y is candidate

            int max_score    = std::numeric_limits<int>::min();
            size_t max_index = 0;
            for (auto a : std::views::iota(0ul, s_matrices.size())) {
                auto const& s_matrix = s_matrices[a];
                auto score           = calculate_score(y, s_matrix);
                if (score > max_score) {
                    max_score = score;
                    max_index = a;
                }
            }

            auto& chosen_z = z_matrix[max_index];

            spdlog::debug("Found a TOHPE move");
            spdlog::debug("- z: {}", fmt::join(chosen_z, ""));
            spdlog::debug("- y: {}", fmt::join(y, ""));

            for (auto const i : std::views::iota(0ul, phase_poly_matrix_copy.num_rows())) {
                if (chosen_z[i] == 1) {
                    phase_poly_matrix_copy[i] += y;
                }
            }

            phase_poly_matrix_copy = dvlab::transpose(phase_poly_matrix_copy);
            if (y.sum() % 2 == 1) {
                phase_poly_matrix_copy.push_row(chosen_z);
            }

            return from_boolean_matrix(phase_poly_matrix_copy);
        }
    }
    // no candidate, return same matrix
    return from_boolean_matrix(dvlab::transpose(phase_poly_matrix_copy));
}

}  // namespace auxiliary_func

using namespace auxiliary_func;

std::pair<StabilizerTableau, Polynomial> TohpePhasePolynomialOptimizationStrategy::optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const {
    if (polynomial.empty()) {
        fmt::println("Polynomial is empty, returning the input Clifford and polynomial");
        return {clifford, polynomial};
    }

    if (std::ranges::any_of(polynomial, [](PauliRotation const& rotation) { return 4 % rotation.phase().denominator() != 0; })) {
        spdlog::error("Failed to perform TODD optimization: the polynomial contains a non-4th-root-of-unity phase!!");
        return {clifford, polynomial};
    }

    auto ret_clifford   = clifford;
    auto ret_polynomial = polynomial;

    properize(ret_clifford, ret_polynomial);

    auto multi_linear_polynomial = complex_polynomial::MultiLinearPolynomial();
    multi_linear_polynomial.add_rotations(ret_polynomial, false);

    spdlog::trace("Polynomial before TODD:\n{}", fmt::join(ret_polynomial, "\n"));
    spdlog::debug("num_terms before TODD: {}", ret_polynomial.size());

    while (true) {
        auto const num_terms = ret_polynomial.size();
        ret_polynomial       = tohpe_once(ret_polynomial);
        if (ret_polynomial.empty() || ret_polynomial.size() == num_terms) {
            break;
        }
        spdlog::trace("Polynomial after TOHPE:\n{}", fmt::join(ret_polynomial, "\n"));
        spdlog::debug("num_terms after TOHPE: {}", ret_polynomial.size());
    }

    multi_linear_polynomial.add_rotations(ret_polynomial, true);

    if (auto clifford_ops = multi_linear_polynomial.extract_clifford_operators(); clifford_ops.has_value()) {
        ret_clifford.apply(*clifford_ops);
    } else {
        spdlog::error("Failed to perform TOHPE optimization: the post-optimization polynomial does not have the same signature as the pre-optimization polynomial!!");
        return {clifford, polynomial};
    }

    return {ret_clifford, ret_polynomial};
}

}  // namespace qsyn::experimental
