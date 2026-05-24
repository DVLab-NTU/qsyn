// SPDX-License-Identifier: MIT
//
// Algebraic-ring data structures used by gridsynth.
//
//   ZRootTwo   :=  Z[√2]            elements:  a + b√2     (a, b ∈ Z)
//   DRootTwo   :=  D[√2]            elements:  ZRootTwo / (√2)^k    (k ∈ N)
//   ZOmega     :=  Z[ω]             ω = e^(iπ/4); elements: aω³ + bω² + cω + d
//   DOmega     :=  D[ω]             elements:  ZOmega / (√2)^k       (k ∈ N)
//
// Coefficients use GMP arbitrary-precision integers (`mpz_class`) so that
// none of the algorithm's intermediate magnitudes overflow.  All real /
// complex evaluations go through `MPFloat`.
#pragma once

#include <gmpxx.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <tuple>
#include <utility>

#include "cppgridsynth/mpfloat.hpp"

namespace cppgridsynth {

class ZRootTwo;
class DRootTwo;
class ZOmega;
class DOmega;

// =========================================================================
//  Z[√2]  --  integers in the ring Z[√2].  Element  =  a + b·√2.
// =========================================================================
class ZRootTwo {
public:
    ZRootTwo() : a_(0), b_(0) {}
    ZRootTwo(long a, long b) : a_(a), b_(b) {}
    ZRootTwo(const mpz_class& a, const mpz_class& b) : a_(a), b_(b) {}

    static ZRootTwo from_int(const mpz_class& v) { return ZRootTwo(v, mpz_class(0)); }
    static ZRootTwo from_int(long v) { return ZRootTwo(v, 0); }
    static ZRootTwo from_zomega(const ZOmega& v);

    const mpz_class& a() const noexcept { return a_; }
    const mpz_class& b() const noexcept { return b_; }

    bool operator==(const ZRootTwo& o) const noexcept { return a_ == o.a_ && b_ == o.b_; }
    bool operator!=(const ZRootTwo& o) const noexcept { return !(*this == o); }
    bool operator==(long o) const noexcept { return a_ == o && b_ == 0; }
    bool operator!=(long o) const noexcept { return !(*this == o); }

    bool operator<(const ZRootTwo& o) const;
    bool operator<=(const ZRootTwo& o) const { return *this == o || *this < o; }
    bool operator>(const ZRootTwo& o) const { return o < *this; }
    bool operator>=(const ZRootTwo& o) const { return o <= *this; }
    bool operator<(long o) const { return *this < ZRootTwo::from_int(o); }
    bool operator<=(long o) const { return *this <= ZRootTwo::from_int(o); }
    bool operator>(long o) const { return *this > ZRootTwo::from_int(o); }
    bool operator>=(long o) const { return *this >= ZRootTwo::from_int(o); }

    ZRootTwo operator-() const { return ZRootTwo(-a_, -b_); }

    ZRootTwo& operator+=(const ZRootTwo& o) { a_ += o.a_; b_ += o.b_; return *this; }
    ZRootTwo& operator-=(const ZRootTwo& o) { a_ -= o.a_; b_ -= o.b_; return *this; }
    ZRootTwo& operator*=(const ZRootTwo& o) { *this = *this * o; return *this; }

    friend ZRootTwo operator+(const ZRootTwo& l, const ZRootTwo& r) { return ZRootTwo(l.a_ + r.a_, l.b_ + r.b_); }
    friend ZRootTwo operator-(const ZRootTwo& l, const ZRootTwo& r) { return ZRootTwo(l.a_ - r.a_, l.b_ - r.b_); }
    friend ZRootTwo operator*(const ZRootTwo& l, const ZRootTwo& r) {
        return ZRootTwo(l.a_ * r.a_ + 2 * l.b_ * r.b_,
                        l.a_ * r.b_ + l.b_ * r.a_);
    }
    friend ZRootTwo operator+(const ZRootTwo& l, long r) { return l + ZRootTwo::from_int(r); }
    friend ZRootTwo operator-(const ZRootTwo& l, long r) { return l - ZRootTwo::from_int(r); }
    friend ZRootTwo operator*(const ZRootTwo& l, long r) { return l * ZRootTwo::from_int(r); }
    friend ZRootTwo operator+(long l, const ZRootTwo& r) { return ZRootTwo::from_int(l) + r; }
    friend ZRootTwo operator-(long l, const ZRootTwo& r) { return ZRootTwo::from_int(l) - r; }
    friend ZRootTwo operator*(long l, const ZRootTwo& r) { return ZRootTwo::from_int(l) * r; }

