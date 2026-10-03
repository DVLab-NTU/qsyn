// SPDX-License-Identifier: MIT
//
// Free-function helpers used throughout the gridsynth pipeline: integer
// utilities (bit-counts, integer square roots, gcd via GMP, etc.), as
// well as the floating-point helpers that the Python `mymath` module
// exposed (sqrt2, floorlog, solve_quadratic, ...).
#pragma once

#include <gmpxx.h>

#include <cstdint>
#include <optional>
#include <utility>

#include "cppgridsynth/mpfloat.hpp"

namespace cppgridsynth {

// Number of trailing zeros in a non-negative `mpz_class`.  Returns 0 when
// `n == 0` (matches pygridsynth's `ntz`).
long ntz(const mpz_class& n);

// Sign helpers.
int sign(const mpz_class& v);
int sign(const MPFloat& v);
int sign(long v);

// Integer floor square-root (largest k with k*k <= |x|).  Mirrors
// pygridsynth's `floorsqrt` and uses GMP's `mpz_sqrt`.
mpz_class floorsqrt(const mpz_class& x);
mpz_class floorsqrt(const MPFloat& x);

// Banker's-style "round-to-nearest-with-ties-toward-zero" integer
// division used by pygridsynth's `rounddiv`.
mpz_class rounddiv(const mpz_class& x, const mpz_class& y);

// Returns sqrt(2)^k as an MPFloat.
MPFloat pow_sqrt2(long k);

// SQRT(2) at the working precision.
inline MPFloat SQRT2() { return MPFloat::sqrt2(); }

// Number of decimal places mpmath would use to safely represent `epsilon`.
int dps_for_epsilon(const MPFloat& epsilon);

// ⌊log_y(x)⌋  with a side-effect-free implementation; returns (n, r)
// where x = y^n * r and 1 <= r < y for x > 0.
std::pair<long, MPFloat> floorlog(const MPFloat& x, const MPFloat& y);

// Returns the (sorted) real roots of a*x^2 + b*x + c = 0, or
// std::nullopt if discriminant < 0.  Matches pygridsynth.
std::optional<std::pair<MPFloat, MPFloat>> solve_quadratic(const MPFloat& a,
                                                           const MPFloat& b,
                                                           const MPFloat& c);

// Convenience: floor/ceil from various types.
inline mpz_class mpf_floor(const MPFloat& x) { return floor(x); }
inline mpz_class mpf_ceil(const MPFloat& x) { return ceil(x); }

}  // namespace cppgridsynth
