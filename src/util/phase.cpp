/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Implementation of the Phase class and pertinent classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./phase.hpp"

#include "./rational.hpp"
#include "argparse/arg_def.hpp"

namespace dvlab {

Phase Phase::operator+() const {
    return *this;
}
Phase Phase::operator-() const {
    return Phase(-_rational.numerator(), _rational.denominator());
}

Phase& Phase::operator+=(Phase const& rhs) {
    this->_rational += rhs._rational;
    normalize();
    return *this;
}
Phase& Phase::operator-=(Phase const& rhs) {
    this->_rational -= rhs._rational;
    normalize();
    return *this;
}
Phase operator+(Phase lhs, Phase const& rhs) {
    lhs += rhs;
    return lhs;
}
Phase operator-(Phase lhs, Phase const& rhs) {
    lhs -= rhs;
    return lhs;
}
Rational operator/(Phase const& lhs, Phase const& rhs) {
    Rational q = lhs._rational / rhs._rational;
    return q;
}
bool Phase::operator==(Phase const& rhs) const {
    return _rational == rhs._rational;
}
bool Phase::operator!=(Phase const& rhs) const {
    return !(*this == rhs);
}

/**
 * @brief Get Ascii String
 *
 * @return std::string
 */
std::string Phase::get_ascii_string() const {
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
std::string Phase::get_print_string() const {
    return (
               _rational.numerator() == 1 ? ""
               : _rational.numerator() == -1
                   ? "-"
                   : std::to_string(_rational.numerator())) +
           ((_rational.numerator() != 0) ? "\u03C0" : "") + ((_rational.denominator() != 1) ? ("/" + std::to_string(_rational.denominator())) : "");
}

/**
 * @brief Normalize the phase to 0-2pi
 *
 */
void Phase::normalize() {
    Rational factor = (_rational / 2);
    IntegralType integral_part = std::floor(Rational::rational_to_f(factor));
    _rational -= (integral_part * 2);
    if (_rational > 1) _rational -= 2;
}

std::ostream& operator<<(std::ostream& os, dvlab::Phase const& p) {
    return os << p.get_print_string();
}

}  // namespace dvlab

namespace dvlab::argparse {
template <>
std::string type_string(dvlab::Phase const& /*unused*/) { return "Phase"; }
template <>
bool parse_from_string(dvlab::Phase& val, std::string const& token) {
    return dvlab::Phase::str_to_phase(token, val);
}
}  // namespace dvlab::argparse