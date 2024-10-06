#include <catch2/catch_test_macros.hpp>

#include "util/phase.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("Identity Fusion", "[zx]") {
    ZXGraph g;
    g.add_vertex(VertexType::z);
    g.add_vertex(VertexType::z, Phase(1, 3));
    g.add_vertex(VertexType::z, Phase(1, 4));
    g.add_vertex(VertexType::z);
    g.add_vertex(VertexType::z);
    g.add_vertex(VertexType::z);

    g.add_input(0);
    g.add_output(0);

    g.add_edge(0, 1, EdgeType::hadamard);
    g.add_edge(0, 2, EdgeType::hadamard);
    g.add_edge(1, 2, EdgeType::hadamard);
    g.add_edge(1, 3, EdgeType::hadamard);
    g.add_edge(2, 3, EdgeType::hadamard);
    g.add_edge(1, 4, EdgeType::hadamard);
    g.add_edge(2, 5, EdgeType::hadamard);
    g.add_edge(1, 6, EdgeType::simple);
    g.add_edge(2, 7, EdgeType::simple);

    IdentityFusion ifu{0};
    auto g_before = g;

    REQUIRE(ifu.apply(g));
    REQUIRE(g[0] == nullptr);
    REQUIRE(g[1] != nullptr);
    REQUIRE(g[2] == nullptr);
    REQUIRE(g.is_neighbor(1, 4, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(1, 5, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(1, 6, EdgeType::simple));
    REQUIRE(g.is_neighbor(1, 7, EdgeType::simple));
    REQUIRE(g.num_neighbors(g[3]) == 0);

    REQUIRE(g[1]->phase() == Phase(1, 3) + Phase(1, 4) + Phase(1));

    auto g_after = g;

    REQUIRE(ifu.undo(g));
    REQUIRE(g == g_before);

    REQUIRE(ifu.apply(g));
    REQUIRE(g == g_after);
}
