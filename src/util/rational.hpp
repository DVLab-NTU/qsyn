/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Definition of the Rational Number class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>

#include <cassert>
#include <cmath>
#include <concepts>
#include <gsl/util>
#include <iosfwd>
#include <limits>
#include <numeric>

//--- Rational Numbers ----------------------------------
// This class maintains the canonicity of stored rational numbers by simplifying the numerator/denominator whenever possible.
// Rational numbers are not the same as fractions! This class does not support nested fractions or irrational numbers in numerator/denominator.
// This class implicitly convert floating points to rational approximation when performing arithmetic operations.
//--- Rational Numbers ----------------------------------

namespace dvlab {

class Rational {
public:
    // Default constructor for two integral type
    using IntegralType      = int;
    using FloatingPointType = double;

    constexpr Rational() {}
    constexpr Rational(IntegralType n) : _numer(n) {}
    constexpr Rational(IntegralType n, IntegralType d) : _numer(n), _denom(d) {
        assert(d != 0);
        reduce();
    }
    // Implicitly use 1 as denominator
    template <class T>
    requires std::floating_point<T>
    Rational(T f, T eps = 1e-4) {
        *this = Rational::to_rational(f, eps);
    }

    // Operator Overloading
    friend std::ostream& operator<<(std::ostream& os, Rational const& q);

    constexpr Rational operator+() const;
    constexpr Rational operator-() const;

    // Arithmetic operators always preserve the normalities of Rational
    constexpr Rational& operator+=(Rational const& rhs);
    constexpr Rational& operator-=(Rational const& rhs);
    constexpr Rational& operator*=(Rational const& rhs);
    constexpr Rational& operator/=(Rational const& rhs);
    friend constexpr Rational operator+(Rational lhs, Rational const& rhs);
    friend constexpr Rational operator-(Rational lhs, Rational const& rhs);
    friend constexpr Rational operator*(Rational lhs, Rational const& rhs);
    friend constexpr Rational operator/(Rational lhs, Rational const& rhs);

    constexpr bool operator==(Rational const& rhs) const;
    constexpr bool operator!=(Rational const& rhs) const;
    constexpr bool operator<(Rational const& rhs) const;
    constexpr bool operator<=(Rational const& rhs) const;
    constexpr bool operator>(Rational const& rhs) const;
    constexpr bool operator>=(Rational const& rhs) const;

    // Operations for Rational Numbers
    constexpr void reduce();
    //                                      vvv won't lose precision if the following static_assert is satisfied
    constexpr IntegralType numerator() const { return static_cast<IntegralType>(_numer); }
    constexpr IntegralType denominator() const { return static_cast<IntegralType>(_denom); }

    static_assert(
        std::numeric_limits<IntegralType>::digits <= std::numeric_limits<FloatingPointType>::digits,
        "IntegralType must have at least as many digits as FloatingPointType");

    template <class T>
    requires std::floating_point<T>
    constexpr static T rational_to_floating_point(Rational const& q) { return ((T)q._numer) / q._denom; }

    constexpr static float rational_to_f(Rational const& q) { return rational_to_floating_point<float>(q); }
    constexpr static double rational_to_d(Rational const& q) { return rational_to_floating_point<double>(q); }
    constexpr static long double rational_to_ld(Rational const& q) { return rational_to_floating_point<long double>(q); }

