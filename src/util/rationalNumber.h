/****************************************************************************
  FileName     [ rationalNumber.h ]
  PackageName  [ util ]
  Synopsis     [ Definition of the Rational Number class]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#ifndef RATIONAL_NUM_H
#define RATIONAL_NUM_H

// TODO: define rational number class
#include <iostream>
#include <tuple>
#include <concepts>
#include <vector>
#include <cmath>
#include <type_traits>
class Rational {
public: 
    // Default constructor for two integral type
    Rational(int n, int d): _numer(n), _denom(d) { normalize(); }
    // Implicitly use 1 as denominator
    Rational(int n): _numer(n), _denom(1) {}

    template <class T> requires std::floating_point<T>
    Rational(T n, T eps = 1e-4) {
        *this = Rational::toRational(n, eps);
    }
    Rational(const Rational&) = default;
    virtual ~Rational() = default;

    // Operator Overloading
    friend std::ostream& operator<< (std::ostream& os, const Rational& q);
    
    Rational& operator=(const Rational& rhs) = default;
    Rational& operator=(Rational&& rhs) = default;

    Rational operator+() const;
    Rational operator-() const;

    // Arithmetic operators always preserve the normalities of RatioNum
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
    bool operator< (const Rational& rhs) const;
    bool operator<=(const Rational& rhs) const;
    bool operator> (const Rational& rhs) const;
    bool operator>=(const Rational& rhs) const;
  
    // Operations for Rational Numbers
    void normalize();
    int  numerator() const {
        return _numer;
    }
    int  denominator() const {
        return _denom;
    }

    virtual float toFloat() { return ((float) _numer) / _denom; }
    virtual double toDouble() { return ((double) _numer) / _denom; }
    virtual long double toLongDouble() { return ((long double) _numer) / _denom; }

    template <class T> requires std::floating_point<T>
    static Rational toRational(T f, T eps = 1e-4){
        int integralPart = (int) floor(f);
        f -= integralPart;
        Rational lower(0, 1), upper(1, 1);
        Rational med(1, 2);
        while (true) {
            T med_floatType;
            if constexpr (std::is_same<T, double>::value) {
                med_floatType = med.toDouble();
            } else if constexpr (std::is_same<T, float>::value) {
                med_floatType = med.toFloat();
            } else {
                med_floatType = med.toLongDouble();
            }
            if ((med_floatType + eps) < f) {
                lower = med;
            } else if ((med_floatType - eps > f)) {
                upper = med;
            } else {
                return med + integralPart;
            }
            med = mediant(lower, upper);
        }
    }

    
protected:
    int _numer, _denom;
    static Rational mediant(const Rational& lhs, const Rational& rhs);
};

#endif //RATIONAL_NUM_H