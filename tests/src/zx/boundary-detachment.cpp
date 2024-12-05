#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <utility>

#include "common/global.hpp"
#include "util/phase.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("Boundary Detachment", "[zx][action]") {
    ZXGraph g;

    g.add_vertex(VertexType::z);

    auto num_neighbors = GENERATE(as<size_t>(), 3, 4, 5, 6, 7);

    for (size_t i = 0; i < num_neighbors; ++i) {
        coin_flip() ? g.add_vertex(VertexType::z) : g.add_input(i);
        g.add_edge(
            0, i + 1, coin_flip() ? EdgeType::hadamard : EdgeType::simple);
    }

    BoundaryDetachment bd{0};

    auto g_before = g;

    REQUIRE(bd.apply(g));

    for (size_t i = 1; i < num_neighbors + 1; ++i) {
        REQUIRE(g[i] != nullptr);

        if (g[i]->is_boundary()) {
            // vertex 0 should be detached from the boundary
            REQUIRE(!g.is_neighbor(0, i));

            auto [buffer, _] = g.get_first_neighbor(g[i]);
            REQUIRE(buffer != nullptr);
            REQUIRE(buffer->is_z());
            REQUIRE(buffer->phase() == Phase(0));
            REQUIRE(g.num_neighbors(buffer) == 2);
            REQUIRE(g.is_neighbor(g[0], buffer, EdgeType::hadamard));
        } else {
            // vertex 0 should be connected to the internal vertices
            REQUIRE(g.is_neighbor(0, i));
        }
    }

    auto g_after = g;

    REQUIRE(bd.undo(g));
    REQUIRE(g == g_before);
    REQUIRE(bd.apply(g));
    REQUIRE(g == g_after);
}

TEST_CASE("Boundary Detachment void application", "[zx][action]") {
    ZXGraph g;

    g.add_vertex(VertexType::z);

    BoundaryDetachment bd{0};

    auto num_neighbors = GENERATE(as<size_t>(), 3, 4, 5, 6, 7);

    // when all neighbors are not boundary vertices
    for (size_t i = 0; i < num_neighbors; ++i) {
        g.add_vertex(VertexType::z);
        g.add_edge(
            0, i + 1, coin_flip() ? EdgeType::hadamard : EdgeType::simple);
    }

    auto const g_before = g;

    REQUIRE(bd.apply(g));
    REQUIRE(g == g_before);
    REQUIRE(bd.undo(g));
    REQUIRE(g == g_before);
    REQUIRE(bd.apply(g));
    REQUIRE(g == g_before);
}
