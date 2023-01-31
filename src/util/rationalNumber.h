/****************************************************************************
  FileName     [ rationalNumber.h ]
  PackageName  [ util ]
  Synopsis     [ Definition of the Rational Number class.]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef RATIONAL_NUM_H
#define RATIONAL_NUM_H

#include <cassert>  // for assert
#include <cmath>
#include <concepts>
#include <iosfwd>  // for ostream

//--- Rational Numbers ----------------------------------
// This class maintains the canonicity of stored rational numbers by simplifying the numerator/denominator whenever possible.
// Rational numbers are not the same as fractions! This class does not support nested fractions or irrational numbers in numerator/denominator.
// This class implicitly convert floating points to rational approximation when performing arithmetic operations.
//--- Rational Numbers ----------------------------------

class Rational {
public:
    // Default constructor for two integral type
    Rational() : _numer(0), _denom(1) {}
    Rational(int n) : _numer(n), _denom(1) {}
    Rational(int n, int d) : _numer(n), _denom(d) {
        assert(d != 0);
        reduce();
    }
    // Implicitly use 1 as denominator
    template <class T>
    requires std::floating_point<T>
    Rational(T f, T eps = 1e-4) {
        *this = Rational::toRational(f, eps);
    }

    // Operator Overloading
    friend std::ostream& operator<<(std::ostream& os, const Rational& q);

    Rational operator+() const;
    Rational operator-() const;

    // Arithmetic operators always preserve the normalities of Rational
    Rational& operator+=(const Rational& rhs);
    Rational& operator-=(const Rational& rhs);
    Rational& operator*=(const Rational& rhs);
    Rational& operator/=(const Rational& rhs);
    friend Rational operator+(Rational lhs, const Rational& rhs);
    friend Rational operator-(Rational lhs, const Rational& rhs);
    friend Rational operator*(Rational lhs, const Rational& rhs);
    friend Rational operator/(Rational lhs, const Rational& rhs);

    bool operator==(const Rational& rhs) const;
    bool operator!=(const Rational& rhs) const;
    bool operator<(const Rational& rhs) const;
    bool operator<=(const Rational& rhs) const;
    bool operator>(const Rational& rhs) const;
    bool operator>=(const Rational& rhs) const;

    // Operations for Rational Numbers
    void reduce();
    int numerator() const { return (int)_numer; }
    int denominator() const { return (int)_denom; }

    template <class T>
    requires std::floating_point<T>
    T toFloatType() const { return ((T)_numer) / _denom; }

    float toFloat() const { return toFloatType<float>(); }
    double toDouble() const { return toFloatType<double>(); }
    long double toLongDouble() const { return toFloatType<long double>(); }

    template <class T>
    requires std::floating_point<T>
    static Rational toRational(T f, T eps = 1e-4);

protected:
    double _numer, _denom;
    static Rational mediant(const Rational& lhs, const Rational& rhs);
};

template <class T>
requires std::floating_point<T>
Rational Rational::toRational(T f, T eps) {
    int integralPart = (int)floor(f);
    f -= integralPart;
    Rational lower(0, 1), upper(1, 1);
    Rational med(1, 2);

    auto inLowerBound = [&f, &eps](const Rational& q) -> bool {
        return ((f - eps) <= q.toFloatType<T>());
    };
    auto inUpperBound = [&f, &eps](const Rational& q) -> bool {
        return ((f + eps) >= q.toFloatType<T>());
    };

    if (inLowerBound(lower) && inUpperBound(lower)) {
        return lower + integralPart;
    }
    if (inLowerBound(upper) && inUpperBound(upper)) {
        return upper + integralPart;
    }

    while (true) {
        if (!inLowerBound(med))
            lower = med;
        else if (!inUpperBound(med))
            upper = med;
        else
            return med + integralPart;
        med = mediant(lower, upper);
    }
}

#endif  // RATIONAL_NUM_H