    ZRootTwo pow(unsigned long n) const;
    ZRootTwo pow(long n) const;  // integer exponent, may be negative

    // Floor / Euclidean division for integer-quotient algorithms.
    static std::pair<ZRootTwo, ZRootTwo> divmod(const ZRootTwo& a, const ZRootTwo& b);
    ZRootTwo floordiv(const ZRootTwo& o) const { return divmod(*this, o).first; }
    ZRootTwo mod(const ZRootTwo& o) const { return divmod(*this, o).second; }

    // gcd / extended gcd (Euclidean ring ZRootTwo).
    static ZRootTwo gcd(ZRootTwo a, ZRootTwo b);
    // Returns (x, z, g) with x*a0 + z*b0 = g.
    static std::tuple<ZRootTwo, ZRootTwo, ZRootTwo> ext_gcd(ZRootTwo a, ZRootTwo b);

    // True iff a | b and b | a in Z[√2].
    static bool sim(const ZRootTwo& a, const ZRootTwo& b);

    // Reciprocal in the units of Z[√2] (only for unit elements).
    ZRootTwo inv() const;

    // Square-root in Z[√2], if it exists (returns std::nullopt otherwise).
    std::optional<ZRootTwo> sqrt() const;

    // Norm:  N(a + b√2) = a² - 2b².
    mpz_class norm() const { return a_ * a_ - 2 * b_ * b_; }

    // Galois conjugate w.r.t. √2: replaces b with -b.
    ZRootTwo conj_sq2() const { return ZRootTwo(a_, -b_); }

    // Parity = a mod 2.
    int parity() const { return mpz_odd_p(a_.get_mpz_t()) ? 1 : 0; }

    // Real-valued evaluation a + b√2.
    MPFloat to_real() const;

    std::string to_string() const;

private:
    mpz_class a_;
    mpz_class b_;
};

// =========================================================================
//  D[√2]  --  dyadic fractions over Z[√2].  Element = α / (√2)^k.
// =========================================================================
class DRootTwo {
public:
    DRootTwo() : alpha_(), k_(0) {}
    DRootTwo(ZRootTwo alpha, long k) : alpha_(std::move(alpha)), k_(k) {}

    static DRootTwo from_int(long v) { return DRootTwo(ZRootTwo::from_int(v), 0); }
    static DRootTwo from_int(const mpz_class& v) { return DRootTwo(ZRootTwo::from_int(v), 0); }
    static DRootTwo from_zroottwo(ZRootTwo v) { return DRootTwo(std::move(v), 0); }
    static DRootTwo from_zomega(const ZOmega& v);
    static DRootTwo from_domega(const DOmega& v);
    static DRootTwo power_of_inv_sqrt2(long k) { return DRootTwo(ZRootTwo(1, 0), k); }

    const ZRootTwo& alpha() const noexcept { return alpha_; }
    long k() const noexcept { return k_; }

    bool operator==(const DRootTwo& o) const;
    bool operator!=(const DRootTwo& o) const { return !(*this == o); }
    bool operator==(long o) const { return *this == DRootTwo::from_int(o); }
    bool operator!=(long o) const { return !(*this == o); }
    bool operator==(const ZRootTwo& o) const { return *this == DRootTwo::from_zroottwo(o); }
    bool operator<(const DRootTwo& o) const;
    bool operator<=(const DRootTwo& o) const { return *this == o || *this < o; }
    bool operator>(const DRootTwo& o) const { return o < *this; }
    bool operator>=(const DRootTwo& o) const { return o <= *this; }
    bool operator<=(const ZRootTwo& o) const { return *this <= DRootTwo::from_zroottwo(o); }

