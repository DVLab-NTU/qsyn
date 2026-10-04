// SPDX-License-Identifier: MIT
//
// MPFloat: a thin C++ wrapper around `mpfr_t` providing the subset of
// mpmath.mpf semantics that pygridsynth relies on.  The default rounding
// mode is MPFR_RNDN (round to nearest, ties to even), which matches mpmath.
//
// All operations operate at the working precision `MPFloat::working_prec`
// (in bits).  This mirrors mpmath's `mpmath.mp.dps` (decimal places); a
// helper `set_working_dps` converts decimal places into the appropriate
// number of bits.
#pragma once

#include <gmp.h>
#include <gmpxx.h>
#include <mpfr.h>

#include <cstdint>
#include <iosfwd>
#include <string>
#include <utility>

namespace cppgridsynth {

class MPFloat {
public:
    // ------------------------------------------------------------------
    // Working precision controls
    // ------------------------------------------------------------------
    // Number of bits used by newly created MPFloat objects.  Defaults to
    // the equivalent of mpmath's default 53 bits (~15.95 decimal digits).
    static mpfr_prec_t working_prec();
    static void set_working_prec(mpfr_prec_t prec_bits);
    static void set_working_dps(int decimal_places);
    static int working_dps();

    // ------------------------------------------------------------------
    // Construction / destruction
    // ------------------------------------------------------------------
    MPFloat();
    MPFloat(int v);
    MPFloat(long v);
    MPFloat(unsigned long v);
    MPFloat(double v);
    MPFloat(const mpz_class& v);
    explicit MPFloat(const std::string& s, int base = 10);
    MPFloat(const char* s, int base = 10);

    MPFloat(const MPFloat& other);
    MPFloat(MPFloat&& other) noexcept;
    MPFloat& operator=(const MPFloat& other);
    MPFloat& operator=(MPFloat&& other) noexcept;
    ~MPFloat();

    // ------------------------------------------------------------------
    // Raw access
    // ------------------------------------------------------------------
    mpfr_ptr raw() noexcept { return v_; }
    mpfr_srcptr raw() const noexcept { return v_; }
    mpfr_prec_t prec() const noexcept { return mpfr_get_prec(v_); }

    // ------------------------------------------------------------------
    // Conversions
    // ------------------------------------------------------------------
    double to_double() const noexcept;
    long to_long() const noexcept;
    mpz_class to_mpz_floor() const;        // floor towards -infinity
    mpz_class to_mpz_ceil() const;         // ceil  towards +infinity
    mpz_class to_mpz_round() const;        // round to nearest (banker's)
    std::string to_string(int digits = 0) const;

    // ------------------------------------------------------------------
    // Compound assignment
    // ------------------------------------------------------------------
    MPFloat& operator+=(const MPFloat& rhs);
    MPFloat& operator-=(const MPFloat& rhs);
    MPFloat& operator*=(const MPFloat& rhs);
    MPFloat& operator/=(const MPFloat& rhs);

    MPFloat operator-() const;

    // ------------------------------------------------------------------
    // Predicates / sign
    // ------------------------------------------------------------------
    int sgn() const noexcept;
    bool is_zero() const noexcept;
    bool is_finite() const noexcept;

    // ------------------------------------------------------------------
    // Constants
    // ------------------------------------------------------------------
    static MPFloat zero();
    static MPFloat one();
    static MPFloat pi();
    static MPFloat sqrt2();

private:
    mpfr_t v_;
};

// ---- Free arithmetic operators ------------------------------------------
MPFloat operator+(const MPFloat& a, const MPFloat& b);
MPFloat operator-(const MPFloat& a, const MPFloat& b);
MPFloat operator*(const MPFloat& a, const MPFloat& b);
MPFloat operator/(const MPFloat& a, const MPFloat& b);

// Mixed integer arithmetic
MPFloat operator+(const MPFloat& a, long b);
MPFloat operator+(long a, const MPFloat& b);
MPFloat operator-(const MPFloat& a, long b);
MPFloat operator-(long a, const MPFloat& b);
MPFloat operator*(const MPFloat& a, long b);
MPFloat operator*(long a, const MPFloat& b);
MPFloat operator/(const MPFloat& a, long b);
MPFloat operator/(long a, const MPFloat& b);

MPFloat operator+(const MPFloat& a, const mpz_class& b);
MPFloat operator-(const MPFloat& a, const mpz_class& b);
MPFloat operator*(const MPFloat& a, const mpz_class& b);
MPFloat operator/(const MPFloat& a, const mpz_class& b);
MPFloat operator+(const mpz_class& a, const MPFloat& b);
MPFloat operator-(const mpz_class& a, const MPFloat& b);
MPFloat operator*(const mpz_class& a, const MPFloat& b);
MPFloat operator/(const mpz_class& a, const MPFloat& b);

// ---- Comparison operators ------------------------------------------------
bool operator==(const MPFloat& a, const MPFloat& b);
bool operator!=(const MPFloat& a, const MPFloat& b);
bool operator<(const MPFloat& a, const MPFloat& b);
bool operator<=(const MPFloat& a, const MPFloat& b);
bool operator>(const MPFloat& a, const MPFloat& b);
bool operator>=(const MPFloat& a, const MPFloat& b);

bool operator<(const MPFloat& a, long b);
bool operator<=(const MPFloat& a, long b);
bool operator>(const MPFloat& a, long b);
bool operator>=(const MPFloat& a, long b);
bool operator==(const MPFloat& a, long b);
bool operator!=(const MPFloat& a, long b);

// ---- Math functions ------------------------------------------------------
MPFloat abs(const MPFloat& x);
MPFloat sqrt(const MPFloat& x);
MPFloat log(const MPFloat& x);
MPFloat log10(const MPFloat& x);
MPFloat exp(const MPFloat& x);
MPFloat sin(const MPFloat& x);
MPFloat cos(const MPFloat& x);
MPFloat pow(const MPFloat& x, const MPFloat& y);
MPFloat pow(const MPFloat& x, long n);

mpz_class floor(const MPFloat& x);
mpz_class ceil(const MPFloat& x);

std::ostream& operator<<(std::ostream& os, const MPFloat& v);

// ---- Working precision RAII ---------------------------------------------
class WorkPrec {
public:
    explicit WorkPrec(mpfr_prec_t bits)
        : saved_(MPFloat::working_prec()) {
        MPFloat::set_working_prec(bits);
    }
    explicit WorkPrec(int decimal_places, int /*tag*/)
        : saved_(MPFloat::working_prec()) {
        MPFloat::set_working_dps(decimal_places);
    }
    ~WorkPrec() { MPFloat::set_working_prec(saved_); }

    WorkPrec(const WorkPrec&) = delete;
    WorkPrec& operator=(const WorkPrec&) = delete;

private:
    mpfr_prec_t saved_;
};

// Convenience: create a WorkPrec from decimal-places precision (mpmath.mp.dps).
inline WorkPrec work_dps(int decimal_places) {
    return WorkPrec(decimal_places, 0);
}

}  // namespace cppgridsynth
