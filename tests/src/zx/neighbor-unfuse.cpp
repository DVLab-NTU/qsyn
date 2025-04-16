#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <tl/enumerate.hpp>

#include "common/global.hpp"
#include "util/phase.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

TEST_CASE("Neighbor Unfusion matches", "[zx][action]") {
    auto const vtype         = GENERATE(VertexType::z, VertexType::x);
    auto const phase         = GENERATE(take(2, value(get_random_phase())));
    auto const phase_to_keep = GENERATE(take(2, value(get_random_phase())));

    auto const num_neighbors = GENERATE(as<size_t>(), 5, 7);
    auto const num_unfused   = GENERATE(as<size_t>(), 2, 4);

    ZXGraph g;
    g.add_vertex(vtype, phase);
    for (size_t i = 0; i < num_neighbors; i++) {
        g.add_vertex(VertexType::z, get_random_phase());
        g.add_edge(0,
                   i + 1,
                   coin_flip() ? EdgeType::hadamard : EdgeType::simple);
    }

    // randomly pick `num_unfused` vertices to be unfused
    auto const shuffled_id_seq = get_shuffle_seq(
        std::views::iota(1ul, num_neighbors + 1) | tl::to<std::vector>());

    std::vector<size_t> neighbors_to_unfuse;
    for (size_t i = 0; i < num_unfused; i++) {
        neighbors_to_unfuse.emplace_back(shuffled_id_seq[i]);
    }

    NeighborUnfusion nu{0, phase_to_keep, neighbors_to_unfuse};

    auto g_before = g;

    REQUIRE(nu.apply(g));

    REQUIRE(g[0]->phase() == phase_to_keep);

    auto const buffer = [&]() -> ZXVertex* {
        for (auto const& [nb, etype] : g.get_neighbors(g[0])) {
            if (nb->get_id() > num_neighbors) return nb;
        }
        return nullptr;
    }();

    REQUIRE(buffer != nullptr);
    REQUIRE(buffer->is_zx());
    REQUIRE(g.num_neighbors(buffer) == 2);

    auto const unfuse = [&]() -> ZXVertex* {
        for (auto const& [nb, etype] : g.get_neighbors(buffer)) {
            if (nb != g[0]) return nb;
        }
        return nullptr;
    }();

    REQUIRE(unfuse != nullptr);
    REQUIRE(unfuse->type() == vtype);
    REQUIRE(unfuse->phase() == phase - phase_to_keep);
    REQUIRE(g.is_neighbor(g[0], buffer, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(unfuse, buffer, EdgeType::hadamard));

    for (auto const& [i, nb_id] : tl::views::enumerate(shuffled_id_seq)) {
        if (i < num_unfused) {
            REQUIRE(!g.is_neighbor(0, nb_id));
            REQUIRE(g.is_neighbor(unfuse->get_id(), nb_id));
        } else {
            REQUIRE(g.is_neighbor(0, nb_id));
            REQUIRE(!g.is_neighbor(unfuse->get_id(), nb_id));
        }
    }

    auto g_after = g;
    REQUIRE(nu.undo(g));
    REQUIRE(g == g_before);

    REQUIRE(nu.apply(g));
    REQUIRE(g == g_after);
}

TEST_CASE("NeighborUnfusion No Neighbors to Unfuse", "[zx][action]") {
    ZXGraph g;
    // Add a single vertex to the graph
    auto const phase_to_keep = get_random_phase();
    g.add_vertex(VertexType::z, phase_to_keep);

    NeighborUnfusion nu{0, get_random_phase(), {}};

    auto g_before = g;

    // Apply the NeighborUnfusion rule
    REQUIRE(nu.apply(g));

    // The graph should remain unchanged as there are no neighbors to unfuse
    REQUIRE(g[1] != nullptr);
    REQUIRE(g[2] != nullptr);
    REQUIRE(g.num_vertices() == 3);
    REQUIRE(g.is_neighbor(0, 2, EdgeType::hadamard));
    REQUIRE(g.is_neighbor(1, 2, EdgeType::hadamard));
    auto g_after = g;
    // Undo the NeighborUnfusion rule
    REQUIRE(nu.undo(g));
    REQUIRE(g == g_before);

    // Redo the NeighborUnfusion rule
    REQUIRE(nu.apply(g));
    REQUIRE(g == g_after);
}
