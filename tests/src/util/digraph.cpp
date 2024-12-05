#include "util/graph/digraph.hpp"

#include <catch2/catch_get_random_seed.hpp>
#include <catch2/catch_test_macros.hpp>

#include "util/graph/minimum_spanning_arborescence.hpp"

TEST_CASE("digraph", "[digraph]") {
    using Digraph = dvlab::Digraph<int, int>;
    auto g        = Digraph{};

    REQUIRE(g.num_vertices() == 0);
    REQUIRE(g.num_edges() == 0);

    auto v1 = g.add_vertex();
    auto v2 = g.add_vertex(2);
    auto v3 = g.add_vertex(3);

    REQUIRE(g.num_vertices() == 3);
    REQUIRE(g[v1] == 0);
    REQUIRE(g[v2] == 2);
    REQUIRE(g[v3] == 3);

    auto n = g.remove_vertex(v1);
    REQUIRE(n == 1);
    REQUIRE(g.num_vertices() == 2);
    REQUIRE(g[v2] == 2);
    REQUIRE(g[v3] == 3);

    n = g.remove_vertex(87);
    REQUIRE(n == 0);

    auto e1 = g.add_edge(v2, v2);
    REQUIRE(g.num_edges() == 1);
    REQUIRE(g[e1] == 0);
    REQUIRE(e1 == Digraph::Edge{v2, v2});

    auto e2 = g.add_edge(v2, v3, 42);
    REQUIRE(g.num_edges() == 2);
    REQUIRE(g[e2] == 42);
    REQUIRE(e2 == Digraph::Edge{v2, v3});

    auto e3 = g.add_edge(v3, v2, 43);
    REQUIRE(g.num_edges() == 3);
    REQUIRE(g[e3] == 43);
    REQUIRE(e3 == Digraph::Edge{v3, v2});

    // remove edge
    n = g.remove_edge(e1);
    REQUIRE(n == 1);
    REQUIRE(g.num_edges() == 2);
    REQUIRE(!g.has_edge(v2, v2));
    REQUIRE(g.has_edge(v2, v3));
    REQUIRE(g.has_edge(v3, v2));

    n = g.remove_edge(e1.src, e1.dst);
    REQUIRE(n == 0);
}

TEST_CASE("minimum spanning arborescence 1", "[digraph]") {
    using Digraph = dvlab::Digraph<int, int>;
    auto g        = Digraph{3};

    g.add_edge(0, 1, -1);
    g.add_edge(1, 0, -2);
    g.add_edge(0, 2, -3);
    g.add_edge(2, 0, -2);
    g.add_edge(1, 2, -2);
    g.add_edge(2, 1, 0);

    auto const mst1 = dvlab::minimum_spanning_arborescence(g, 0);

    auto mst1_expected = Digraph{3};
    mst1_expected.add_edge(0, 1, -1);
    mst1_expected.add_edge(0, 2, -3);

    REQUIRE(mst1 == mst1_expected);

    auto const mst2 = dvlab::minimum_spanning_arborescence(g, 1);

    auto mst2_expected = Digraph{3};
    mst2_expected.add_edge(1, 0, -2);
    mst2_expected.add_edge(0, 2, -3);

    REQUIRE(mst2 == mst2_expected);

    auto const [best_mst, root] = dvlab::minimum_spanning_arborescence(g);
    REQUIRE(best_mst == mst2_expected);
    REQUIRE(root == 1);
}

TEST_CASE("minimum spanning arborescence 2", "[digraph]") {
    using Digraph = dvlab::Digraph<size_t, int>;
    auto g        = Digraph{4};

    g.add_edge(0, 1, -10);
    g.add_edge(1, 0, -8);
    g.add_edge(0, 2, -4);
    g.add_edge(2, 0, -2);
    g.add_edge(0, 3, -9);
    g.add_edge(3, 0, -11);
    g.add_edge(1, 2, -10);
    g.add_edge(2, 1, -12);
    g.add_edge(1, 3, -3);
    g.add_edge(3, 1, -2);
    g.add_edge(2, 3, -7);
    g.add_edge(3, 2, -6);
    auto const mst =
        dvlab::minimum_spanning_arborescence(g, 0);

    auto mst_expected = Digraph{4};
    mst_expected.add_edge(0, 1, -10);
    mst_expected.add_edge(1, 2, -10);
    mst_expected.add_edge(0, 3, -9);

    REQUIRE(mst == mst_expected);
}
