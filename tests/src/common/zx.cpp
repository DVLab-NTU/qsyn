#include "common/zx.hpp"

#include "common/global.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;
using dvlab::Phase;

ZXGraph generate_random_palm_graph(size_t num_neighbors,
                                   Phase phase,
                                   bool boundary_neighbors) {
    ZXGraph g;

    g.add_vertex(qsyn::zx::VertexType::z, phase);

    for (auto i : std::views::iota(0ul, num_neighbors)) {
        if (boundary_neighbors && coin_flip(0.25)) {
            g.add_input(i);
        } else {
            g.add_vertex(qsyn::zx::VertexType::z, get_random_phase());
        }
        g.add_edge(0, i + 1, qsyn::zx::EdgeType::hadamard);
    }

    return g;
}

/**
 * @brief generate a random lcomp graph. The graph will have a Z-spider with the
 *        given phase as the first vertex, and the rest of the vertices will be
 *        Z-spiders with random phases. The first vertex will have edges to all
 *        other vertices, and the rest of the vertices will have edges between
 *        them with a 50% chance.
 *
 * @param num_neighbors number of neighbors for the first vertex
 * @param phase phase of the first vertex
 * @param boundary_neighbors whether the neighbors of the first vertex should be
 * @return qsyn::zx::ZXGraph
 */
ZXGraph
generate_random_lcomp_graph(size_t num_neighbors,
                            Phase phase,
                            bool boundary_neighbors) {
    auto g = generate_random_palm_graph(
        num_neighbors, phase, boundary_neighbors);

    for (size_t i = 1; i <= num_neighbors; ++i) {
        if (g[i]->is_boundary()) continue;
        for (size_t j = i + 1; j <= num_neighbors; ++j) {
            if (g[j]->is_boundary()) continue;
            if (coin_flip()) {
                g.add_edge(i, j, qsyn::zx::EdgeType::hadamard);
            }
        }
    }

    return g;
}

ZXGraph
generate_random_two_palm_graph(size_t num_v1_nbrs,
                               size_t num_v2_nbrs,
                               size_t num_common_nbrs,
                               dvlab::Phase phase1,
                               dvlab::Phase phase2,
                               bool boundary_neighbors) {
    ZXGraph g;

    g.add_vertex(VertexType::z, phase1);
    g.add_vertex(VertexType::z, phase2);

    g.add_edge(0, 1, EdgeType::hadamard);

    auto const v1_nbrs_range = std::views::iota(
        2ul, num_v1_nbrs + 2);
    auto const v2_nbrs_range = std::views::iota(
        2ul + num_v1_nbrs, num_v2_nbrs + 2 + num_v1_nbrs);
    auto const common_nbrs_range = std::views::iota(
        2 + num_v1_nbrs + num_v2_nbrs,
        num_common_nbrs + 2 + num_v1_nbrs + num_v2_nbrs);

    for (auto i : v1_nbrs_range) {
        if (boundary_neighbors && coin_flip(0.25)) {
            g.add_input(i);
        } else {
            g.add_vertex(VertexType::z, get_random_phase());
        }
        g.add_edge(0, i, EdgeType::hadamard);
    }

    for (auto i : v2_nbrs_range) {
        if (boundary_neighbors && coin_flip(0.25)) {
            g.add_input(i);
        } else {
            g.add_vertex(VertexType::z, get_random_phase());
        }
        g.add_edge(1, i, EdgeType::hadamard);
    }

    for (auto i : common_nbrs_range) {
        g.add_vertex(VertexType::z, get_random_phase());
        g.add_edge(0, i, EdgeType::hadamard);
        g.add_edge(1, i, EdgeType::hadamard);
    }

    return g;
}

ZXGraph
generate_random_pivot_graph(size_t num_v1_nbrs,
                            size_t num_v2_nbrs,
                            size_t num_common_nbrs,
                            dvlab::Phase phase1,
                            dvlab::Phase phase2,
                            bool boundary_neighbors) {
    auto g = generate_random_two_palm_graph(
        num_v1_nbrs, num_v2_nbrs, num_common_nbrs, phase1, phase2,
        boundary_neighbors);

    auto const v1_nbrs_range = std::views::iota(
        2ul, num_v1_nbrs + 2);
    auto const v2_nbrs_range = std::views::iota(
        2ul + num_v1_nbrs, num_v2_nbrs + 2 + num_v1_nbrs);
    auto const common_nbrs_range = std::views::iota(
        2 + num_v1_nbrs + num_v2_nbrs,
        num_common_nbrs + 2 + num_v1_nbrs + num_v2_nbrs);

    // random connections between the three groups

    auto const add_random_edges =
        [&](auto const& range1, auto const& range2) {
            for (auto i : range1) {
                if (g[i]->is_boundary()) continue;
                for (auto j : range2) {
                    if (g[j]->is_boundary()) continue;
                    if (coin_flip()) {
                        g.add_edge(i, j, EdgeType::hadamard);
                    }
                }
            }
        };

    add_random_edges(v1_nbrs_range, v2_nbrs_range);
    add_random_edges(v1_nbrs_range, common_nbrs_range);
    add_random_edges(v2_nbrs_range, common_nbrs_range);

    return g;
}
