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
#include <iosfwd>

//--- Rational Numbers ----------------------------------
// This class maintains the canonicity of stored rational numbers by simplifying the numerator/denominator whenever possible.
// Rational numbers are not the same as fractions! This class does not support nested fractions or irrational numbers in numerator/denominator.
// This class implicitly convert floating points to rational approximation when performing arithmetic operations.
//--- Rational Numbers ----------------------------------

namespace dvlab {

class Rational {
public:
    // Default constructor for two integral type
    constexpr Rational() {}
    constexpr Rational(int n) : _numer(n) {}
    constexpr Rational(int n, int d) : _numer(n), _denom(d) {
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

    Rational operator+() const;
    Rational operator-() const;

    // Arithmetic operators always preserve the normalities of Rational
    Rational& operator+=(Rational const& rhs);
    Rational& operator-=(Rational const& rhs);
    Rational& operator*=(Rational const& rhs);
    Rational& operator/=(Rational const& rhs);
    friend Rational operator+(Rational lhs, Rational const& rhs);
    friend Rational operator-(Rational lhs, Rational const& rhs);
    friend Rational operator*(Rational lhs, Rational const& rhs);
    friend Rational operator/(Rational lhs, Rational const& rhs);

    bool operator==(Rational const& rhs) const;
    bool operator!=(Rational const& rhs) const;
    bool operator<(Rational const& rhs) const;
    bool operator<=(Rational const& rhs) const;
    bool operator>(Rational const& rhs) const;
    bool operator>=(Rational const& rhs) const;

    // Operations for Rational Numbers
    void reduce();
    int numerator() const { return (int)_numer; }
    int denominator() const { return (int)_denom; }

    template <class T>
    requires std::floating_point<T>
    static T rational_to_floating_point(Rational const& q) { return ((T)q._numer) / q._denom; }

    static float rational_to_f(Rational const& q) { return rational_to_floating_point<float>(q); }
    static double rational_to_d(Rational const& q) { return rational_to_floating_point<double>(q); }
    static long double rational_to_ld(Rational const& q) { return rational_to_floating_point<long double>(q); }

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
    int integral_part = static_cast<int>(floor(f));
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

}  // namespace dvlab

template <>
struct fmt::formatter<dvlab::Rational> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(dvlab::Rational const& q, FormatContext& ctx) {
        return (q.denominator() == 1) ? fmt::format_to(ctx.out(), "{}", q.numerator()) : fmt::format_to(ctx.out(), "{}/{}", q.numerator(), q.denominator());
    }
};