#include "zx/simplifier/rules/rule-matchers.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "common/global.hpp"
#include "common/zx.hpp"

using namespace qsyn::zx;
using namespace qsyn::zx::simplify;
using dvlab::Phase;

size_t binomial_coefficient(size_t n, size_t k) {
    if (k > n) return 0;
    if (k == 0 || k == n) return 1;

    size_t res = 1;
    // C(n, k) = C(n, n-k), so use the smaller value to minimize computations
    if (k > n - k) k = n - k;

    for (size_t i = 0; i < k; ++i) {
        res *= (n - i);
        res /= (i + 1);
    }
    return res;
}

TEST_CASE("Matching Identity Fusions (Case 1)", "[zx][rule][matchers]") {
    ZXGraph graph;

    auto const num_vertices = GENERATE(as<size_t>{}, 3, 5, 7, 10);

    for (size_t i = 0; i < num_vertices; ++i) {
        graph.add_vertex(VertexType::z);
    }

    // create a ring
    for (size_t i = 0; i < num_vertices - 1; ++i) {
        graph.add_edge(i, i + 1, EdgeType::hadamard);
    }
    graph.add_edge(num_vertices - 1, 0, EdgeType::hadamard);

    auto const matcher = IdentityFusionMatcher();

    auto const matches = matcher.find_matches(graph);

    REQUIRE(matches.size() == num_vertices);

    for (auto const& match : matches) {
        REQUIRE(match.is_applicable(graph));
    }
}

TEST_CASE("Matching Identity Fusions (Case 2)", "[zx][rule][matchers]") {
    ZXGraph graph;
    auto const num_vertices = GENERATE(as<size_t>{}, 3, 5, 7, 10);

    for (size_t i = 0; i < num_vertices; ++i) {
        graph.add_vertex(VertexType::z);
    }
    auto const i      = graph.add_input(0);
    auto const o      = graph.add_output(0);
    auto const i_axle = graph.add_vertex(VertexType::z);
    auto const o_axle = graph.add_vertex(VertexType::z);

    graph.add_edge(i, i_axle, EdgeType::simple);
    graph.add_edge(o_axle, o, EdgeType::simple);

    for (size_t i = 0; i < num_vertices; ++i) {
        graph.add_edge(i_axle, graph[i], EdgeType::hadamard);
        graph.add_edge(graph[i], o_axle, EdgeType::hadamard);
    }

    auto const matcher = IdentityFusionMatcher();
    auto const matches = matcher.find_matches(graph);

    REQUIRE(matches.size() == num_vertices);

    for (auto const& match : matches) {
        REQUIRE(match.is_applicable(graph));
    }
}

namespace {

auto get_max_unfusions(size_t num_neighbors, size_t max_unfusions) {
    if (num_neighbors <= 2) {
        return 0ul;
    }
    return std::min(num_neighbors - 2, max_unfusions);
}

}  // namespace

TEST_CASE("Matching LComp Unfusions", "[zx][rule][matchers]") {
    // no neighbor is boundary
    auto const num_neighbors = GENERATE(as<size_t>{}, 2, 5, 10);

    auto const phase = GENERATE(Phase(0), Phase(1, 2), get_random_phase());

    auto g = generate_random_palm_graph(num_neighbors, phase, true);

    auto const num_boundaries = std::ranges::count_if(
        g.get_vertices(),
        &ZXVertex::is_boundary);

    auto const matcher_max_unfusions = GENERATE(as<size_t>{}, 0, 1, 2, 4);

    auto const matcher = LCompUnfusionMatcher(matcher_max_unfusions);
    auto const matches = matcher.find_matches(g);

    CAPTURE(num_neighbors, phase, num_boundaries, matcher_max_unfusions);
    CAPTURE(matches.size());

    for (auto const& match : matches) {
        REQUIRE(match.is_applicable(g));
    }

    auto const min_num_unfusions = gsl::narrow<size_t>(num_boundaries);
    auto const max_num_unfusions =
        get_max_unfusions(num_neighbors, matcher_max_unfusions);

    CAPTURE(min_num_unfusions, max_num_unfusions);

    auto count = 0ul;

    if (max_num_unfusions >= min_num_unfusions) {
        for (auto k : std::views::iota(min_num_unfusions,
                                       max_num_unfusions + 1)) {
            count += binomial_coefficient(
                num_neighbors - num_boundaries,
                k - num_boundaries);
        }

        if (phase.denominator() != 2 && num_boundaries == 0) {
            count -= 1;
        }
    }

    REQUIRE(matches.size() == count);
}

