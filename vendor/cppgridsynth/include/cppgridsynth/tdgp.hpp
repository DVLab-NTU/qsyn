// SPDX-License-Identifier: MIT
//
// Two-dimensional grid problem (TDGP) solver.  Faithful port of
// pygridsynth/tdgp.py.
#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "cppgridsynth/grid_op.hpp"
#include "cppgridsynth/region.hpp"
#include "cppgridsynth/ring.hpp"

namespace cppgridsynth {

using DOmegaGen = std::function<std::optional<DOmega>()>;

// `setA`, `setB` are the original convex sets; `opG`, ellipses,
// rectangles are the precomputed upright form.
DOmegaGen solve_TDGP(const ConvexSet& setA,
                     const ConvexSet& setB,
                     const GridOp& opG,
                     const Ellipse& ellipseA_upright,
                     const Ellipse& ellipseB_upright,
                     const Rectangle& bboxA,
                     const Rectangle& bboxB,
                     long k,
                     int verbose = 0);

}  // namespace cppgridsynth
