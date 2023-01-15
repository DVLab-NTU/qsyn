/****************************************************************************
  FileName     [ rationalNumber.cpp ]
  PackageName  [ util ]
  Synopsis     [ Implementation of the Rational Number class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "rationalNumber.h"

#include <iostream>
#include <numeric>
#include <stdexcept>

//----------------------------------------
// Operator Overloading
//----------------------------------------
std::ostream& operator<<(std::ostream& os, const Rational& q) {
    os << q._numer;
    if (q._denom != 1) os << "/" << q._denom;
    return os;
}

Rational Rational::operator+() const {
    return Rational(_numer, _denom);
}
Rational Rational::operator-() const {
    return Rational(-_numer, _denom);
}

// a/b + c/d  = (ad + bc) / bd
// We adopt for this more complex expression (instead of (ad + bc / bd)) so as to minimize the risk of overflow when multiplying numbers
Rational& Rational::operator+=(const Rational& rhs) {
    _numer = _numer * rhs._denom + _denom * rhs._numer;
    _denom = _denom * rhs._denom;
    assert(_denom != 0);
    reduce();
    return *this;
}
Rational& Rational::operator-=(const Rational& rhs) {
    _numer = _numer * rhs._denom - _denom * rhs._numer;
    _denom = _denom * rhs._denom;
    assert(_denom != 0);
    reduce();
    return *this;
}
Rational& Rational::operator*=(const Rational& rhs) {
    _numer *= rhs._numer;
    _denom *= rhs._denom;
    assert(_denom != 0);
    reduce();
    return *this;
}
Rational& Rational::operator/=(const Rational& rhs) {
    if (rhs._numer == 0) {
        throw std::overflow_error("Attempting to divide by 0");
    }
    _numer *= rhs._denom;
    _denom *= rhs._numer;
    assert(_denom != 0);
    reduce();
    return *this;
}
Rational operator+(Rational lhs, const Rational& rhs) {
    lhs += rhs;
    return lhs;
}
Rational operator-(Rational lhs, const Rational& rhs) {
    lhs -= rhs;
    return lhs;
}
Rational operator*(Rational lhs, const Rational& rhs) {
    lhs *= rhs;
    return lhs;
}
Rational operator/(Rational lhs, const Rational& rhs) {
    lhs /= rhs;
    return lhs;
}

bool Rational::operator==(const Rational& rhs) const {
    return (_numer == rhs._numer) && (_denom == rhs._denom);
}
bool Rational::operator!=(const Rational& rhs) const {
    return !(*this == rhs);
}
bool Rational::operator<(const Rational& rhs) const {
    return (_numer * rhs._denom) < (_denom * rhs._numer);
}
bool Rational::operator<=(const Rational& rhs) const {
    return (*this == rhs) || (*this < rhs);
}
bool Rational::operator>(const Rational& rhs) const {
    return !(*this == rhs) && !(*this < rhs);
}
bool Rational::operator>=(const Rational& rhs) const {
    return !(*this < rhs);
}

//----------------------------------------
// Operations specific to rational numbers
//----------------------------------------
void Rational::reduce() {
    if (_denom < 0) {
        _numer = -_numer;
        _denom = -_denom;
    }
    int gcd = std::gcd((int)_numer, (int)_denom);
    _numer /= gcd;
    _denom /= gcd;
}

/**
 * @brief Calculate mediant
 *
 * @param lhs
 * @param rhs
 * @return Rational
 */
Rational Rational::mediant(const Rational& lhs, const Rational& rhs) {
    return Rational((int)(lhs._numer + rhs._numer), (int)(lhs._denom + rhs._denom));
}