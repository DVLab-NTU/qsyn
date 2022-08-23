/****************************************************************************
  FileName     [ rationalNumber.cpp ]
  PackageName  [ util ]
  Synopsis     [ Implementation of the Rational Number class ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#include <iostream>
#include <numeric>
#include "rationalNumber.h"
//----------------------------------------
// Operator Overloading
//----------------------------------------
std::ostream& operator<< (std::ostream& os, const Rational& q) {
    os << q._numer; 
    if (q._denom != 1) os << "/" << q._denom;
    return os;
}

Rational Rational::operator+() const{
    return Rational(_numer, _denom);
}
Rational Rational::operator-() const{
    return Rational(-_numer, _denom);
}

// a/b + c/d  = (a*lcm(b, d)/b + c*lcm(b, d)/d)/lcm(b,d)
// We adopt for this more complex expression (instead of (ad + bc / bd)) so as to minimize the risk of overflow when multiplying numbers 
Rational& Rational::operator+=(const Rational& rhs) {
    int new_denom = std::lcm(_denom, rhs._denom);
    int new_numer = _numer * (new_denom / _denom) + rhs._numer * (new_denom / rhs._denom);
    _numer = new_numer;
    _denom = new_denom;
    return *this;
}
Rational operator+(Rational lhs, const Rational& rhs) {
    lhs += rhs;
    return lhs;
}
Rational& Rational::operator-=(const Rational& rhs) {
    int new_denom = std::lcm(_denom, rhs._denom);
    int new_numer = _numer * (new_denom / _denom) - rhs._numer * (new_denom / rhs._denom);
    _numer = new_numer;
    _denom = new_denom;
    return *this;
}
Rational operator-(Rational lhs, const Rational& rhs) {
    lhs -= rhs;
    return lhs;
}
Rational& Rational::operator*=(const Rational& rhs) {
    _numer *= rhs._numer;
    _denom *= rhs._denom;
    normalize();
    return *this;
}
Rational operator*(Rational lhs, const Rational& rhs) {
    lhs *= rhs;
    return lhs;
}
Rational& Rational::operator/=(const Rational& rhs) {
    if (rhs._numer == 0) {
        throw std::overflow_error("Attempting to divide by 0");
    }
    _numer *= rhs._denom;
    _denom *= rhs._numer;
    normalize();
    return *this;
}
Rational operator/(Rational lhs, const Rational& rhs) {
    lhs /= rhs;
    return lhs;
}

bool Rational::operator== (const Rational& rhs) const{
    return (_numer == rhs._numer) && (_denom == rhs._denom);
}
bool Rational::operator!= (const Rational& rhs) const{
    return !(*this == rhs);
}
bool Rational::operator< (const Rational& rhs) const{
    return (_numer * rhs._denom) < (_denom * rhs._numer);
}
bool Rational::operator<=(const Rational& rhs) const{
    return (*this == rhs) || (*this < rhs);
}
bool Rational::operator> (const Rational& rhs) const{
    return !(*this == rhs) && !(*this < rhs);
}
bool Rational::operator>=(const Rational& rhs) const{
    return !(*this < rhs);
}


//----------------------------------------
// Operations specific to rational numbers
//----------------------------------------
void Rational::normalize() {
    if (_denom < 0) {
        _numer = -_numer;
        _denom = -_denom;
    }
    int gcd = std::gcd(_numer, _denom);
    _numer /= gcd;
    _denom /= gcd;
}
