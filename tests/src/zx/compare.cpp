#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <utility>

#include "common/global.hpp"
#include "util/phase.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("Graphs equal", "[zx][basic]") {
    ZXGraph g1, g2;

    auto const vt1 = GENERATE(
        VertexType::z, VertexType::x, VertexType::h_box);
    auto const vt2 = GENERATE(
        VertexType::z, VertexType::x, VertexType::h_box);
    auto const phase1 = get_random_phase();
    auto const phase2 = get_random_phase();

    g1.add_input(0);
    g1.add_input(1);
    g1.add_output(0);
    g1.add_output(1);
    g1.add_vertex(vt1, phase1, 0);
    g1.add_vertex(vt2, phase2, 0);

    g1.add_edge(0, 4, EdgeType::hadamard);
    g1.add_edge(4, 2, EdgeType::simple);
    g1.add_edge(1, 5, EdgeType::hadamard);
    g1.add_edge(5, 3, EdgeType::simple);
    g1.add_edge(4, 5, EdgeType::hadamard);

    g2.add_input(1, 1, 0., 0.);
    g2.add_input(0, 0, 0., 0.);
    g2.add_output(3, 1, 0., 0.);
    g2.add_output(2, 0, 0., 0.);
    g2.add_vertex(5, vt2, phase2, 0);
    g2.add_vertex(4, vt1, phase1, 0);

    g2.add_edge(2, 4, EdgeType::simple);
    g2.add_edge(4, 0, EdgeType::hadamard);
    g2.add_edge(3, 5, EdgeType::simple);
    g2.add_edge(5, 4, EdgeType::hadamard);
    g2.add_edge(5, 1, EdgeType::hadamard);

    REQUIRE(g1 == g2);

    // vertex coordinates does not matter
    g2.vertex(4)->set_row(1);
    g1.vertex(2)->set_col(87);

    REQUIRE(g1 == g2);

    // internal qubit ID does not matter
    g2.vertex(4)->set_qubit(9);
}

TEST_CASE("Boundary qubit ID mismatch", "[zx][basic]") {
    ZXGraph g1, g2;

    g1.add_input(0);
    g1.add_input(1);
    g1.add_output(0);
    g1.add_output(1);
    g1.add_edge(0, 2, EdgeType::simple);
    g1.add_edge(1, 3, EdgeType::simple);

    g2.add_input(0);
    g2.add_input(1);
    g2.add_output(1);
    g2.add_output(0);
    g2.add_edge(0, 2, EdgeType::simple);
    g2.add_edge(1, 3, EdgeType::simple);

    REQUIRE(g1 != g2);
}

TEST_CASE("Vertex ID mismatch", "[zx][basic]") {
    ZXGraph g1, g2;

    g1.add_vertex(0, VertexType::z, Phase(1, 3));
    g2.add_vertex(3, VertexType::z, Phase(1, 3));

    REQUIRE(g1 != g2);
}
