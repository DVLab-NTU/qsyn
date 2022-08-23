/****************************************************************************
  FileName     [ myHashMap.h ]
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
class Rational {
public: 
    // Default constructor for two integral type
    Rational(int n, int d): _numer(n), _denom(d) { normalize(); }
    // Implicitly use 1 as denominator
    Rational(int n): _numer(n), _denom(1) {}
    template <class T> requires std::floating_point<T>
    Rational(T n, T eps = 1e-4) {
        *this = Rational::approximate(n, eps);
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
    friend Rational operator+(Rational lhs, const Rational& rhs); 
    Rational& operator-=(const Rational& rhs);
    friend Rational operator-(Rational lhs, const Rational& rhs); 
    Rational& operator*=(const Rational& rhs);
    friend Rational operator*(Rational lhs, const Rational& rhs); 
    Rational& operator/=(const Rational& rhs);
    friend Rational operator/(Rational lhs, const Rational& rhs); 

    bool operator== (const Rational& rhs) const;
    bool operator!= (const Rational& rhs) const;
    bool operator<  (const Rational& rhs) const;
    bool operator<= (const Rational& rhs) const;
    bool operator>  (const Rational& rhs) const;
    bool operator>= (const Rational& rhs) const;
  
    // Operations for Rational Numbers
    void normalize();
    void reciprocal(const Rational& q);
    template <class T> requires std::floating_point<T>
    static Rational approximate(T f, T eps = 1e-4) {
    std::vector<int> ctd_fracs;
    auto cur = f;
    while (true) {
        auto integralPart = floor(cur);
        ctd_fracs.push_back((int) (integralPart));
        if ((cur - integralPart) < eps) break;
        cur = 1. / (cur - integralPart);
    }
    Rational q = 0;
    while (true) {
        q += ctd_fracs.back();
        ctd_fracs.pop_back();
        if (ctd_fracs.empty()) break;
        q = 1 / q;
    }
    return q;
}

    
protected:
    int _numer, _denom;
};

#endif //RATIONAL_NUM_H