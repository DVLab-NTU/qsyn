/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define the interface of SAT solver ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./sat_solver.hpp"

#include <algorithm>
#include <ranges>
#include <tl/to.hpp>

namespace dvlab::sat {

void SatSolver::assume_all(std::vector<Literal> const& literals) {
    for (auto const& lit : literals) {
        assume(lit);
    }
}

/**
 * @brief Add pseudo-Boolean constraint x1 + x2 + ... + xn >= k
 *        ref: https://people.eng.unimelb.edu.au/pstuckey/mddenc/mddenc.pdf
 *
 * @param literals Literals in the clause
 * @param k
 */
void SatSolver::add_gte_constraint(std::vector<Literal> const& literals, size_t const& k) {
    using std::vector, std::views::iota;

    if (k == 0) {
        return;
    }

    auto bdd        = vector<vector<Literal>>(literals.size(), vector<Literal>(k));
    auto true_node  = Literal(new_var());
    auto false_node = Literal(new_var());

    auto is_in_range = [&literals, &k](int i, int j) {
        return j >= std::max(i + (int)k - int(literals.size()), 0) && j < std::min(i + 1, (int)k);
    };

    for (const int i : iota(0, (int)literals.size())) {
        for (const int j : iota(std::max(i + (int)k - (int)literals.size(), 0), std::min((int)i + 1, (int)k))) {
            bdd[i][j] = Literal(new_var());
        }
    }

    for (const int i : iota(0, (int)literals.size())) {
        const Literal x = literals[i];
        for (const int j : iota(std::max(i + (int)k - (int)literals.size(), 0), std::min(i + 1, (int)k))) {
            const Literal t = is_in_range(i + 1, j + 1) ? bdd[i + 1][j + 1] : true_node;
            const Literal f = is_in_range(i + 1, j) ? bdd[i + 1][j] : false_node;
            const Literal v = bdd[i][j];
            add_clause({~t, ~x, v});
            add_clause({t, ~x, ~v});
            add_clause({~f, x, v});
            add_clause({f, x, ~v});
            add_clause({~t, ~f, v});
            add_clause({t, f, ~v});
        }
    }

    add_clause({true_node});
    add_clause({~false_node});
    add_clause({Literal(bdd[0][0])});
}

/**
 * @brief Add pseudo-Boolean constraint x1 + x2 + ... + xn <= k
 *        ref: https://people.eng.unimelb.edu.au/pstuckey/mddenc/mddenc.pdf
 *
 * @param literals Literals in the clause
 * @param k
 */
void SatSolver::add_lte_constraint(std::vector<Literal> const& literals, size_t const& k) {
    // sum_{i=1}^N x_i <= k -> sum_{i=1}^N ~x_i >= N - k
    auto negated_literals = literals | std::views::transform([](auto const& lit) { return ~lit; }) | tl::to<std::vector>();
    add_gte_constraint(negated_literals, literals.size() - k);
}

}  // namespace dvlab::sat
