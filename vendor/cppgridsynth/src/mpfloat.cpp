// SPDX-License-Identifier: MIT
#include "cppgridsynth/mpfloat.hpp"

#include <cmath>
#include <cstdio>
#include <ostream>
#include <vector>

namespace cppgridsynth {

namespace {

constexpr mpfr_rnd_t RND = MPFR_RNDN;

// Convert decimal places (mpmath.mp.dps) to MPFR bit precision.
// mpmath uses the same convention as MPFR for the conversion.
inline mpfr_prec_t dps_to_bits(int dps) {
    if (dps < 1) dps = 1;
    // Each decimal digit is roughly log2(10) ≈ 3.3219... bits.
    // We add a small guard so that at least one extra bit is always kept.
    return static_cast<mpfr_prec_t>(static_cast<long>(dps * 3.3219280948873626) + 2);
}

inline int bits_to_dps(mpfr_prec_t bits) {
    if (bits < 1) bits = 1;
    return static_cast<int>(static_cast<double>(bits) / 3.3219280948873626);
}

// Thread-local working precision (in bits).
mpfr_prec_t& working_prec_ref() {
    static thread_local mpfr_prec_t prec_bits = 53;
    return prec_bits;
}

}  // namespace

mpfr_prec_t MPFloat::working_prec() { return working_prec_ref(); }

void MPFloat::set_working_prec(mpfr_prec_t prec_bits) {
    if (prec_bits < MPFR_PREC_MIN) prec_bits = MPFR_PREC_MIN;
    working_prec_ref() = prec_bits;
}

void MPFloat::set_working_dps(int decimal_places) {
    set_working_prec(dps_to_bits(decimal_places));
}

int MPFloat::working_dps() { return bits_to_dps(working_prec_ref()); }

// ---- ctors --------------------------------------------------------------
MPFloat::MPFloat() {
    mpfr_init2(v_, working_prec_ref());
    mpfr_set_zero(v_, +1);
}

MPFloat::MPFloat(int v) {
    mpfr_init2(v_, working_prec_ref());
    mpfr_set_si(v_, v, RND);
}

MPFloat::MPFloat(long v) {
    mpfr_init2(v_, working_prec_ref());
    mpfr_set_si(v_, v, RND);
}

MPFloat::MPFloat(unsigned long v) {
    mpfr_init2(v_, working_prec_ref());
    mpfr_set_ui(v_, v, RND);
}

MPFloat::MPFloat(double v) {
    mpfr_init2(v_, working_prec_ref());
    mpfr_set_d(v_, v, RND);
}

MPFloat::MPFloat(const mpz_class& v) {
    mpfr_init2(v_, working_prec_ref());
    mpfr_set_z(v_, v.get_mpz_t(), RND);
}

MPFloat::MPFloat(const std::string& s, int base) {
    mpfr_init2(v_, working_prec_ref());
    if (mpfr_set_str(v_, s.c_str(), base, RND) != 0) {
        mpfr_set_zero(v_, +1);
    }
}

MPFloat::MPFloat(const char* s, int base) {
    mpfr_init2(v_, working_prec_ref());
    if (mpfr_set_str(v_, s, base, RND) != 0) {
        mpfr_set_zero(v_, +1);
    }
}

MPFloat::MPFloat(const MPFloat& other) {
    mpfr_init2(v_, working_prec_ref());
    mpfr_set(v_, other.v_, RND);
}

MPFloat::MPFloat(MPFloat&& other) noexcept {
    // Move via swap with an initialised zero.
    mpfr_init2(v_, working_prec_ref());
    mpfr_swap(v_, other.v_);
}

MPFloat& MPFloat::operator=(const MPFloat& other) {
    if (this != &other) {
        mpfr_set_prec(v_, working_prec_ref());
        mpfr_set(v_, other.v_, RND);
    }
    return *this;
}

MPFloat& MPFloat::operator=(MPFloat&& other) noexcept {
    if (this != &other) {
        mpfr_swap(v_, other.v_);
    }
    return *this;
}

MPFloat::~MPFloat() { mpfr_clear(v_); }

// ---- conversions --------------------------------------------------------
double MPFloat::to_double() const noexcept { return mpfr_get_d(v_, RND); }

long MPFloat::to_long() const noexcept { return mpfr_get_si(v_, RND); }

mpz_class MPFloat::to_mpz_floor() const {
    MPFloat tmp;
    mpfr_floor(tmp.v_, v_);
    mpz_class out;
    mpfr_get_z(out.get_mpz_t(), tmp.v_, MPFR_RNDD);
    return out;
}

mpz_class MPFloat::to_mpz_ceil() const {
    MPFloat tmp;
    mpfr_ceil(tmp.v_, v_);
    mpz_class out;
    mpfr_get_z(out.get_mpz_t(), tmp.v_, MPFR_RNDU);
    return out;
}

mpz_class MPFloat::to_mpz_round() const {
    mpz_class out;
    mpfr_get_z(out.get_mpz_t(), v_, MPFR_RNDN);
    return out;
}

std::string MPFloat::to_string(int digits) const {
    if (digits <= 0) digits = working_dps();
    mpfr_exp_t exp = 0;
    char* raw = mpfr_get_str(nullptr, &exp, 10, static_cast<size_t>(digits), v_, RND);
    if (!raw) return "nan";
    std::string s(raw);
    mpfr_free_str(raw);

    // Format to a fairly readable decimal representation.
    std::string out;
    bool neg = !s.empty() && s.front() == '-';
    std::string digits_str = neg ? s.substr(1) : s;
    if (mpfr_zero_p(v_)) return neg ? "-0" : "0";

    if (neg) out.push_back('-');
    if (exp <= 0) {
        out += "0.";
        for (mpfr_exp_t i = 0; i < -exp; ++i) out.push_back('0');
        out += digits_str;
    } else if (static_cast<size_t>(exp) >= digits_str.size()) {
        out += digits_str;
        for (size_t i = digits_str.size(); i < static_cast<size_t>(exp); ++i)
            out.push_back('0');
    } else {
        out += digits_str.substr(0, exp);
        out.push_back('.');
        out += digits_str.substr(exp);
    }
    return out;
}

// ---- compound assignment ------------------------------------------------
MPFloat& MPFloat::operator+=(const MPFloat& rhs) {
    mpfr_add(v_, v_, rhs.v_, RND);
    return *this;
}
MPFloat& MPFloat::operator-=(const MPFloat& rhs) {
    mpfr_sub(v_, v_, rhs.v_, RND);
    return *this;
}
MPFloat& MPFloat::operator*=(const MPFloat& rhs) {
    mpfr_mul(v_, v_, rhs.v_, RND);
    return *this;
}
MPFloat& MPFloat::operator/=(const MPFloat& rhs) {
    mpfr_div(v_, v_, rhs.v_, RND);
    return *this;
}

MPFloat MPFloat::operator-() const {
    MPFloat out;
    mpfr_neg(out.v_, v_, RND);
    return out;
}

int MPFloat::sgn() const noexcept { return mpfr_sgn(v_); }
bool MPFloat::is_zero() const noexcept { return mpfr_zero_p(v_) != 0; }
bool MPFloat::is_finite() const noexcept { return mpfr_number_p(v_) != 0; }

// ---- constants ----------------------------------------------------------
MPFloat MPFloat::zero() { return MPFloat(); }
MPFloat MPFloat::one() { return MPFloat(1); }

MPFloat MPFloat::pi() {
    MPFloat out;
    mpfr_const_pi(out.v_, RND);
    return out;
}

MPFloat MPFloat::sqrt2() {
    MPFloat out(2);
    mpfr_sqrt(out.v_, out.v_, RND);
    return out;
}

// ---- free operators -----------------------------------------------------
MPFloat operator+(const MPFloat& a, const MPFloat& b) {
    MPFloat out;
    mpfr_add(out.raw(), a.raw(), b.raw(), RND);
    return out;
}
MPFloat operator-(const MPFloat& a, const MPFloat& b) {
    MPFloat out;
    mpfr_sub(out.raw(), a.raw(), b.raw(), RND);
    return out;
}
MPFloat operator*(const MPFloat& a, const MPFloat& b) {
    MPFloat out;
    mpfr_mul(out.raw(), a.raw(), b.raw(), RND);
    return out;
}
MPFloat operator/(const MPFloat& a, const MPFloat& b) {
    MPFloat out;
    mpfr_div(out.raw(), a.raw(), b.raw(), RND);
    return out;
}

MPFloat operator+(const MPFloat& a, long b) {
    MPFloat out;
    mpfr_add_si(out.raw(), a.raw(), b, RND);
    return out;
}
MPFloat operator+(long a, const MPFloat& b) { return b + a; }
MPFloat operator-(const MPFloat& a, long b) {
    MPFloat out;
    mpfr_sub_si(out.raw(), a.raw(), b, RND);
    return out;
}
MPFloat operator-(long a, const MPFloat& b) {
    MPFloat out;
    mpfr_si_sub(out.raw(), a, b.raw(), RND);
    return out;
}
MPFloat operator*(const MPFloat& a, long b) {
    MPFloat out;
    mpfr_mul_si(out.raw(), a.raw(), b, RND);
    return out;
}
MPFloat operator*(long a, const MPFloat& b) { return b * a; }
MPFloat operator/(const MPFloat& a, long b) {
    MPFloat out;
    mpfr_div_si(out.raw(), a.raw(), b, RND);
    return out;
}
MPFloat operator/(long a, const MPFloat& b) {
    MPFloat out;
    mpfr_si_div(out.raw(), a, b.raw(), RND);
    return out;
}

MPFloat operator+(const MPFloat& a, const mpz_class& b) {
    MPFloat out;
    mpfr_add_z(out.raw(), a.raw(), b.get_mpz_t(), RND);
    return out;
}
MPFloat operator-(const MPFloat& a, const mpz_class& b) {
    MPFloat out;
    mpfr_sub_z(out.raw(), a.raw(), b.get_mpz_t(), RND);
    return out;
}
MPFloat operator*(const MPFloat& a, const mpz_class& b) {
    MPFloat out;
    mpfr_mul_z(out.raw(), a.raw(), b.get_mpz_t(), RND);
    return out;
}
MPFloat operator/(const MPFloat& a, const mpz_class& b) {
    MPFloat out;
    mpfr_div_z(out.raw(), a.raw(), b.get_mpz_t(), RND);
    return out;
}
MPFloat operator+(const mpz_class& a, const MPFloat& b) { return b + a; }
MPFloat operator-(const mpz_class& a, const MPFloat& b) { return -(b - a); }
MPFloat operator*(const mpz_class& a, const MPFloat& b) { return b * a; }
MPFloat operator/(const mpz_class& a, const MPFloat& b) {
    return MPFloat(a) / b;
}

// ---- comparisons --------------------------------------------------------
bool operator==(const MPFloat& a, const MPFloat& b) { return mpfr_cmp(a.raw(), b.raw()) == 0; }
bool operator!=(const MPFloat& a, const MPFloat& b) { return mpfr_cmp(a.raw(), b.raw()) != 0; }
bool operator<(const MPFloat& a, const MPFloat& b) { return mpfr_cmp(a.raw(), b.raw()) < 0; }
bool operator<=(const MPFloat& a, const MPFloat& b) { return mpfr_cmp(a.raw(), b.raw()) <= 0; }
bool operator>(const MPFloat& a, const MPFloat& b) { return mpfr_cmp(a.raw(), b.raw()) > 0; }
bool operator>=(const MPFloat& a, const MPFloat& b) { return mpfr_cmp(a.raw(), b.raw()) >= 0; }

bool operator<(const MPFloat& a, long b) { return mpfr_cmp_si(a.raw(), b) < 0; }
bool operator<=(const MPFloat& a, long b) { return mpfr_cmp_si(a.raw(), b) <= 0; }
bool operator>(const MPFloat& a, long b) { return mpfr_cmp_si(a.raw(), b) > 0; }
bool operator>=(const MPFloat& a, long b) { return mpfr_cmp_si(a.raw(), b) >= 0; }
bool operator==(const MPFloat& a, long b) { return mpfr_cmp_si(a.raw(), b) == 0; }
bool operator!=(const MPFloat& a, long b) { return mpfr_cmp_si(a.raw(), b) != 0; }

// ---- math ---------------------------------------------------------------
MPFloat abs(const MPFloat& x) {
    MPFloat out;
    mpfr_abs(out.raw(), x.raw(), RND);
    return out;
}
MPFloat sqrt(const MPFloat& x) {
    MPFloat out;
    mpfr_sqrt(out.raw(), x.raw(), RND);
    return out;
}
MPFloat log(const MPFloat& x) {
    MPFloat out;
    mpfr_log(out.raw(), x.raw(), RND);
    return out;
}
MPFloat log10(const MPFloat& x) {
    MPFloat out;
    mpfr_log10(out.raw(), x.raw(), RND);
    return out;
}
MPFloat exp(const MPFloat& x) {
    MPFloat out;
    mpfr_exp(out.raw(), x.raw(), RND);
    return out;
}
MPFloat sin(const MPFloat& x) {
    MPFloat out;
    mpfr_sin(out.raw(), x.raw(), RND);
    return out;
}
MPFloat cos(const MPFloat& x) {
    MPFloat out;
    mpfr_cos(out.raw(), x.raw(), RND);
    return out;
}
MPFloat pow(const MPFloat& x, const MPFloat& y) {
    MPFloat out;
    mpfr_pow(out.raw(), x.raw(), y.raw(), RND);
    return out;
}
MPFloat pow(const MPFloat& x, long n) {
    MPFloat out;
    mpfr_pow_si(out.raw(), x.raw(), n, RND);
    return out;
}

mpz_class floor(const MPFloat& x) { return x.to_mpz_floor(); }
mpz_class ceil(const MPFloat& x) { return x.to_mpz_ceil(); }

std::ostream& operator<<(std::ostream& os, const MPFloat& v) {
    os << v.to_string();
    return os;
}

}  // namespace cppgridsynth
