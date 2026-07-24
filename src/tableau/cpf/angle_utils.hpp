/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Phase / angle predicates shared by all CPF passes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "util/phase.hpp"

namespace qsyn::experimental::cpf {

// True iff `phase` represents an integer multiple of pi/2 (i.e. a Clifford
// rotation angle). qsyn's dvlab::Phase is rational in units of pi and is
// auto-normalised to (-1, 1], so the check is simply whether the denominator
// divides 2.
[[nodiscard]] bool is_clifford_phase(dvlab::Phase const& phase) noexcept;

// True iff `phase` is the zero angle (modulo 2 pi).
[[nodiscard]] bool is_zero_phase(dvlab::Phase const& phase) noexcept;

// True iff `phase` equals pi (modulo 2 pi). Corresponds to the X / Y / Z Pauli
// gate, depending on the rotation axis it is attached to.
[[nodiscard]] bool is_pi_phase(dvlab::Phase const& phase) noexcept;

// Floating-point view of `phase` in radians, range (-pi, +pi].
[[nodiscard]] double phase_to_radians(dvlab::Phase const& phase) noexcept;

// True iff |radians(lhs - rhs)| <= tol. Useful when comparing phases that
// originate from `dvlab::Phase::from_string` rounding.
[[nodiscard]] bool approx_equal(dvlab::Phase const& lhs,
                                dvlab::Phase const& rhs,
                                double tol = 1e-9) noexcept;

}  // namespace qsyn::experimental::cpf
