// SPDX-License-Identifier: MIT
//
// Geometric primitives over MPFloat used by the grid problem solver.
#pragma once

#include <array>
#include <optional>
#include <utility>

#include "cppgridsynth/mpfloat.hpp"

namespace cppgridsynth {

class GridOp;
class DOmega;

// =====================================================================
//  Interval [l, r]  on the real line.
// =====================================================================
class Interval {
public:
    Interval() : l(), r() {}
    Interval(MPFloat L, MPFloat R) : l(std::move(L)), r(std::move(R)) {}

    MPFloat width() const { return r - l; }
    bool within(const MPFloat& x) const { return l <= x && x <= r; }
    Interval fatten(const MPFloat& eps) const { return Interval(l - eps, r + eps); }

    Interval operator+(const MPFloat& o) const { return Interval(l + o, r + o); }
    Interval operator-(const MPFloat& o) const { return Interval(l - o, r - o); }
    Interval operator-() const { return Interval(-r, -l); }
    Interval operator*(const MPFloat& o) const;
    Interval operator/(const MPFloat& o) const;
    friend Interval operator+(const MPFloat& a, const Interval& b) { return b + a; }

    MPFloat l;
    MPFloat r;
};

// =====================================================================
//  Axis-aligned rectangle  Ix × Iy.
// =====================================================================
class Rectangle {
public:
    Rectangle() = default;
    Rectangle(MPFloat xl, MPFloat xr, MPFloat yl, MPFloat yr)
        : I_x(std::move(xl), std::move(xr)),
          I_y(std::move(yl), std::move(yr)) {}

    Interval I_x;
    Interval I_y;
    MPFloat area() const { return I_x.width() * I_y.width(); }
};

// =====================================================================
//  Centred ellipse  E = { v : (v-p)^T D (v-p) <= 1 }.
// =====================================================================
class Ellipse {
public:
    Ellipse(); // identity ellipse at origin
    Ellipse(MPFloat a, MPFloat b, MPFloat d, MPFloat px, MPFloat py)
        : a_(std::move(a)), b_(std::move(b)), d_(std::move(d)),
          px_(std::move(px)), py_(std::move(py)) {}
    Ellipse(const std::array<MPFloat, 3>& D /* (a,b,d) */, MPFloat px, MPFloat py)
        : a_(D[0]), b_(D[1]), d_(D[2]),
          px_(std::move(px)), py_(std::move(py)) {}

    const MPFloat& a()  const { return a_;  }
    const MPFloat& b()  const { return b_;  }
    const MPFloat& d()  const { return d_;  }
    const MPFloat& px() const { return px_; }
    const MPFloat& py() const { return py_; }
    MPFloat& a()  { return a_;  }
    MPFloat& b()  { return b_;  }
    MPFloat& d()  { return d_;  }
    MPFloat& px() { return px_; }
    MPFloat& py() { return py_; }

    bool inside(const MPFloat& vx, const MPFloat& vy) const;
    Rectangle bbox() const;
    MPFloat sqrt_det() const;
    MPFloat area() const;
    MPFloat skew() const { return b_ * b_; }
    MPFloat bias() const { return d_ / a_; }
    Ellipse normalize() const;

    Ellipse operator/(const MPFloat& s) const;
    Ellipse operator*(const MPFloat& s) const;

    // Apply a special grid operator G to this ellipse: returns G * E.
    Ellipse transform(const GridOp& G) const;

private:
    MPFloat a_, b_, d_;
    MPFloat px_, py_;
};

// =====================================================================
//  Pair of Ellipses --- used by the upright reduction.
// =====================================================================
class EllipsePair {
public:
    EllipsePair(Ellipse A, Ellipse B) : A(std::move(A)), B(std::move(B)) {}
    MPFloat skew() const { return A.skew() + B.skew(); }
    MPFloat bias() const { return B.bias() / A.bias(); }
    Ellipse A;
    Ellipse B;
};

// =====================================================================
//  Convex set: ellipse + (intersect / inside) interface.
// =====================================================================
class ConvexSet {
public:
    explicit ConvexSet(Ellipse e) : ellipse_(std::move(e)) {}
    virtual ~ConvexSet() = default;

    const Ellipse& ellipse() const noexcept { return ellipse_; }

    virtual bool inside(const DOmega& u) const = 0;
    virtual std::optional<std::pair<MPFloat, MPFloat>> intersect(const DOmega& u0,
                                                                 const DOmega& v) const = 0;

protected:
    Ellipse ellipse_;
};

}  // namespace cppgridsynth
