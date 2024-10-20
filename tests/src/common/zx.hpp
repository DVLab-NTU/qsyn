#pragma once

#include <cstddef>

#include "zx/zxgraph.hpp"

qsyn::zx::ZXGraph
generate_random_palm_graph(size_t num_neighbors,
                           dvlab::Phase phase,
                           bool boundary_neighbors = false);

qsyn::zx::ZXGraph
generate_random_two_palm_graph(size_t num_v1_nbrs,
                               size_t num_v2_nbrs,
                               size_t num_common_nbrs,
                               dvlab::Phase phase1,
                               dvlab::Phase phase2,
                               bool boundary_neighbors = false);

qsyn::zx::ZXGraph
generate_random_lcomp_graph(size_t num_neighbors,
                            dvlab::Phase phase,
                            bool boundary_neighbors = false);

qsyn::zx::ZXGraph
generate_random_pivot_graph(size_t num_v1_nbrs,
                            size_t num_v2_nbrs,
                            size_t num_common_nbrs,
                            dvlab::Phase phase1,
                            dvlab::Phase phase2,
                            bool boundary_neighbors = false);