    DRootTwo operator-() const { return DRootTwo(-alpha_, k_); }

    friend DRootTwo operator+(const DRootTwo& l, const DRootTwo& r);
    friend DRootTwo operator-(const DRootTwo& l, const DRootTwo& r);
    friend DRootTwo operator*(const DRootTwo& l, const DRootTwo& r) {
        return DRootTwo(l.alpha_ * r.alpha_, l.k_ + r.k_);
    }
    friend DRootTwo operator+(const DRootTwo& l, long r) { return l + DRootTwo::from_int(r); }
    friend DRootTwo operator-(const DRootTwo& l, long r) { return l - DRootTwo::from_int(r); }
    friend DRootTwo operator*(const DRootTwo& l, long r) { return l * DRootTwo::from_int(r); }
    friend DRootTwo operator+(long l, const DRootTwo& r) { return DRootTwo::from_int(l) + r; }
    friend DRootTwo operator-(long l, const DRootTwo& r) { return DRootTwo::from_int(l) - r; }
    friend DRootTwo operator*(long l, const DRootTwo& r) { return DRootTwo::from_int(l) * r; }
    friend DRootTwo operator+(const DRootTwo& l, const ZRootTwo& r) { return l + DRootTwo::from_zroottwo(r); }
    friend DRootTwo operator-(const DRootTwo& l, const ZRootTwo& r) { return l - DRootTwo::from_zroottwo(r); }

    // Adjust the (√2)^k denominator without changing the value.
    DRootTwo renew_denomexp(long new_k) const;
    DRootTwo reduce_denomexp() const;
    DRootTwo mul_by_inv_sqrt2() const;
    DRootTwo mul_by_sqrt2_power(long d) const;
    DRootTwo mul_by_sqrt2_power_renewing_denomexp(long d) const;

    int parity() const { return alpha_.parity(); }
    DRootTwo conj_sq2() const;
    MPFloat scale() const;  // sqrt(2)^k
    mpz_class squared_scale() const { return mpz_class(1) << static_cast<unsigned long>(k_); }

    MPFloat to_real() const;

    std::string to_string() const;

private:
    ZRootTwo alpha_;
    long k_;
};

// =========================================================================
//  Z[ω]  --  integers in the cyclotomic ring Z[ω], ω = e^{iπ/4}.
//      Element = a·ω³ + b·ω² + c·ω + d.
// =========================================================================
class ZOmega {
public:
    ZOmega() : a_(0), b_(0), c_(0), d_(0) {}
    ZOmega(long a, long b, long c, long d) : a_(a), b_(b), c_(c), d_(d) {}
    ZOmega(const mpz_class& a, const mpz_class& b, const mpz_class& c, const mpz_class& d)
        : a_(a), b_(b), c_(c), d_(d) {}

    static ZOmega from_int(long v) { return ZOmega(0, 0, 0, v); }
    static ZOmega from_int(const mpz_class& v) { return ZOmega(0, 0, 0, v); }
    static ZOmega from_zroottwo(const ZRootTwo& v) { return ZOmega(-v.b(), 0, v.b(), v.a()); }

    const mpz_class& a() const noexcept { return a_; }
    const mpz_class& b() const noexcept { return b_; }
    const mpz_class& c() const noexcept { return c_; }
    const mpz_class& d() const noexcept { return d_; }

    // Coefficient list ordered low→high index: [d, c, b, a]  (matches Python).
    std::array<mpz_class, 4> coef() const { return {d_, c_, b_, a_}; }

    bool operator==(const ZOmega& o) const noexcept {
        return a_ == o.a_ && b_ == o.b_ && c_ == o.c_ && d_ == o.d_;
    }
    bool operator!=(const ZOmega& o) const noexcept { return !(*this == o); }
    bool operator==(long o) const { return *this == ZOmega::from_int(o); }
    bool operator!=(long o) const { return !(*this == o); }

    ZOmega operator-() const { return ZOmega(-a_, -b_, -c_, -d_); }

