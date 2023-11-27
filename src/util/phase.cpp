/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Implementation of the Phase class and pertinent classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./phase.hpp"

#include "./rational.hpp"
#include "argparse/arg_def.hpp"

#ifdef _LIBCPP_VERSION
#include <iostream>
#endif

namespace dvlab {

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

std::ostream& operator<<(std::ostream& os, dvlab::Phase const& p) {
    return os << p.get_print_string();
}

}  // namespace dvlab

namespace dvlab::argparse {
template <>
std::string type_string(dvlab::Phase const& /*unused*/) { return "Phase"; }
template <>
bool parse_from_string(dvlab::Phase& val, std::string_view token) {
    return dvlab::Phase::str_to_phase(token, val);
}
}  // namespace dvlab::argparse