TEST_CASE("Matching LComp Unfusions (Edge Case)", "[zx][rule][matchers]") {
    ZXGraph g1;

    auto const phase = GENERATE(Phase(0), Phase(1, 2), get_random_phase());
    g1.add_vertex(VertexType::z, phase);
    g1.add_vertex(VertexType::z);

    g1.add_edge(0, 1, EdgeType::hadamard);

    auto const matcher = LCompUnfusionMatcher(2);
    auto matches       = matcher.find_matches(g1);

    REQUIRE(matches.size() == 0);

    ZXGraph g2;

    g2.add_vertex(VertexType::z, phase);
    g2.add_input(0);

    g2.add_edge(0, 1, EdgeType::hadamard);

    matches = matcher.find_matches(g2);

    REQUIRE(matches.size() == 0);

    ZXGraph g3;

    g3.add_vertex(VertexType::z, phase);

    matches = matcher.find_matches(g3);

    REQUIRE(matches.size() == (phase.denominator() == 2 ? 1 : 0));
}

TEST_CASE("Matching Pivot Unfusions", "[zx][rule][matchers]") {
    auto const num_v1_nbrs     = GENERATE(as<size_t>{}, 3, 5, 7);
    auto const num_v2_nbrs     = GENERATE(as<size_t>{}, 3, 5, 7);
    auto const num_common_nbrs = GENERATE(as<size_t>{}, 0, 2, 4);

    auto const phase1 = GENERATE(Phase(0), Phase(1, 3));
    auto const phase2 = GENERATE(Phase(0), Phase(1, 3));

    auto g = generate_random_pivot_graph(
        num_v1_nbrs, num_v2_nbrs, num_common_nbrs, phase1, phase2, true);

    auto const v1_nbrs_range = std::views::iota(
        2ul, num_v1_nbrs + 2);
    auto const v2_nbrs_range = std::views::iota(
        2ul + num_v1_nbrs, num_v2_nbrs + 2 + num_v1_nbrs);

    auto const num_boundaries_v1 = gsl::narrow<size_t>(std::ranges::count_if(
        v1_nbrs_range,
        [&](auto i) { return g[i]->is_boundary(); }));
    auto const num_boundaries_v2 = gsl::narrow<size_t>(std::ranges::count_if(
        v2_nbrs_range,
        [&](auto i) { return g[i]->is_boundary(); }));

    auto const matcher_max_unfusions = GENERATE(as<size_t>{}, 2, 4);

    // we need to force the candidates to be the first two vertices
    // or it would be very hard to predict the result
    ZXVertexList candidates{g[0], g[1]};

    auto const matcher = PivotUnfusionMatcher(matcher_max_unfusions);
    auto const matches = matcher.find_matches(g, candidates);

    CAPTURE(num_v1_nbrs, num_v2_nbrs, num_common_nbrs,
            phase1, phase2, matcher_max_unfusions,
            num_boundaries_v1, num_boundaries_v2);
    for (auto const& match : matches) {
        REQUIRE(match.is_applicable(g));
    }

    auto const min_num_unfusions_v1 = gsl::narrow<size_t>(num_boundaries_v1);
    auto const min_num_unfusions_v2 = gsl::narrow<size_t>(num_boundaries_v2);

    auto const max_num_unfusions_v1 =
        get_max_unfusions(num_v1_nbrs + num_common_nbrs, matcher_max_unfusions);
    auto const max_num_unfusions_v2 =
        get_max_unfusions(num_v2_nbrs + num_common_nbrs, matcher_max_unfusions);

    CAPTURE(min_num_unfusions_v1, min_num_unfusions_v2, max_num_unfusions_v1,
            max_num_unfusions_v2);

    auto count = 0ul;

    if (max_num_unfusions_v1 >= min_num_unfusions_v1 &&
        max_num_unfusions_v2 >= min_num_unfusions_v2) {
        for (auto k1 : std::views::iota(min_num_unfusions_v1,
                                        max_num_unfusions_v1 + 1)) {
            auto k1_count = binomial_coefficient(
                num_v1_nbrs + num_common_nbrs - num_boundaries_v1,
                k1 - num_boundaries_v1);
            if (phase1.denominator() != 1 && k1 == 0) {
                k1_count -= 1;
            }
            for (auto k2 : std::views::iota(min_num_unfusions_v2,
                                            max_num_unfusions_v2 + 1)) {
                auto k2_count = binomial_coefficient(
                    num_v2_nbrs + num_common_nbrs - num_boundaries_v2,
                    k2 - num_boundaries_v2);
                if (phase2.denominator() != 1 && k2 == 0) {
                    k2_count -= 1;
                }
                count += k1_count * k2_count;
            }
        }
    }

    REQUIRE(matches.size() == count);
}