    friend ZOmega operator+(const ZOmega& l, const ZOmega& r) {
        return ZOmega(l.a_ + r.a_, l.b_ + r.b_, l.c_ + r.c_, l.d_ + r.d_);
    }
    friend ZOmega operator-(const ZOmega& l, const ZOmega& r) {
        return ZOmega(l.a_ - r.a_, l.b_ - r.b_, l.c_ - r.c_, l.d_ - r.d_);
    }
    friend ZOmega operator*(const ZOmega& l, const ZOmega& r);
    friend ZOmega operator+(const ZOmega& l, long r) { return l + ZOmega::from_int(r); }
    friend ZOmega operator-(const ZOmega& l, long r) { return l - ZOmega::from_int(r); }
    friend ZOmega operator*(const ZOmega& l, long r) { return l * ZOmega::from_int(r); }
    friend ZOmega operator+(long l, const ZOmega& r) { return ZOmega::from_int(l) + r; }
    friend ZOmega operator-(long l, const ZOmega& r) { return ZOmega::from_int(l) - r; }
    friend ZOmega operator*(long l, const ZOmega& r) { return ZOmega::from_int(l) * r; }
    friend ZOmega operator+(const ZOmega& l, const mpz_class& r) { return l + ZOmega::from_int(r); }
    friend ZOmega operator+(const ZOmega& l, const ZRootTwo& r) { return l + ZOmega::from_zroottwo(r); }
    friend ZOmega operator-(const ZOmega& l, const ZRootTwo& r) { return l - ZOmega::from_zroottwo(r); }
    friend ZOmega operator*(const ZOmega& l, const ZRootTwo& r) { return l * ZOmega::from_zroottwo(r); }
    friend ZOmega operator*(const ZRootTwo& l, const ZOmega& r) { return ZOmega::from_zroottwo(l) * r; }

    ZOmega& operator+=(const ZOmega& o) { *this = *this + o; return *this; }
    ZOmega& operator-=(const ZOmega& o) { *this = *this - o; return *this; }
    ZOmega& operator*=(const ZOmega& o) { *this = *this * o; return *this; }

    ZOmega pow(unsigned long n) const;

    // Norm = (a²+b²+c²+d²)² - 2·(ab+bc+cd-da)².
    mpz_class norm() const;

    static std::pair<ZOmega, ZOmega> divmod(const ZOmega& a, const ZOmega& b);
    ZOmega floordiv(const ZOmega& o) const { return divmod(*this, o).first; }
    ZOmega mod(const ZOmega& o) const { return divmod(*this, o).second; }

    static ZOmega gcd(ZOmega a, ZOmega b);
    // Returns (x, z, g) with x*a0 + z*b0 = g.
    static std::tuple<ZOmega, ZOmega, ZOmega> ext_gcd(ZOmega a, ZOmega b);

    static bool sim(const ZOmega& a, const ZOmega& b);

    // Inverse only for unit elements (norm = 1).
    ZOmega inv() const;

    // Multiplications by ω (and powers).
    ZOmega mul_by_omega() const { return ZOmega(b_, c_, d_, -a_); }
    ZOmega mul_by_omega_inv() const { return ZOmega(-d_, a_, b_, c_); }
    ZOmega mul_by_omega_power(long n) const;

    // residue = (a&1)<<3 | (b&1)<<2 | (c&1)<<1 | (d&1).
    int residue() const;

    // ω = (1 + i)/√2 in C, so ω̄ = ω⁻¹ = -ω³, hence:
    //   conj    : a ↦ -c, b ↦ -b, c ↦ -a, d ↦ d
    //   conj_√2 : negate odd-degree (a, c) coefficients.
    ZOmega conj() const { return ZOmega(-c_, -b_, -a_, d_); }
    ZOmega conj_sq2() const { return ZOmega(-a_, b_, -c_, d_); }

    // Real / imaginary part as MPFloat.
    MPFloat real() const;
    MPFloat imag() const;
    std::pair<MPFloat, MPFloat> to_complex() const { return {real(), imag()}; }

