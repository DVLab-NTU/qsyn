#include <fmt/core.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "common/zx.hpp"
#include "util/phase.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("LCompUnfusion matches", "[zx][action]") {
    ZXGraph g;

    g.add_vertex(VertexType::z, Phase(1, 3));

    g.add_vertex(VertexType::z, Phase(2, 3));
    g.add_vertex(VertexType::z, Phase(1, 4));
    g.add_vertex(VertexType::z, Phase(3, 4));
    g.add_vertex(VertexType::z, Phase(1, 5));
    g.add_input(0);

    for (size_t i = 1; i <= 5; i++) {
        g.add_edge(0, i, EdgeType::hadamard);
    }

    LCompUnfusion lcu{0, {4, 5}};

    auto g_before = g;

    REQUIRE(lcu.apply(g));

    REQUIRE(g[0] == nullptr);

    for (size_t i = 1; i <= 5; i++) {
        REQUIRE(g[i] != nullptr);
    }

    // REVIEW - This test relies on the buffer vertex added
    // after the unfused vertices.

    auto const buffer_vertex  = g[7];
    auto const unfused_vertex = g[6];
    REQUIRE(buffer_vertex != nullptr);
    REQUIRE(unfused_vertex != nullptr);

    REQUIRE(buffer_vertex->phase() == Phase(-1, 2));
    REQUIRE(buffer_vertex->type() == VertexType::z);
    REQUIRE(g.num_neighbors(buffer_vertex) == 4);
    REQUIRE(unfused_vertex->type() == VertexType::z);
    REQUIRE(unfused_vertex->phase() == Phase(-1, 6));
    REQUIRE(g.num_neighbors(unfused_vertex) == 3);

    REQUIRE(g.is_neighbor(7, 6, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(4, 6, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(5, 6, EdgeType::hadamard));

    REQUIRE(g.is_neighbor(1, 2, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(1, 3, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(1, 7, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(2, 3, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(2, 7, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(3, 7, EdgeType::hadamard));

    auto g_after = g;
    REQUIRE(lcu.undo(g));

    REQUIRE(g == g_before);

    REQUIRE(lcu.apply(g));
    REQUIRE(g == g_after);
}

TEST_CASE("When LCompUnfusion Reduces to LComp", "[zx][action]") {
    // Add some vertices to the graph
    auto const phase         = GENERATE(Phase(1, 2), Phase(-1, 2));
    auto const num_neighbors = GENERATE(as<size_t>(), 0, 4, 6);

    auto g = generate_random_lcomp_graph(num_neighbors, phase);

    // when the unfusion set is empty, lcomp unfusion is equivalent to lcomp
    LCompUnfusion lcu{0, {}};
    LComp lc{0};

    auto const g_before = g;
    auto g_lc           = g;

    REQUIRE(lcu.apply(g));
    REQUIRE(lc.apply(g_lc));
    REQUIRE(g == g_lc);

    auto const g_after = g;

    REQUIRE(lcu.undo(g));
    REQUIRE(g == g_before);

    REQUIRE(lcu.apply(g));
    REQUIRE(g == g_after);
}

TEST_CASE("LCompUnfusion matches with no unfused neighbors", "[zx][action]") {
    // if the phase is not ±π/2, the phase should still be unfused
    // so that lcomp becomes applicable
    auto const phase         = GENERATE(Phase(1, 3), Phase(7, 4), Phase(0));
    auto const num_neighbors = GENERATE(as<size_t>(), 3, 5);

    auto g = generate_random_lcomp_graph(num_neighbors, phase);

    LCompUnfusion lcu{0, {}};

    auto g_before = g;

    REQUIRE(lcu.apply(g));

    auto const buffer_vertex  = g[num_neighbors + 1];
    auto const unfused_vertex = g[num_neighbors + 2];

    REQUIRE(buffer_vertex != nullptr);
    REQUIRE(unfused_vertex != nullptr);

    auto g_after = g;
    REQUIRE(lcu.undo(g));

    REQUIRE(g == g_before);

    REQUIRE(lcu.apply(g));
    REQUIRE(g == g_after);
}
