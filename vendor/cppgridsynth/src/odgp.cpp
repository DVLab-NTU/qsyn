// SPDX-License-Identifier: MIT
#include "cppgridsynth/odgp.hpp"

#include <memory>

#include "cppgridsynth/mymath.hpp"

namespace cppgridsynth {

ZRootTwoGen empty_zroottwo_gen() {
    return []() -> std::optional<ZRootTwo> { return std::nullopt; };
}

DRootTwoGen empty_droottwo_gen() {
    return []() -> std::optional<DRootTwo> { return std::nullopt; };
}

namespace {

// Forward declarations.
ZRootTwoGen solve_ODGP_internal(const Interval& I, const Interval& J);

// Builds a generator that emits ZRootTwo(a, b) for b in [b_min, b_max]
// then advances `a` and recomputes the b-range.  Lazily reads I, J via
// captured values.
ZRootTwoGen gen_from_a_range(MPFloat I_l, MPFloat I_r,
                             MPFloat J_l, MPFloat J_r) {
    mpz_class a_min = ceil((I_l + J_l) / MPFloat(2));
    mpz_class a_max = floor((I_r + J_r) / MPFloat(2));

    // State held by reference via shared_ptr so it survives copies.
    struct State {
        mpz_class a;
        mpz_class a_max;
        mpz_class b;
        mpz_class b_max;
        bool      have_b_range;
        MPFloat   J_l;
        MPFloat   J_r;
    };
    auto st = std::make_shared<State>();
    st->a = a_min;
    st->a_max = a_max;
    st->b = 0;
    st->b_max = -1;
    st->have_b_range = false;
    st->J_l = J_l;
    st->J_r = J_r;

    return [st]() mutable -> std::optional<ZRootTwo> {
        while (true) {
            if (!st->have_b_range) {
                if (st->a > st->a_max) return std::nullopt;
                MPFloat sq2 = MPFloat::sqrt2();
                st->b     = ceil((sq2 * (MPFloat(st->a) - st->J_r)) / MPFloat(2));
                st->b_max = floor((sq2 * (MPFloat(st->a) - st->J_l)) / MPFloat(2));
                st->have_b_range = true;
            }
            if (st->b > st->b_max) {
                st->a += 1;
                st->have_b_range = false;
                continue;
            }
            ZRootTwo result(st->a, st->b);
            st->b += 1;
            return result;
        }
    };
}

// Compose: yields f(g()) lazily.
ZRootTwoGen map_zr(ZRootTwoGen src, std::function<ZRootTwo(const ZRootTwo&)> f) {
    auto src_ptr = std::make_shared<ZRootTwoGen>(std::move(src));
    auto fp = std::make_shared<std::function<ZRootTwo(const ZRootTwo&)>>(std::move(f));
    return [src_ptr, fp]() -> std::optional<ZRootTwo> {
        auto v = (*src_ptr)();
        if (!v) return std::nullopt;
        return (*fp)(*v);
    };
}

ZRootTwoGen filter_zr(ZRootTwoGen src,
                      std::function<bool(const ZRootTwo&)> pred) {
    auto src_ptr = std::make_shared<ZRootTwoGen>(std::move(src));
    auto pp = std::make_shared<std::function<bool(const ZRootTwo&)>>(std::move(pred));
    return [src_ptr, pp]() -> std::optional<ZRootTwo> {
        while (true) {
            auto v = (*src_ptr)();
            if (!v) return std::nullopt;
            if ((*pp)(*v)) return v;
        }
    };
}

DRootTwoGen map_dr(DRootTwoGen src, std::function<DRootTwo(const DRootTwo&)> f) {
    auto src_ptr = std::make_shared<DRootTwoGen>(std::move(src));
    auto fp = std::make_shared<std::function<DRootTwo(const DRootTwo&)>>(std::move(f));
    return [src_ptr, fp]() -> std::optional<DRootTwo> {
        auto v = (*src_ptr)();
        if (!v) return std::nullopt;
        return (*fp)(*v);
    };
}

DRootTwoGen map_zr_to_dr(ZRootTwoGen src,
                        std::function<DRootTwo(const ZRootTwo&)> f) {
    auto src_ptr = std::make_shared<ZRootTwoGen>(std::move(src));
    auto fp = std::make_shared<std::function<DRootTwo(const ZRootTwo&)>>(std::move(f));
    return [src_ptr, fp]() -> std::optional<DRootTwo> {
        auto v = (*src_ptr)();
        if (!v) return std::nullopt;
        return (*fp)(*v);
    };
}

ZRootTwoGen solve_ODGP_internal(const Interval& I, const Interval& J) {
    if (I.width() < 0 || J.width() < 0) return empty_zroottwo_gen();

    if (I.width() > 0 && J.width() <= 0) {
        // Solve the swapped problem and apply conj_sq2.
        ZRootTwoGen sol = solve_ODGP_internal(J, I);
        return map_zr(std::move(sol), [](const ZRootTwo& b) { return b.conj_sq2(); });
    }

    long n = 0;
    if (J.width() > 0) {
        auto fl = floorlog(J.width(), LAMBDA().to_real());
        n = fl.first;
    }
    if (n == 0) {
        return gen_from_a_range(I.l, I.r, J.l, J.r);
    }

    ZRootTwo lambda_n = LAMBDA().pow(n);
    ZRootTwo lambda_inv_n = LAMBDA().pow(-n);
    ZRootTwo lambda_conj_n = LAMBDA().conj_sq2().pow(n);
    Interval I2(I.l * lambda_n.to_real(), I.r * lambda_n.to_real());
    if (lambda_n.to_real() < 0) std::swap(I2.l, I2.r);
    Interval J2(J.l * lambda_conj_n.to_real(), J.r * lambda_conj_n.to_real());
    if (lambda_conj_n.to_real() < 0) std::swap(J2.l, J2.r);

    ZRootTwoGen sol = solve_ODGP_internal(I2, J2);
    return map_zr(std::move(sol),
                  [lambda_inv_n](const ZRootTwo& b) { return b * lambda_inv_n; });
}

}  // namespace

ZRootTwoGen solve_ODGP(const Interval& I, const Interval& J) {
    if (I.width() < 0 || J.width() < 0) return empty_zroottwo_gen();

    mpz_class a = floor((I.l + J.l) / MPFloat(2));
    mpz_class b = floor((MPFloat::sqrt2() * (I.l - J.l)) / MPFloat(4));
    ZRootTwo alpha(a, b);
    Interval Ip(I.l - alpha.to_real(), I.r - alpha.to_real());
    Interval Jp(J.l - alpha.conj_sq2().to_real(), J.r - alpha.conj_sq2().to_real());
    ZRootTwoGen sol = solve_ODGP_internal(Ip, Jp);
    sol = map_zr(std::move(sol),
                 [alpha](const ZRootTwo& v) { return v + alpha; });
    sol = filter_zr(std::move(sol),
                    [I, J](const ZRootTwo& v) {
                        return I.within(v.to_real()) && J.within(v.conj_sq2().to_real());
                    });
    return sol;
}

ZRootTwoGen solve_ODGP_with_parity(const Interval& I, const Interval& J,
                                   const ZRootTwo& beta) {
    int p = beta.parity();
    Interval I2 = (I - MPFloat(p)) * (MPFloat::sqrt2() / MPFloat(2));
    Interval J2 = (J - MPFloat(p)) * (MPFloat(-1) * MPFloat::sqrt2() / MPFloat(2));
    ZRootTwoGen sol = solve_ODGP(I2, J2);
    return map_zr(std::move(sol),
                  [p](const ZRootTwo& alpha) {
                      return alpha * ZRootTwo(0, 1) + ZRootTwo::from_int(p);
                  });
}

DRootTwoGen solve_scaled_ODGP(const Interval& I, const Interval& J, long k) {
    MPFloat scale = pow_sqrt2(k);
    Interval I2(I.l * scale, I.r * scale);
    Interval J2(J.l * scale, J.r * scale);
    if (k & 1) {
        J2 = -J2;
    }
    ZRootTwoGen sol = solve_ODGP(I2, J2);
    return map_zr_to_dr(std::move(sol),
                        [k](const ZRootTwo& alpha) { return DRootTwo(alpha, k); });
}

DRootTwoGen solve_scaled_ODGP_with_parity(const Interval& I, const Interval& J,
                                          long k, const DRootTwo& beta) {
    if (k == 0) {
        ZRootTwo b0 = beta.renew_denomexp(0).alpha();
        ZRootTwoGen sol = solve_ODGP_with_parity(I, J, b0);
        return map_zr_to_dr(std::move(sol),
                            [](const ZRootTwo& a) { return DRootTwo::from_zroottwo(a); });
    }
    int p = beta.renew_denomexp(k).parity();
    DRootTwo offset = (p == 0) ? DRootTwo::from_int(0) : DRootTwo::power_of_inv_sqrt2(k);
    Interval I2 = I - offset.to_real();
    Interval J2 = J - offset.conj_sq2().to_real();
    DRootTwoGen sol = solve_scaled_ODGP(I2, J2, k - 1);
    return map_dr(std::move(sol),
                  [offset](const DRootTwo& a) { return a + offset; });
}

}  // namespace cppgridsynth
