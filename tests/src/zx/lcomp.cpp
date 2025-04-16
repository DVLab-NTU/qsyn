#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>

#include "common/zx.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("LComp Matches", "[zx][action]") {
    auto const phase           = GENERATE(Phase(1, 2), Phase(-1, 2));
    size_t const num_neighbors = GENERATE(3, 5, 7);

    auto g = generate_random_lcomp_graph(num_neighbors, phase);

    LComp lc{0};

    auto const g_before = g;

    REQUIRE(lc.apply(g));

    REQUIRE(g[0] == nullptr);

    for (size_t i = 1; i <= num_neighbors; ++i) {
        for (size_t j = i + 1; j <= num_neighbors; ++j) {
            auto const old_edge = g_before.get_edge_type(i, j);
            auto const new_edge = g.get_edge_type(i, j);
            // exactly one of the edges should be hadamard
            REQUIRE((old_edge == EdgeType::hadamard) ^
                    (new_edge == EdgeType::hadamard));
        }
    }

    auto const g_after = g;

    REQUIRE(lc.undo(g));
    REQUIRE(g == g_before);

    REQUIRE(lc.apply(g));
    REQUIRE(g == g_after);
}

TEST_CASE("LComp Match with boundary as neighbors", "[zx][action]") {
    auto const phase           = GENERATE(Phase(1, 2), Phase(-1, 2));
    size_t const num_neighbors = GENERATE(3, 5, 7);

    auto g = generate_random_lcomp_graph(num_neighbors, phase, true);

    LComp lc{0};

    auto const g_before = g;

    REQUIRE(lc.apply(g));
    auto const g_after = g;
    REQUIRE(lc.undo(g));
    REQUIRE(g == g_before);
    REQUIRE(lc.apply(g));
    REQUIRE(g == g_after);
}

TEST_CASE("LComp Edge Case", "[zx][action]") {
    ZXGraph g;
    // Add some vertices to the graph
    auto const phase = GENERATE(Phase(1, 2), Phase(-1, 2));
    g.add_vertex(VertexType::z, phase);

    LComp lc{0};  // Empty unfuse set

    auto const g_before = g;

    REQUIRE(lc.apply(g));

    // should remove the only vertex
    REQUIRE(g.is_empty());

    auto const g_after = g;

    REQUIRE(lc.undo(g));
    REQUIRE(g == g_before);

    REQUIRE(lc.apply(g));
    REQUIRE(g == g_after);
}
