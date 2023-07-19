/****************************************************************************
  FileName     [ phase.cpp ]
  PackageName  [ util ]
  Synopsis     [ Implementation of the Phase class and pertinent classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "phase.h"

#include "rationalNumber.h"

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
Rational operator/(const Phase& lhs, const Phase& rhs) {
    Rational q = lhs._rational / rhs._rational;
    return q;
}
bool Phase::operator==(const Phase& rhs) const {
    return _rational == rhs._rational;
}
bool Phase::operator!=(const Phase& rhs) const {
    return !(*this == rhs);
}

/**
 * @brief Get Ascii String
 *
 * @return std::string
 */
std::string Phase::getAsciiString() const {
    std::string str;
    if (_rational.numerator() != 1)
        str += std::to_string(_rational.numerator()) + "*";
    str += "pi";
    if (_rational.denominator() != 1)
        str += "/" + std::to_string(_rational.denominator());
    return str;
}

/**
 * @brief Get string for printing
 *
 * @return std::string
 */
std::string Phase::getPrintString() const {
    if (Phase::getPrintUnit() == PhaseUnit::PI) {
        return (
                   _rational.numerator() == 1 ? ""
                   : _rational.numerator() == -1
                       ? "-"
                       : std::to_string(_rational.numerator())) +
               ((_rational.numerator() != 0) ? "\u03C0" : "") + ((_rational.denominator() != 1) ? ("/" + std::to_string(_rational.denominator())) : "");
    } else {
        return std::to_string(this->toFloatType<double>());
    }
}

/**
 * @brief Normalize the phase to 0-2pi
 *
 */
void Phase::normalize() {
    Rational factor = (_rational / 2);
    int integralPart = std::floor(factor.toFloat());
    _rational -= (integralPart * 2);
    if (_rational > 1) _rational -= 2;
}

std::ostream& operator<<(std::ostream& os, const setPhaseUnit& pu) {
    Phase::setPrintUnit(pu._printUnit);
    return os;
}