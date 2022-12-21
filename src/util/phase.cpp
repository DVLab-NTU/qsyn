/****************************************************************************
  FileName     [ phase.cpp ]
  PackageName  [ util ]
  Synopsis     [ Implementation of the Phase class and pertinent classes]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/

#include "phase.h"
#include "util.h"
#include <cassert>

PhaseUnit Phase::_printUnit = PhaseUnit::PI;

std::ostream& operator<<(std::ostream& os, const Phase& p) {
    return os << p.getPrintString();
}

Phase Phase::operator+() const {
    return *this;
}
Phase Phase::operator-() const {
    return Phase(-_rational.numerator(), _rational.denominator());
}

Phase& Phase::operator+=(const Phase& rhs) {
    this->_rational += rhs._rational;
    normalize();
    return *this;
}
Phase& Phase::operator-=(const Phase& rhs) {
    this->_rational -= rhs._rational;
    normalize();
    return *this;
}
Phase operator+(Phase lhs, const Phase& rhs) {
    lhs += rhs;
    return lhs;
}
Phase operator-(Phase lhs, const Phase& rhs) {
    lhs -= rhs;
    return lhs;
}
bool Phase::operator== (const Phase& rhs) const{
    return _rational == rhs._rational;
}
bool Phase::operator!= (const Phase& rhs) const{
    return !(*this == rhs);
}

void Phase::normalize() {
    Rational factor = (_rational / 2);
    int integralPart = std::floor(factor.toFloat());
    _rational -= (integralPart * 2);
}

std::ostream& operator<<(std::ostream& os, const setPhaseUnit& pu) {
    Phase::setPrintUnit(pu._printUnit); 
    return os;
}