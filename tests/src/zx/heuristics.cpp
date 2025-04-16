#include "zx/simplifier/heuristics.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "common/global.hpp"
#include "common/zx.hpp"

using namespace qsyn::zx;
using namespace qsyn::zx::simplify;
using dvlab::Phase;

TEST_CASE("IdentityFusion Heuristic", "[zx][rule][heuristics]") {
    auto const num_left_neighbors   = GENERATE(as<size_t>(), 3, 5, 7);
    auto const num_right_neighbors  = GENERATE(as<size_t>(), 3, 5, 7);
    auto const num_common_neighbors = GENERATE(as<size_t>(), 0, 1, 3);

    ZXGraph g;

    auto const v0 = g.add_vertex(VertexType::z);
    // left neighbor
    auto const left = g.add_vertex(VertexType::z, get_random_phase());
    // right neighbor
    auto const right = g.add_vertex(VertexType::z, get_random_phase());

    g.add_edge(v0, left, EdgeType::hadamard);
    g.add_edge(v0, right, EdgeType::hadamard);

    if (coin_flip()) {
        g.add_edge(left, right, EdgeType::hadamard);
    }

    // neighbors of the left neighbor

    for (size_t i = 0; i < num_left_neighbors; ++i) {
        auto const v = g.add_vertex(VertexType::z, get_random_phase());
        g.add_edge(left, v, EdgeType::hadamard);
    }

    // neighbors of the right neighbor
    for (size_t i = 0; i < num_right_neighbors; ++i) {
        auto const v = g.add_vertex(VertexType::z, get_random_phase());
        g.add_edge(right, v, EdgeType::hadamard);
    }

    // neighbors common to the left and right neighbors
    for (size_t i = 0; i < num_common_neighbors; ++i) {
        auto const v = g.add_vertex(VertexType::z, get_random_phase());
        g.add_edge(left, v, EdgeType::hadamard);
        g.add_edge(right, v, EdgeType::hadamard);
    }

    for (size_t i = 0; i < num_left_neighbors + num_right_neighbors; ++i) {
        for (size_t j = 0; j < num_common_neighbors; ++j) {
            if (coin_flip(0.25)) {
                g.add_edge(i + 3,
                           j + 3 + num_left_neighbors + num_right_neighbors,
                           EdgeType::hadamard);
            }
        }
    }

    // Initialize the graph with vertices and edges suitable for IdentityFusion
    IdentityFusion ifu(0);

    auto const decrease = calculate_2q_decrease(ifu, g);

    auto const e_minus_v_before = gsl::narrow<long>(g.num_edges()) -
                                  gsl::narrow<long>(g.num_vertices());

    REQUIRE(ifu.apply(g));
    g.remove_isolated_vertices();
    auto const e_minus_v_after = gsl::narrow<long>(g.num_edges()) -
                                 gsl::narrow<long>(g.num_vertices());

    REQUIRE(decrease == e_minus_v_before - e_minus_v_after);
}

TEST_CASE("LCompUnfusion Heuristic", "[zx][rule][heuristics]") {
    auto const num_neighbors = GENERATE(as<size_t>(), 5, 7);

    auto const phase = GENERATE(get_random_phase(), Phase(1, 2), Phase(-1, 2));

    auto g = generate_random_lcomp_graph(num_neighbors, phase);

    std::vector<size_t> unfused;
    for (size_t i = 0; i < num_neighbors; ++i) {
        if (coin_flip()) {
            unfused.push_back(i + 1);
        }
    }

    auto lcu = GENERATE_COPY(LCompUnfusion{0, unfused}, LCompUnfusion{0, {}});

    auto const decrease = calculate_2q_decrease(lcu, g);

    auto const e_minus_v_before = gsl::narrow<long>(g.num_edges()) -
                                  gsl::narrow<long>(g.num_vertices());

    REQUIRE(lcu.apply(g));

    auto const e_minus_v_after = gsl::narrow<long>(g.num_edges()) -
                                 gsl::narrow<long>(g.num_vertices());

    REQUIRE(decrease == e_minus_v_before - e_minus_v_after);
}

TEST_CASE("PivotUnfusion Heuristic", "[zx][rule][heuristics]") {
    auto const num_v1_neighbors     = GENERATE(as<size_t>(), 3, 4, 5);
    auto const num_v2_neighbors     = GENERATE(as<size_t>(), 3, 4, 5);
    auto const num_common_neighbors = GENERATE(as<size_t>(), 0, 1, 3);

    auto const phase1 = GENERATE(get_random_phase(), Phase(0), Phase(1));
    auto const phase2 = GENERATE(get_random_phase(), Phase(0), Phase(1));

    auto g = generate_random_pivot_graph(num_v1_neighbors, num_v2_neighbors,
                                         num_common_neighbors, phase1, phase2);

    std::vector<size_t> unfused1;
    std::vector<size_t> unfused2;
    for (size_t i = 0; i < num_v1_neighbors; ++i) {
        if (coin_flip()) {
            unfused1.push_back(i + 2);
        }
    }
    for (size_t i = 0; i < num_v2_neighbors; ++i) {
        if (coin_flip()) {
            unfused2.push_back(i + num_v1_neighbors + 2);
        }
    }

    auto pvu = GENERATE_COPY(PivotUnfusion{0, 1, unfused1, unfused2},
                             PivotUnfusion{0, 1, {}, {}},
                             PivotUnfusion{0, 1, unfused1, {}},
                             PivotUnfusion{0, 1, {}, unfused2});

    auto const decrease = calculate_2q_decrease(pvu, g);

    auto const e_minus_v_before = gsl::narrow<long>(g.num_edges()) -
                                  gsl::narrow<long>(g.num_vertices());

    REQUIRE(pvu.apply(g));

    auto const e_minus_v_after = gsl::narrow<long>(g.num_edges()) -
                                 gsl::narrow<long>(g.num_vertices());

    REQUIRE(decrease == e_minus_v_before - e_minus_v_after);
}
