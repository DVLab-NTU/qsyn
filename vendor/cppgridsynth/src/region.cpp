// SPDX-License-Identifier: MIT
#include "cppgridsynth/region.hpp"

#include <stdexcept>

#include "cppgridsynth/grid_op.hpp"
#include "cppgridsynth/mymath.hpp"
#include "cppgridsynth/ring.hpp"

namespace cppgridsynth {

// ---- Interval ----------------------------------------------------------
Interval Interval::operator*(const MPFloat& o) const {
    if (o >= 0) return Interval(l * o, r * o);
    return Interval(r * o, l * o);
}
Interval Interval::operator/(const MPFloat& o) const {
    if (o > 0) return Interval(l / o, r / o);
    if (o < 0) return Interval(r / o, l / o);
    throw std::domain_error("Interval::operator/: division by zero");
}

// ---- Ellipse -----------------------------------------------------------
Ellipse::Ellipse() : a_(1), b_(0), d_(1), px_(0), py_(0) {}

bool Ellipse::inside(const MPFloat& vx, const MPFloat& vy) const {
    MPFloat x = vx - px_;
    MPFloat y = vy - py_;
    MPFloat tmp = a_ * x * x + MPFloat(2) * b_ * x * y + d_ * y * y;
    return tmp <= MPFloat(1);
}

Rectangle Ellipse::bbox() const {
    MPFloat sd = sqrt_det();
    MPFloat w = sqrt(d_) / sd;
    MPFloat h = sqrt(a_) / sd;
    return Rectangle(px_ - w, px_ + w, py_ - h, py_ + h);
}

MPFloat Ellipse::sqrt_det() const {
    MPFloat det = d_ * a_ - b_ * b_;
    return sqrt(det);
}

MPFloat Ellipse::area() const { return MPFloat::pi() / sqrt_det(); }

Ellipse Ellipse::normalize() const {
    MPFloat sd = sqrt_det();
    MPFloat sd_root = sqrt(sd);
    return Ellipse(a_ / sd, b_ / sd, d_ / sd, px_ * sd_root, py_ * sd_root);
}

Ellipse Ellipse::operator/(const MPFloat& s) const {
    MPFloat s2 = s * s;
    return Ellipse(a_ * s2, b_ * s2, d_ * s2, px_ / s, py_ / s);
}
Ellipse Ellipse::operator*(const MPFloat& s) const {
    MPFloat inv2 = MPFloat(1) / (s * s);
    return Ellipse(a_ * inv2, b_ * inv2, d_ * inv2, px_ * s, py_ * s);
}

Ellipse Ellipse::transform(const GridOp& G) const {
    auto Ginv = G.inv();
    if (!Ginv) {
        throw std::domain_error("Ellipse::transform: grid operator has no inverse");
    }
    auto Mi = Ginv->to_matrix();
    auto M  = G.to_matrix();
    MPFloat M00 = Mi[0], M01 = Mi[1], M10 = Mi[2], M11 = Mi[3];
    MPFloat na = a_ * M00 * M00 + MPFloat(2) * b_ * M00 * M10 + d_ * M10 * M10;
    MPFloat nb = a_ * M00 * M01
                  + b_ * (M00 * M11 + M01 * M10)
                  + d_ * M10 * M11;
    MPFloat nd = a_ * M01 * M01 + MPFloat(2) * b_ * M11 * M01 + d_ * M11 * M11;
    MPFloat npx = M[0] * px_ + M[1] * py_;
    MPFloat npy = M[2] * px_ + M[3] * py_;
    return Ellipse(na, nb, nd, npx, npy);
}

}  // namespace cppgridsynth