    std::string to_string() const;

private:
    mpz_class a_, b_, c_, d_;
};

// =========================================================================
//  D[ω]  --  dyadic fractions over Z[ω].  Element = u / (√2)^k.
// =========================================================================
class DOmega {
public:
    DOmega() : u_(), k_(0) {}
    DOmega(ZOmega u, long k) : u_(std::move(u)), k_(k) {}

    static DOmega from_int(long v) { return DOmega(ZOmega::from_int(v), 0); }
    static DOmega from_int(const mpz_class& v) { return DOmega(ZOmega::from_int(v), 0); }
    static DOmega from_zomega(ZOmega v) { return DOmega(std::move(v), 0); }
    static DOmega from_zroottwo(const ZRootTwo& v) { return DOmega(ZOmega::from_zroottwo(v), 0); }
    static DOmega from_droottwo(const DRootTwo& v) { return DOmega(ZOmega::from_zroottwo(v.alpha()), v.k()); }
    // From two DRootTwo "x + y·i": creates (x + y·ω²)/(√2)^k after renew.
    static DOmega from_droottwo_vector(const DRootTwo& x, const DRootTwo& y, long k);

    const ZOmega& u() const noexcept { return u_; }
    long k() const noexcept { return k_; }

    bool operator==(const DOmega& o) const;
    bool operator!=(const DOmega& o) const { return !(*this == o); }
    bool operator==(long o) const { return *this == DOmega::from_int(o); }
    bool operator!=(long o) const { return !(*this == o); }

    DOmega operator-() const { return DOmega(-u_, k_); }

    friend DOmega operator+(const DOmega& l, const DOmega& r);
    friend DOmega operator-(const DOmega& l, const DOmega& r);
    friend DOmega operator*(const DOmega& l, const DOmega& r) {
        return DOmega(l.u_ * r.u_, l.k_ + r.k_);
    }
    friend DOmega operator+(const DOmega& l, long r) { return l + DOmega::from_int(r); }
    friend DOmega operator-(const DOmega& l, long r) { return l - DOmega::from_int(r); }
    friend DOmega operator*(const DOmega& l, long r) { return l * DOmega::from_int(r); }
    friend DOmega operator+(long l, const DOmega& r) { return DOmega::from_int(l) + r; }
    friend DOmega operator-(long l, const DOmega& r) { return DOmega::from_int(l) - r; }
    friend DOmega operator*(long l, const DOmega& r) { return DOmega::from_int(l) * r; }
    friend DOmega operator*(const ZOmega& l, const DOmega& r) { return DOmega::from_zomega(l) * r; }
    friend DOmega operator*(const DOmega& l, const ZOmega& r) { return l * DOmega::from_zomega(r); }

    DOmega renew_denomexp(long new_k) const;
    DOmega reduce_denomexp() const;
    DOmega mul_by_inv_sqrt2() const;
    DOmega mul_by_sqrt2_power(long d) const;
    DOmega mul_by_omega() const { return DOmega(u_.mul_by_omega(), k_); }
    DOmega mul_by_omega_inv() const { return DOmega(u_.mul_by_omega_inv(), k_); }
    DOmega mul_by_omega_power(long n) const { return DOmega(u_.mul_by_omega_power(n), k_); }

    int residue() const { return u_.residue(); }
    MPFloat scale() const;  // sqrt(2)^k
    mpz_class squared_scale() const { return mpz_class(1) << static_cast<unsigned long>(k_); }

    MPFloat real() const;
    MPFloat imag() const;

    DOmega conj() const { return DOmega(u_.conj(), k_); }
    DOmega conj_sq2() const;

    std::string to_string() const;

private:
    ZOmega u_;
    long k_;
};

// Convenient global constants used throughout the algorithm.
const ZRootTwo& LAMBDA();
const ZOmega&   OMEGA();
const std::array<ZOmega, 8>& OMEGA_POWER();

// I/O ---------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const ZRootTwo& v);
std::ostream& operator<<(std::ostream& os, const DRootTwo& v);
std::ostream& operator<<(std::ostream& os, const ZOmega& v);
std::ostream& operator<<(std::ostream& os, const DOmega& v);

}  // namespace cppgridsynth
