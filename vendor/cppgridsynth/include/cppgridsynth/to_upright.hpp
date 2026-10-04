// SPDX-License-Identifier: MIT
//
// Upright-form reduction for the two-dimensional grid problem.
// Mirrors pygridsynth/to_upright.py.
#pragma once

#include <optional>
#include <tuple>

#include "cppgridsynth/grid_op.hpp"
#include "cppgridsynth/region.hpp"

namespace cppgridsynth {

GridOp to_upright_ellipse_pair(const Ellipse& ellipseA,
                               const Ellipse& ellipseB,
                               int verbose = 0);

struct UprightResult {
    GridOp opG;
    Ellipse ellipseA_upright;
    Ellipse ellipseB_upright;
    Rectangle bboxA;
    Rectangle bboxB;
};

UprightResult to_upright_set_pair(const ConvexSet& setA,
                                  const ConvexSet& setB,
                                  std::optional<GridOp> opG = std::nullopt,
                                  int verbose = 0);

}  // namespace cppgridsynth
