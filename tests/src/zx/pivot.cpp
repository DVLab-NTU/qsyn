#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>

#include "common/zx.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("Pivot matches", "[zx][action]") {
    auto const phase1 = GENERATE(Phase(0), Phase(1));
    auto const phase2 = GENERATE(Phase(0), Phase(1));

    auto const num_v1_nbrs     = GENERATE(as<size_t>(), 0, 3, 5);
    auto const num_v2_nbrs     = GENERATE(as<size_t>(), 0, 3, 5);
    auto const num_common_nbrs = GENERATE(as<size_t>(), 0, 1, 3);

    auto const v1_nbrs_range = std::views::iota(
        2ul, num_v1_nbrs + 2);
    auto const v2_nbrs_range = std::views::iota(
        2ul + num_v1_nbrs, num_v2_nbrs + 2 + num_v1_nbrs);
    auto const common_nbrs_range = std::views::iota(
        2 + num_v1_nbrs + num_v2_nbrs,
        num_common_nbrs + 2 + num_v1_nbrs + num_v2_nbrs);

    auto g = generate_random_pivot_graph(
        num_v1_nbrs, num_v2_nbrs, num_common_nbrs, phase1, phase2);

    Pivot p{0, 1};

    auto const g_before = g;

    REQUIRE(p.apply(g));

    REQUIRE(g[0] == nullptr);
    REQUIRE(g[1] == nullptr);

    // exactly one edge of the original/rewritten graph should be hadamard

    auto const check_edges = [&](auto const& range1, auto const& range2) {
        for (auto i : range1) {
            for (auto j : range2) {
                auto const old_edge = g_before.get_edge_type(i, j);
                auto const new_edge = g.get_edge_type(i, j);
                REQUIRE((old_edge == EdgeType::hadamard) ^
                        (new_edge == EdgeType::hadamard));
            }
        }
    };

    check_edges(v1_nbrs_range, v2_nbrs_range);
    check_edges(v1_nbrs_range, common_nbrs_range);
    check_edges(v2_nbrs_range, common_nbrs_range);

    auto const g_after = g;

    REQUIRE(p.undo(g));
    REQUIRE(g == g_before);

    REQUIRE(p.apply(g));
    REQUIRE(g == g_after);
}

TEST_CASE("Pivot rule with some neighbors being boundaries", "[zx][action]") {
    auto const phase1 = GENERATE(Phase(0), Phase(1));
    auto const phase2 = GENERATE(Phase(0), Phase(1));

    auto const num_v1_nbrs     = GENERATE(as<size_t>(), 0, 3, 5);
    auto const num_v2_nbrs     = GENERATE(as<size_t>(), 0, 3, 5);
    auto const num_common_nbrs = GENERATE(as<size_t>(), 0, 1, 3);

    auto g = generate_random_pivot_graph(
        num_v1_nbrs, num_v2_nbrs, num_common_nbrs, phase1, phase2, true);

    Pivot p{0, 1};

    auto const g_before = g;

    REQUIRE(p.apply(g));

    REQUIRE(g[0] == nullptr);
    REQUIRE(g[1] == nullptr);

    auto const g_after = g;

    REQUIRE(p.undo(g));
    REQUIRE(g == g_before);

    REQUIRE(p.apply(g));
    REQUIRE(g == g_after);
}
