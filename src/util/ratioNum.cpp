/****************************************************************************
  FileName     [ myHashMap.h ]
  PackageName  [ util ]
  Synopsis     [ Define rational number and phase type]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#include <iostream>
#include <numeric>
#include "ratioNum.h"
//----------------------------------------
// Operator Overloading
//----------------------------------------
std::ostream& operator<< (std::ostream& os, const RatioNum& q) {
    return os << q._numer << "/" << q._denom;
}

RatioNum RatioNum::operator+() const{
    return RatioNum(_numer, _denom);
}
RatioNum RatioNum::operator-() const{
    return RatioNum(-_numer, _denom);
}

// a/b + c/d  = (a*lcm(b, d)/b + c*lcm(b, d)/d)/lcm(b,d)
// We adopt for this more complex expression (instead of (ad + bc / bd)) so as to minimize the risk of overflow when multiplying numbers 
RatioNum& RatioNum::operator+=(const RatioNum& rhs) {
    int new_denom = std::lcm(_denom, rhs._denom);
    int new_numer = _numer * (new_denom / _denom) + rhs._numer * (new_denom / rhs._denom);
    _numer = new_numer;
    _denom = new_denom;
    return *this;
}
RatioNum operator+(RatioNum lhs, const RatioNum& rhs) {
    lhs += rhs;
    return lhs;
}
RatioNum& RatioNum::operator-=(const RatioNum& rhs) {
    int new_denom = std::lcm(_denom, rhs._denom);
    int new_numer = _numer * (new_denom / _denom) - rhs._numer * (new_denom / rhs._denom);
    _numer = new_numer;
    _denom = new_denom;
    return *this;
}
RatioNum operator-(RatioNum lhs, const RatioNum& rhs) {
    lhs -= rhs;
    return lhs;
}
RatioNum& RatioNum::operator*=(const RatioNum& rhs) {
    _numer *= rhs._numer;
    _denom *= rhs._denom;
    normalize();
    return *this;
}
RatioNum operator*(RatioNum lhs, const RatioNum& rhs) {
    lhs *= rhs;
    return lhs;
}
RatioNum& RatioNum::operator/=(const RatioNum& rhs) {
    _numer *= rhs._denom;
    _denom *= rhs._numer;
    normalize();
    return *this;
}
RatioNum operator/(RatioNum lhs, const RatioNum& rhs) {
    lhs /= rhs;
    return lhs;
}

bool RatioNum::operator== (const RatioNum& rhs) const{
    return (_numer == rhs._numer) && (_denom == rhs._denom);
}
bool RatioNum::operator!= (const RatioNum& rhs) const{
    return !(*this == rhs);
}
bool RatioNum::operator< (const RatioNum& rhs) const{
    return (_numer * rhs._denom) < (_denom * rhs._numer);
}
bool RatioNum::operator<=(const RatioNum& rhs) const{
    return (*this == rhs) || (*this < rhs);
}
bool RatioNum::operator> (const RatioNum& rhs) const{
    return !(*this == rhs) && !(*this < rhs);
}
bool RatioNum::operator>=(const RatioNum& rhs) const{
    return !(*this < rhs);
}


//----------------------------------------
// Operations specific to rational numbers
//----------------------------------------
void RatioNum::normalize() {
    if (_denom < 0) {
        _numer = -_numer;
        _denom = -_denom;
    }
    int gcd = std::gcd(_numer, _denom);
    _numer /= gcd;
    _denom /= gcd;
}