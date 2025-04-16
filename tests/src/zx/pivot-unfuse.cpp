#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "common/zx.hpp"
#include "util/phase.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("PivotUnfusion matches", "[zx][action]") {
    auto g = generate_random_pivot_graph(
        3, 3, 1, Phase(1, 3), Phase(2, 3));

    // add boundary to v1
    g.add_input(0);
    g.add_edge(0, 9, EdgeType::hadamard);

    // add boundary to v2
    g.add_input(1);
    g.add_edge(1, 10, EdgeType::hadamard);

    std::vector<PivotUnfusion> pvu_list;
    pvu_list.push_back({0, 1, {3, 9}, {6, 10}});
    pvu_list.push_back({0, 1, {}, {7, 10}});
    pvu_list.push_back({0, 1, {4, 9}, {}});

    auto const g_before = g;

    for (auto const& pvu : pvu_list) {
        g = g_before;
        REQUIRE(pvu.apply(g));
        REQUIRE(g[0] == nullptr);
        REQUIRE(g[1] == nullptr);

        auto const g_after = g;
        REQUIRE(pvu.undo(g));
        REQUIRE(g == g_before);

        REQUIRE(pvu.apply(g));
        REQUIRE(g == g_after);
    }
}

TEST_CASE("When PivotUnfusion Reduces to Pivot", "[zx][action]") {
    auto const phase1 = GENERATE(Phase(0), Phase(1));
    auto const phase2 = GENERATE(Phase(0), Phase(1));

    auto const num_v1_nbrs     = GENERATE(as<size_t>(), 0, 3, 5);
    auto const num_v2_nbrs     = GENERATE(as<size_t>(), 0, 3, 5);
    auto const num_common_nbrs = GENERATE(as<size_t>(), 0, 1, 3);

    auto g = generate_random_pivot_graph(
        num_v1_nbrs, num_v2_nbrs, num_common_nbrs, phase1, phase2);

    PivotUnfusion pvu{0, 1, {}, {}};
    Pivot pv{0, 1};

    auto const g_before = g;
    auto g_pv           = g;

    REQUIRE(pvu.apply(g));
    REQUIRE(pv.apply(g_pv));
    REQUIRE(g == g_pv);

    auto const g_after = g;

    REQUIRE(pvu.undo(g));
    REQUIRE(g == g_before);

    REQUIRE(pvu.apply(g));
    REQUIRE(g == g_after);
}
