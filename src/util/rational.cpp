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

namespace dvlab {

/**
 * @brief Calculate mediant
 *
 * @param lhs
 * @param rhs
 * @return Rational
 */
Rational Rational::_mediant(Rational const& lhs, Rational const& rhs) {
    return Rational(static_cast<IntegralType>(lhs._numer + rhs._numer), static_cast<IntegralType>(lhs._denom + rhs._denom));
}

}  // namespace dvlab

std::ostream& operator<<(std::ostream& os, dvlab::Rational const& q) {
    return os << fmt::format("{}", q);
}