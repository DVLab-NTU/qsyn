/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Implementation of the Rational Number class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./rational.hpp"

#include <numeric>
#include <stdexcept>
#include <string>

//----------------------------------------
// Operator Overloading
//----------------------------------------
std::ostream& operator<<(std::ostream& os, Rational const& q) {
    return os << fmt::format("{}", q);
}

Rational Rational::operator+() const {
    return Rational(_numer, _denom);
}
Rational Rational::operator-() const {
    return Rational(-_numer, _denom);
}

// a/b + c/d  = (ad + bc) / bd
// We adopt for this more complex expression (instead of (ad + bc / bd)) so as to minimize the risk of overflow when multiplying numbers
Rational& Rational::operator+=(Rational const& rhs) {
    _numer = _numer * rhs._denom + _denom * rhs._numer;
    _denom = _denom * rhs._denom;
    assert(_denom != 0);
    reduce();
    return *this;
}
Rational& Rational::operator-=(Rational const& rhs) {
    _numer = _numer * rhs._denom - _denom * rhs._numer;
    _denom = _denom * rhs._denom;
    assert(_denom != 0);
    reduce();
    return *this;
}
Rational& Rational::operator*=(Rational const& rhs) {
    _numer *= rhs._numer;
    _denom *= rhs._denom;
    assert(_denom != 0);
    reduce();
    return *this;
}
Rational& Rational::operator/=(Rational const& rhs) {
    if (rhs._numer == 0) {
        throw std::overflow_error("Attempting to divide by 0");
    }
    _numer *= rhs._denom;
    _denom *= rhs._numer;
    assert(_denom != 0);
    reduce();
    return *this;
}
Rational operator+(Rational lhs, Rational const& rhs) {
    lhs += rhs;
    return lhs;
}
Rational operator-(Rational lhs, Rational const& rhs) {
    lhs -= rhs;
    return lhs;
}
Rational operator*(Rational lhs, Rational const& rhs) {
    lhs *= rhs;
    return lhs;
}
Rational operator/(Rational lhs, Rational const& rhs) {
    lhs /= rhs;
    return lhs;
}

bool Rational::operator==(Rational const& rhs) const {
    return (_numer == rhs._numer) && (_denom == rhs._denom);
}
bool Rational::operator!=(Rational const& rhs) const {
    return !(*this == rhs);
}
bool Rational::operator<(Rational const& rhs) const {
    return (_numer * rhs._denom) < (_denom * rhs._numer);
}
bool Rational::operator<=(Rational const& rhs) const {
    return (*this == rhs) || (*this < rhs);
}
bool Rational::operator>(Rational const& rhs) const {
    return !(*this == rhs) && !(*this < rhs);
}
bool Rational::operator>=(Rational const& rhs) const {
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
Rational Rational::_mediant(Rational const& lhs, Rational const& rhs) {
    return Rational((int)(lhs._numer + rhs._numer), (int)(lhs._denom + rhs._denom));
}