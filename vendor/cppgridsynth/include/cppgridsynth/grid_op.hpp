// SPDX-License-Identifier: MIT
//
// GridOp: 2x2 matrix over Z[ω] used as the structural backbone of the
// upright reduction & two-dimensional grid problem.  Mirrors
// pygridsynth's `GridOp` class.
#pragma once

#include <array>
#include <optional>
#include <ostream>

#include "cppgridsynth/mpfloat.hpp"
#include "cppgridsynth/ring.hpp"

namespace cppgridsynth {

class GridOp {
public:
    GridOp() : u0_(0, 0, 0, 1), u1_(0, 1, 0, 0) {}  // identity
    GridOp(ZOmega u0, ZOmega u1) : u0_(std::move(u0)), u1_(std::move(u1)) {}

    const ZOmega& u0() const noexcept { return u0_; }
    const ZOmega& u1() const noexcept { return u1_; }

    const mpz_class& a0() const { return u0_.a(); }
    const mpz_class& b0() const { return u0_.b(); }
    const mpz_class& c0() const { return u0_.c(); }
    const mpz_class& d0() const { return u0_.d(); }
    const mpz_class& a1() const { return u1_.a(); }
    const mpz_class& b1() const { return u1_.b(); }
    const mpz_class& c1() const { return u1_.c(); }
    const mpz_class& d1() const { return u1_.d(); }

    // Determinant vector u0.conj * u1; "special" = unitary modulo phase ω.
    ZOmega det_vec() const { return u0_.conj() * u1_; }
    bool is_special() const;

    // Real-valued 2x2 matrix [[u0.real, u1.real], [u0.imag, u1.imag]].
    std::array<MPFloat, 4> to_matrix() const;

    // Multiplication by another grid op / by a Z[ω] vector / DOmega.
    GridOp  operator*(const GridOp& o) const;
    ZOmega  operator*(const ZOmega& v) const;
    DOmega  operator*(const DOmega& v) const;

    GridOp pow(long n) const;
    std::optional<GridOp> inv() const;
    GridOp adj() const;
    GridOp conj_sq2() const { return GridOp(u0_.conj_sq2(), u1_.conj_sq2()); }

    std::string to_string() const;

private:
    ZOmega u0_;
    ZOmega u1_;
};

std::ostream& operator<<(std::ostream& os, const GridOp& g);

}  // namespace cppgridsynth
