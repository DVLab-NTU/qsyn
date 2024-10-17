#include <cstddef>

#include "./simplify.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

namespace qsyn::zx::simplify {

namespace {
// inplace version of std::ranges::set_difference
// assumes that both vectors are sorted
std::vector<size_t> get_neighbor_ids(ZXGraph const& g, ZXVertex* v) {
    return g.get_neighbors(v) |
           std::views::keys |
           std::views::transform(&ZXVertex::get_id) |
           tl::to<std::vector>();
}

std::vector<size_t> sort(std::vector<size_t> vec) {
    std::ranges::sort(vec);
    return vec;
}

void vec_difference_inplace(
    std::vector<size_t>& vec1,
    std::vector<size_t> const& vec2) {
    auto i = 0ul, j = 0ul, k = 0ul;

    while (i < vec1.size() && j < vec2.size()) {
        if (vec1[i] < vec2[j]) {
            vec1[k++] = vec1[i++];
        } else if (vec1[i] > vec2[j]) {
            ++j;
        } else {
            ++i;
            ++j;
        }
    }

    while (i < vec1.size()) {
        vec1[k++] = vec1[i++];
    }

    vec1.resize(k);
}

// inplace version of std::ranges::set_intersection
// assumes that both vectors are sorted
void vec_intersection_inplace(
    std::vector<size_t>& vec1,
    std::vector<size_t> const& vec2) {
    auto i = 0ul, j = 0ul, k = 0ul;

    while (i < vec1.size() && j < vec2.size()) {
        if (vec1[i] < vec2[j]) {
            ++i;
        } else if (vec1[i] > vec2[j]) {
            ++j;
        } else {
            vec1[k++] = vec1[i++];
            ++i;
            ++j;
        }
    }

    vec1.resize(k);
}

std::vector<size_t> vec_difference(
    std::vector<size_t> const& vec1,
    std::vector<size_t> const& vec2) {
    auto result = std::vector<size_t>{};
    std::ranges::set_difference(vec1, vec2, std::back_inserter(result));
    return result;
}

std::vector<size_t> vec_intersection(
    std::vector<size_t> const& vec1,
    std::vector<size_t> const& vec2) {
    auto result = std::vector<size_t>{};
    std::ranges::set_intersection(vec1, vec2, std::back_inserter(result));
    return result;
}

}  // namespace

template <typename Rule>
requires std::is_base_of_v<ZXRule, Rule>
long calculate_2q_decrease(Rule const& rule, ZXGraph const& g);

template <>
long calculate_2q_decrease(IdentityFusion const& rule, ZXGraph const& g) {
    auto const v_id = rule.get_v_id();

    assert(rule.is_applicable(g));

    auto const left  = g.get_first_neighbor(g[v_id]).first;
    auto const right = g.get_second_neighbor(g[v_id]).first;

    auto const v1_neighbors = sort(get_neighbor_ids(g, left));
    auto const v2_neighbors = sort(get_neighbor_ids(g, right));

    auto const intersection = vec_intersection(v1_neighbors, v2_neighbors);

    auto const e_decrease = 2 * gsl::narrow<long>(intersection.size()) +
                            (g.is_neighbor(left, right) ? 1 : 0);
    auto const v_decrease =
        gsl::narrow<long>(std::ranges::count_if(
            intersection,
            [&](size_t id) { return g.num_neighbors(g[id]) == 2; })) +
        1;

    return e_decrease - v_decrease;
}

template <>
long calculate_2q_decrease(LCompUnfusion const& rule, ZXGraph const& g) {
    auto const v_id = rule.get_v_id();

    assert(rule.is_applicable(g));

    auto const neighbors_to_unfuse = sort(rule.get_neighbors_to_unfuse());
    auto const neighbors           = sort(get_neighbor_ids(g, g[v_id]));

    auto const difference = vec_difference(neighbors, neighbors_to_unfuse);

    // the number of unfusions performed. For lcomp, this is 0 or 1.
    auto const num_unfusions =
        (neighbors_to_unfuse.size() > 0 ||
         g[v_id]->phase().denominator() != 2)
            ? 1
            : 0;
    auto const clique_size   = difference.size() + num_unfusions;
    auto const max_new_edges = clique_size * (clique_size - 1) / 2;

    auto const num_edges = std::ranges::count_if(
        dvlab::combinations<2>(difference),
        [&g](auto const& pair) {
            auto const& [v1, v2] = pair;
            return g.is_neighbor(v1, v2, EdgeType::hadamard);
        });

    auto const e_decrease =
        // complementation in lcomp
        2 * gsl::narrow<long>(num_edges) - gsl::narrow<long>(max_new_edges)
        // removing the vertex in lcomp
        + gsl::narrow<long>(clique_size)
        // unfusion
        - 2 * gsl::narrow<long>(num_unfusions);

    auto const v_decrease = 1l - 2 * num_unfusions;

    return e_decrease - v_decrease;
}

template <>
long calculate_2q_decrease(PivotUnfusion const& rule, ZXGraph const& g) {
    auto const v1_id = rule.get_v1_id();
    auto const v2_id = rule.get_v2_id();

    assert(rule.is_applicable(g));

    auto const neighbors_to_unfuse_v1 = sort(rule.get_neighbors_to_unfuse_v1());
    auto const neighbors_to_unfuse_v2 = sort(rule.get_neighbors_to_unfuse_v2());

    auto neighbors_v1 = sort(get_neighbor_ids(g, g[v1_id]));
    auto neighbors_v2 = sort(get_neighbor_ids(g, g[v2_id]));

    // exclude v1, v2 from each other's neighbors
    std::erase(neighbors_v1, v2_id);
    std::erase(neighbors_v2, v1_id);

    vec_difference_inplace(neighbors_v1, neighbors_to_unfuse_v1);
    vec_difference_inplace(neighbors_v2, neighbors_to_unfuse_v2);

    auto const common_neighbors = vec_intersection(neighbors_v1, neighbors_v2);

    vec_difference_inplace(neighbors_v1, common_neighbors);
    vec_difference_inplace(neighbors_v2, common_neighbors);

    auto const count_hadamard_edges =
        [&g](auto const& vec1, auto const& vec2) {
            size_t count = 0;
            for (auto const& v1 : vec1) {
                for (auto const& v2 : vec2) {
                    if (g.is_neighbor(v1, v2, EdgeType::hadamard)) {
                        ++count;
                    }
                }
            }
            return count;
        };

    auto const num_edges =
        count_hadamard_edges(neighbors_v1, neighbors_v2) +
        count_hadamard_edges(neighbors_v1, common_neighbors) +
        count_hadamard_edges(neighbors_v2, common_neighbors);

    auto const do_unfusion_v1 = neighbors_to_unfuse_v1.size() > 0 ||
                                g[v1_id]->phase().denominator() != 1;
    auto const do_unfusion_v2 = neighbors_to_unfuse_v2.size() > 0 ||
                                g[v2_id]->phase().denominator() != 1;

    // clang-format off
    auto const num_v1_neighbors = neighbors_v1.size() +
        (do_unfusion_v1 ? 1 : 0);
    auto const num_v2_neighbors = neighbors_v2.size() +
        (do_unfusion_v2 ? 1 : 0);
    auto const num_unfusions =
        (do_unfusion_v1 ? 1 : 0) + (do_unfusion_v2 ? 1 : 0);
    // clang-format on
    auto const num_common_neighbors = common_neighbors.size();

    auto const max_new_edges = num_v1_neighbors * num_v2_neighbors +
                               num_v1_neighbors * num_common_neighbors +
                               num_v2_neighbors * num_common_neighbors;

    auto const e_decrease =
        // complementation of pivot
        2 * gsl::narrow<long>(num_edges) - gsl::narrow<long>(max_new_edges)
        // vertex removal of pivot
        + gsl::narrow<long>(num_v1_neighbors) +
        gsl::narrow<long>(num_v2_neighbors) +
        2 * gsl::narrow<long>(num_common_neighbors) + 1
        // unfusion of pivot
        - 2 * gsl::narrow<long>(num_unfusions);
    auto const v_decrease = 2 - 2 * gsl::narrow<long>(num_unfusions);

    return e_decrease - v_decrease;
}

}  // namespace qsyn::zx::simplify
