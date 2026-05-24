// SPDX-License-Identifier: MIT
//
// Diophantine equation solver: given xi ∈ D[√2], find ω-integers t with
// t.conj * t ≡ xi (up to associates).  Faithful port of
// pygridsynth/diophantine.py using GMP for arbitrary-precision integers
// and a primality / factorisation pipeline backed by GMP.
#pragma once

#include <optional>
#include <variant>

#include "cppgridsynth/loop_controller.hpp"
#include "cppgridsynth/ring.hpp"

namespace cppgridsynth {

enum class DiopResult {
    NO_SOLUTION,
    UNSOLVED,
};

// Solver entry-point: returns a DOmega solution or NO_SOLUTION.
std::variant<DOmega, DiopResult> diophantine_dyadic(const DRootTwo& xi,
                                                    int seed,
                                                    LoopController& lc);

// Reseeds the module-level random generator (matches pygridsynth's
// behaviour where each call to the solver re-seeds the RNG).
void set_random_seed(unsigned long seed);

}  // namespace cppgridsynth
