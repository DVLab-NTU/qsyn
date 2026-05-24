// SPDX-License-Identifier: MIT
//
// One-dimensional grid problem (ODGP) solvers.  Faithful port of
// pygridsynth/odgp.py.  Solutions are produced lazily through
// generator-style closures (std::function returning std::optional<...>).
#pragma once

#include <functional>
#include <optional>

#include "cppgridsynth/mpfloat.hpp"
#include "cppgridsynth/region.hpp"
#include "cppgridsynth/ring.hpp"

namespace cppgridsynth {

using ZRootTwoGen = std::function<std::optional<ZRootTwo>()>;
using DRootTwoGen = std::function<std::optional<DRootTwo>()>;

// Empty generators (used when an early-out shortcut wants to return "no
// solutions" without allocating an internal state machine).
ZRootTwoGen empty_zroottwo_gen();
DRootTwoGen empty_droottwo_gen();

// Core problem: enumerate ZRootTwo β with β ∈ I and β.conj_sq2 ∈ J.
ZRootTwoGen solve_ODGP(const Interval& I, const Interval& J);

// Same problem with a parity constraint via β.
ZRootTwoGen solve_ODGP_with_parity(const Interval& I, const Interval& J,
                                   const ZRootTwo& beta);

// Scaled problem: β ∈ DRootTwo at denomexp k.
DRootTwoGen solve_scaled_ODGP(const Interval& I, const Interval& J, long k);

// Scaled with parity.
DRootTwoGen solve_scaled_ODGP_with_parity(const Interval& I, const Interval& J,
                                          long k, const DRootTwo& beta);

}  // namespace cppgridsynth
