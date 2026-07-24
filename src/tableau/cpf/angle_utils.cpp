/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Phase / angle predicates shared by all CPF passes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./angle_utils.hpp"

#include <cmath>

namespace qsyn::experimental::cpf {

bool is_clifford_phase(dvlab::Phase const& phase) noexcept {
    // dvlab::Phase is rational in units of pi and is auto-normalised to
    // (-1, 1]. A phase represents an integer multiple of pi/2 iff its reduced
    // denominator is 1 or 2.
    return phase.denominator() == 1 || phase.denominator() == 2;
}

bool is_zero_phase(dvlab::Phase const& phase) noexcept {
    // After normalisation, 0 has the canonical form `0 / 1`.
    return phase.numerator() == 0;
}

bool is_pi_phase(dvlab::Phase const& phase) noexcept {
    // After normalisation, pi has the canonical form `1 / 1` (the value is
    // expressed in units of pi). -pi normalises to +pi.
    return phase.denominator() == 1 && phase.numerator() == 1;
}

double phase_to_radians(dvlab::Phase const& phase) noexcept {
    return dvlab::Phase::phase_to_d(phase);
}

bool approx_equal(dvlab::Phase const& lhs,
                  dvlab::Phase const& rhs,
                  double tol) noexcept {
    auto const diff = phase_to_radians(lhs - rhs);
    return std::abs(diff) <= tol;
}

}  // namespace qsyn::experimental::cpf
