#include <catch2/catch_test_macros.hpp>

#include "util/phase.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("Identity Removal", "[zx][action]") {
    ZXGraph g;
    g.add_vertex(VertexType::z, Phase(1, 3));
    g.add_vertex(VertexType::z, Phase(0));
    g.add_vertex(VertexType::x, Phase(2, 3));

    g.add_edge(g[0], g[1], EdgeType::simple);
    g.add_edge(g[1], g[2], EdgeType::hadamard);

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
        g[1]->set_phase(Phase(1, 3));
        IdentityRemoval ir{1};
        REQUIRE_FALSE(ir.apply(g));
        REQUIRE(g[1] != nullptr);
    }
}
