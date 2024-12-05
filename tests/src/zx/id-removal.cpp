#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <random>

#include "common/global.hpp"
#include "util/phase.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("Identity Removal Matches", "[zx][action]") {
    ZXGraph g;
    auto const vtype      = GENERATE(VertexType::z, VertexType::x);
    auto const left_edge  = GENERATE(EdgeType::simple, EdgeType::hadamard);
    auto const right_edge = GENERATE(EdgeType::simple, EdgeType::hadamard);

    g.add_vertex(VertexType::z, Phase(1, 3));
    g.add_vertex(VertexType::z, Phase(2, 3));
    g.add_vertex(vtype, Phase(0));

    g.add_edge(g[0], g[2], left_edge);
    g.add_edge(g[2], g[1], right_edge);

    IdentityRemoval ir{2};
    REQUIRE(ir.apply(g));
    REQUIRE(g[0] != nullptr);
    REQUIRE(g[1] != nullptr);
    REQUIRE(g[2] == nullptr);
    REQUIRE(g.is_neighbor(0, 1));
    REQUIRE(g.get_edge_type(0, 1) == concat_edge(left_edge, right_edge));
}

TEST_CASE("Identity Removal Are Associative", "[zx][action]") {
    ZXGraph g;
    g.add_input(0);
    g.add_output(0);

    g.add_vertex(VertexType::z);
    g.add_vertex(VertexType::z);
    g.add_vertex(VertexType::z);
    g.add_vertex(VertexType::z);

    g.add_edge(0, 2, EdgeType::hadamard);
    g.add_edge(2, 3, EdgeType::simple);
    g.add_edge(3, 4, EdgeType::hadamard);
    g.add_edge(4, 5, EdgeType::hadamard);
    g.add_edge(5, 1, EdgeType::simple);

    using namespace Catch::Generators;
    auto vid_seq = GENERATE(
        take(8,
             value([v = std::vector<size_t>{2, 3, 4, 5}]() {
                 return get_shuffle_seq(v);
             }())));

    for (auto vid : vid_seq) {
        IdentityRemoval ir{vid};
        REQUIRE(ir.apply(g));
    }

    REQUIRE(g.is_identity());
    REQUIRE(g.num_vertices() == 2);
    REQUIRE(g[0] != nullptr);
    REQUIRE(g[1] != nullptr);
    REQUIRE(g.is_neighbor(0, 1, EdgeType::hadamard));
}

TEST_CASE("Identity Removal Undo/Redo", "[zx][action]") {
    ZXGraph g;
    g.add_vertex(VertexType::z, Phase(1, 3));
    g.add_vertex(VertexType::z, Phase(0));
    g.add_vertex(VertexType::x, Phase(2, 3));

    g.add_edge(0, 1, EdgeType::simple);
    g.add_edge(1, 2, EdgeType::hadamard);

    SECTION("Apply Success") {
        IdentityRemoval ir{1};
        REQUIRE(ir.apply(g));
        REQUIRE(g[1] == nullptr);

        SECTION("Undo Success") {
            REQUIRE(ir.undo(g));
            REQUIRE(g[1] != nullptr);

            SECTION("Redo Success") {
                REQUIRE(ir.apply(g));
                REQUIRE(g[1] == nullptr);
            }

            SECTION("Redo Fail because vertex has been removed") {
                g.remove_vertex(g[1]);
                REQUIRE_FALSE(ir.apply(g));
                REQUIRE(g[1] == nullptr);
            }
        }

        SECTION("Undo Fail because neighbor has been removed") {
            g.remove_vertex(g[2]);
            REQUIRE_FALSE(ir.undo(g));
            REQUIRE(g[1] == nullptr);
        }

        SECTION("Undo Fail because vertex has been added") {
            g.add_vertex(1, VertexType::z, Phase(0));
            REQUIRE_FALSE(ir.undo(g));
        }
    }

    SECTION("Apply Fail") {
        g[1]->phase() = Phase(1, 3);
        IdentityRemoval ir{1};
        REQUIRE_FALSE(ir.apply(g));
        REQUIRE(g[1] != nullptr);
    }
}
