// SPDX-License-Identifier: MIT
#include "cppgridsynth/mymath.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace cppgridsynth {

long ntz(const mpz_class& n) {
    if (n == 0) return 0;
    return static_cast<long>(mpz_scan1(n.get_mpz_t(), 0));
}

int sign(const mpz_class& v) { return mpz_sgn(v.get_mpz_t()); }
int sign(const MPFloat& v) { return v.sgn(); }
int sign(long v) { return (v > 0) - (v < 0); }

mpz_class floorsqrt(const mpz_class& x) {
    if (sign(x) < 0) {
        throw std::domain_error("floorsqrt(): negative argument");
    }
    mpz_class out;
    mpz_sqrt(out.get_mpz_t(), x.get_mpz_t());
    return out;
}

mpz_class floorsqrt(const MPFloat& x) {
    if (x.sgn() < 0) {
        throw std::domain_error("floorsqrt(): negative argument");
    }
    // Convert to mpz by ceil, then perform integer-sqrt and adjust.  We
    // mimic pygridsynth's binary search: largest k with k*k <= x.
    mpz_class hi = ceil(x) + 1;
    mpz_class lo = 0;
    while (hi - lo > 1) {
        mpz_class mid = (hi + lo) / 2;
        MPFloat midsq = MPFloat(mid) * MPFloat(mid);
        if (midsq <= x) lo = mid;
        else            hi = mid;
    }
    return lo;
}

mpz_class rounddiv(const mpz_class& x, const mpz_class& y) {
    if (sign(y) == 0) throw std::domain_error("rounddiv(): division by zero");
    mpz_class half;
    mpz_class numerator;
    if (sign(y) > 0) {
        half = y / 2;
        numerator = x + half;
    } else {
        half = (-y) / 2;
        numerator = x - half;
    }
    mpz_class q;
    mpz_fdiv_q(q.get_mpz_t(), numerator.get_mpz_t(), y.get_mpz_t());
    return q;
}

MPFloat pow_sqrt2(long k) {
    long absk = k < 0 ? -k : k;
    long div2 = absk >> 1;
    long mod2 = absk & 1;

    MPFloat base(1);
    // Fast path: 2^div2 multiplication via mpfr_mul_2si on `base`.
    mpfr_mul_2si(base.raw(), base.raw(), div2, MPFR_RNDN);
    if (mod2) base *= MPFloat::sqrt2();

    if (k < 0) {
        MPFloat one(1);
        return one / base;
    }
    return base;
}

int dps_for_epsilon(const MPFloat& epsilon) {
    // Mirrors pygridsynth's heuristic: dps = 15 + ceil(2.5 * |log10(epsilon)|)
    if (epsilon.is_zero()) return 100;  // arbitrary high default
    MPFloat lg = log10(abs(epsilon));
    mpz_class k = ceil(-lg);
    long kl = k.get_si();
    if (kl < 0) kl = 0;
    long dps = 15 + static_cast<long>(2.5 * kl);
    if (dps < 30) dps = 30;
    return static_cast<int>(dps);
}

std::pair<long, MPFloat> floorlog(const MPFloat& x_in, const MPFloat& y_in) {
    if (x_in <= 0) throw std::domain_error("floorlog(): non-positive argument");

    MPFloat x = x_in;
    MPFloat y = y_in;

    // Squaring loop to bracket exponent.
    MPFloat tmp = y;
    long m = 0;
    while (x >= tmp || x * tmp < 1) {
        tmp *= tmp;
        ++m;
    }

    // Build a list of squared powers of y from y^(2^(m-1)) downwards.
    std::vector<MPFloat> pow_y;
    pow_y.reserve(static_cast<size_t>(m + 1));
    pow_y.push_back(y);
    for (long i = 1; i < m; ++i) {
        MPFloat last = pow_y.back();
        pow_y.push_back(last * last);
    }
    // Reverse traversal: highest power first.
    long n = (x >= MPFloat(1)) ? 0 : -1;
    MPFloat r = (x >= MPFloat(1)) ? x : (x * tmp);
    for (auto it = pow_y.rbegin(); it != pow_y.rend(); ++it) {
        n <<= 1;
        const MPFloat& p = *it;
        if (r > p) {
            r /= p;
            n += 1;
        }
    }
    return {n, r};
}

std::optional<std::pair<MPFloat, MPFloat>> solve_quadratic(const MPFloat& a_in,
                                                           const MPFloat& b_in,
                                                           const MPFloat& c_in) {
    MPFloat a = a_in, b = b_in, c = c_in;
    if (a < 0) {
        a = -a;
        b = -b;
        c = -c;
    }
    MPFloat disc = b * b - MPFloat(4) * a * c;
    if (disc < 0) return std::nullopt;

    MPFloat r = sqrt(disc);
    MPFloat s1 = -b - r;
    MPFloat s2 = -b + r;
    MPFloat two_a = MPFloat(2) * a;

    if (b >= 0) {
        return std::make_pair(s1 / two_a, s2 / two_a);
    } else {
        if (c.is_zero()) {
            return std::make_pair(MPFloat(0), -b / a);
        }
        return std::make_pair((MPFloat(2) * c) / s2, (MPFloat(2) * c) / s1);
    }
}

}  // namespace cppgridsynth