    template <class T>
    requires std::floating_point<T>
    static Rational to_rational(T f, T eps = 1e-4);

private:
    double _numer = 0;
    double _denom = 1;
    static Rational _mediant(Rational const& lhs, Rational const& rhs);
};

template <class T>
requires std::floating_point<T>
Rational Rational::to_rational(T f, T eps) {
    IntegralType integral_part = gsl::narrow_cast<IntegralType>(floor(f));
    f -= integral_part;
    Rational lower(0, 1), upper(1, 1);
    Rational med(1, 2);

    auto in_lower_bound = [&f, &eps](Rational const& q) -> bool {
        return ((f - eps) <= rational_to_floating_point<T>(q));
    };
    auto in_upper_bound = [&f, &eps](Rational const& q) -> bool {
        return ((f + eps) >= rational_to_floating_point<T>(q));
    };

    if (in_lower_bound(lower) && in_upper_bound(lower)) {
        return lower + integral_part;
    }
    if (in_lower_bound(upper) && in_upper_bound(upper)) {
        return upper + integral_part;
    }

    while (true) {
        if (!in_lower_bound(med))
            lower = med;
        else if (!in_upper_bound(med))
            upper = med;
        else
            return med + integral_part;
        med = _mediant(lower, upper);
    }
}

constexpr void Rational::reduce() {
    if (_denom < 0) {
        _numer = -_numer;
        _denom = -_denom;
    }
    IntegralType const gcd = std::gcd(static_cast<IntegralType>(_numer), static_cast<IntegralType>(_denom));
    _numer /= gcd;
    _denom /= gcd;
}

//----------------------------------------
// Operator Overloading
//----------------------------------------

constexpr Rational Rational::operator+() const {
    return Rational(static_cast<Rational::IntegralType>(_numer), static_cast<Rational::IntegralType>(_denom));
}
constexpr Rational Rational::operator-() const {
    return Rational(-static_cast<Rational::IntegralType>(_numer), static_cast<Rational::IntegralType>(_denom));
}

// a/b + c/d  = (ad + bc) / bd
// We adopt for this more complex expression (instead of (ad + bc / bd)) so as to minimize the risk of overflow when multiplying numbers
constexpr Rational& Rational::operator+=(Rational const& rhs) {
    _numer = _numer * rhs._denom + _denom * rhs._numer;
    _denom = _denom * rhs._denom;
    assert(_denom != 0);
    reduce();
    return *this;
}
constexpr Rational& Rational::operator-=(Rational const& rhs) {
    _numer = _numer * rhs._denom - _denom * rhs._numer;
    _denom = _denom * rhs._denom;
    assert(_denom != 0);
    reduce();
    return *this;
}
constexpr Rational& Rational::operator*=(Rational const& rhs) {
    _numer *= rhs._numer;
    _denom *= rhs._denom;
    assert(_denom != 0);
    reduce();
    return *this;
}
constexpr Rational& Rational::operator/=(Rational const& rhs) {
    if (rhs._numer == 0) {
        throw std::overflow_error("Attempting to divide by 0");
    }
    _numer *= rhs._denom;
    _denom *= rhs._numer;
    assert(_denom != 0);
    reduce();
    return *this;
}
constexpr Rational operator+(Rational lhs, Rational const& rhs) {
    lhs += rhs;
    return lhs;
}
constexpr Rational operator-(Rational lhs, Rational const& rhs) {
    lhs -= rhs;
    return lhs;
}
constexpr Rational operator*(Rational lhs, Rational const& rhs) {
    lhs *= rhs;
    return lhs;
}
constexpr Rational operator/(Rational lhs, Rational const& rhs) {
    lhs /= rhs;
    return lhs;
}

constexpr bool Rational::operator==(Rational const& rhs) const {
    return (_numer == rhs._numer) && (_denom == rhs._denom);
}
constexpr bool Rational::operator!=(Rational const& rhs) const {
    return !(*this == rhs);
}
constexpr bool Rational::operator<(Rational const& rhs) const {
    return (_numer * rhs._denom) < (_denom * rhs._numer);
}
constexpr bool Rational::operator<=(Rational const& rhs) const {
    return (*this == rhs) || (*this < rhs);
}
constexpr bool Rational::operator>(Rational const& rhs) const {
    return !(*this == rhs) && !(*this < rhs);
}
constexpr bool Rational::operator>=(Rational const& rhs) const {
    return !(*this < rhs);
}

}  // namespace dvlab

template <>
struct fmt::formatter<dvlab::Rational> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(dvlab::Rational const& q, FormatContext& ctx) {
        return (q.denominator() == 1) ? fmt::format_to(ctx.out(), "{}", q.numerator()) : fmt::format_to(ctx.out(), "{}/{}", q.numerator(), q.denominator());
    }
};
