// SPDX-License-Identifier: MIT
#include "cppgridsynth/grid_op.hpp"

#include <sstream>

namespace cppgridsynth {

bool GridOp::is_special() const {
    ZOmega v = det_vec();
    return (v.a() + v.c() == 0) && (v.b() == 1 || v.b() == -1);
}

std::array<MPFloat, 4> GridOp::to_matrix() const {
    return {u0_.real(), u1_.real(), u0_.imag(), u1_.imag()};
}

GridOp GridOp::operator*(const GridOp& o) const {
    return GridOp((*this) * o.u0_, (*this) * o.u1_);
}

ZOmega GridOp::operator*(const ZOmega& other) const {
    // The pygridsynth fused multiplication.  Decomposes G * (a+bω+cω²+dω³)
    // back into ZOmega coefficients without intermediate ω-multiplication
    // round-trips.
    const mpz_class& a = other.a();
    const mpz_class& b = other.b();
    const mpz_class& c = other.c();
    const mpz_class& d = other.d();
    mpz_class new_d =
        d0() * d + d1() * b
        + (c1() - a1() + c0() - a0()) / 2 * c
        + (c1() - a1() - c0() + a0()) / 2 * a;
    mpz_class new_c =
        c0() * d + c1() * b
        + (b1() + d1() + b0() + d0()) / 2 * c
        + (b1() + d1() - b0() - d0()) / 2 * a;
    mpz_class new_b =
        b0() * d + b1() * b
        + (c1() + a1() + c0() + a0()) / 2 * c
        + (c1() + a1() - c0() - a0()) / 2 * a;
    mpz_class new_a =
        a0() * d + a1() * b
        + (b1() - d1() + b0() - d0()) / 2 * c
        + (b1() - d1() - b0() + d0()) / 2 * a;
    return ZOmega(new_a, new_b, new_c, new_d);
}

DOmega GridOp::operator*(const DOmega& v) const {
    return DOmega((*this) * v.u(), v.k());
}

GridOp GridOp::pow(long n) const {
    if (n < 0) {
        auto i = inv();
        if (!i) throw std::domain_error("GridOp::pow: non-invertible");
        return i->pow(-n);
    }
    GridOp out;  // identity
    GridOp base = *this;
    while (n > 0) {
        if (n & 1) out = out * base;
        base = base * base;
        n >>= 1;
    }
    return out;
}

std::optional<GridOp> GridOp::inv() const {
    if (!is_special()) return std::nullopt;
    mpz_class new_c0 = (c1() + a1() - c0() - a0()) / 2;
    mpz_class new_a0 = (-c1() - a1() - c0() - a0()) / 2;
    ZOmega new_u0(new_a0, -b0(), new_c0, b1());
    mpz_class new_c1 = (-c1() + a1() + c0() - a0()) / 2;
    mpz_class new_a1 = (c1() - a1() + c0() - a0()) / 2;
    ZOmega new_u1(new_a1, d0(), new_c1, -d1());
    if (det_vec().b() == -1) {
        new_u0 = -new_u0;
        new_u1 = -new_u1;
    }
    return GridOp(new_u0, new_u1);
}

GridOp GridOp::adj() const {
    mpz_class new_c0 = (c1() - a1() + c0() - a0()) / 2;
    mpz_class new_a0 = (c1() - a1() - c0() + a0()) / 2;
    ZOmega new_u0(new_a0, d1(), new_c0, d0());
    mpz_class new_c1 = (c1() + a1() + c0() + a0()) / 2;
    mpz_class new_a1 = (c1() + a1() - c0() - a0()) / 2;
    ZOmega new_u1(new_a1, b1(), new_c1, b0());
    return GridOp(new_u0, new_u1);
}

std::string GridOp::to_string() const {
    std::ostringstream os;
    os << "[[" << d0() << (c0() - a0() >= 0 ? "+" : "") << (c0() - a0()) << "/√2, "
       << d1() << (c1() - a1() >= 0 ? "+" : "") << (c1() - a1()) << "/√2],"
       << "[" << b0() << (c0() + a0() >= 0 ? "+" : "") << (c0() + a0()) << "/√2, "
       << b1() << (c1() + a1() >= 0 ? "+" : "") << (c1() + a1()) << "/√2]]";
    return os.str();
}

std::ostream& operator<<(std::ostream& os, const GridOp& g) { return os << g.to_string(); }

}  // namespace cppgridsynth
