// SPDX-License-Identifier: MIT
#include "cppgridsynth/tdgp.hpp"

#include <memory>
#include <stdexcept>

#include "cppgridsynth/mymath.hpp"
#include "cppgridsynth/odgp.hpp"

namespace cppgridsynth {


DOmegaGen solve_TDGP(const ConvexSet& setA,
                     const ConvexSet& setB,
                     const GridOp& opG,
                     const Ellipse& /*ellipseA_upright*/,
                     const Ellipse& /*ellipseB_upright*/,
                     const Rectangle& bboxA,
                     const Rectangle& bboxB,
                     long k,
                     int /*verbose*/) {
    auto opG_inv = opG.inv();
    if (!opG_inv) {
        throw std::runtime_error("solve_TDGP: opG has no inverse");
    }

    // Establish the x-axis "outer" generator (must produce ≥ 1 solution).
    DRootTwoGen sol_x = solve_scaled_ODGP(bboxA.I_x, bboxB.I_x, k + 1);
    auto first = sol_x();
    if (!first) {
        return []() -> std::optional<DOmega> { return std::nullopt; };
    }
    DRootTwo alpha0 = *first;

    // y-axis solver, slightly fattened.
    Interval Iy_fat = bboxA.I_y.fatten(bboxA.I_y.width() * MPFloat("1e-4"));
    Interval Jy_fat = bboxB.I_y.fatten(bboxB.I_y.width() * MPFloat("1e-4"));
    DRootTwoGen sol_y = solve_scaled_ODGP(Iy_fat, Jy_fat, k + 1);

    // Captured state for the streaming TDGP solver.
    struct State {
        DRootTwoGen sol_y;
        // Inner generator yielding sol_x for current beta (DRootTwo).
        DRootTwoGen inner_x;
        bool inner_active;
        DRootTwo cur_beta;
        DRootTwo alpha0;
        const ConvexSet* setA;
        const ConvexSet* setB;
        GridOp opG;
        GridOp opG_inv;
        long k;
    };
    auto st = std::make_shared<State>();
    st->sol_y = std::move(sol_y);
    st->inner_active = false;
    st->alpha0 = alpha0;
    st->setA = &setA;
    st->setB = &setB;
    st->opG = opG;
    st->opG_inv = *opG_inv;
    st->k = k;
    st->cur_beta = DRootTwo();

    return [st]() mutable -> std::optional<DOmega> {
        while (true) {
            if (!st->inner_active) {
                auto beta_opt = st->sol_y();
                if (!beta_opt) return std::nullopt;
                st->cur_beta = *beta_opt;

                DRootTwo dx = DRootTwo::power_of_inv_sqrt2(st->k);
                DOmega z0 = st->opG_inv * DOmega::from_droottwo_vector(
                                st->alpha0, st->cur_beta, st->k + 1);
                DOmega v = st->opG_inv * DOmega::from_droottwo_vector(
                                dx, DRootTwo::from_int(0), st->k);
                auto t_A = st->setA->intersect(z0, v);
                auto t_B = st->setB->intersect(z0.conj_sq2(), v.conj_sq2());
                if (!t_A || !t_B) {
                    st->inner_active = false;
                    continue;
                }
                DRootTwo parity =
                    (st->cur_beta - st->alpha0).mul_by_sqrt2_power_renewing_denomexp(st->k);
                Interval intA(t_A->first, t_A->second);
                Interval intB(t_B->first, t_B->second);
                MPFloat width_b = intB.width();
                MPFloat width_a = intA.width();
                MPFloat scale = MPFloat(mpz_class(1) << static_cast<unsigned long>(st->k));
                MPFloat ten(10);
                MPFloat denomA = scale * width_b;
                MPFloat denomB = scale * width_a;
                if (denomA < ten) denomA = ten;
                if (denomB < ten) denomB = ten;
                MPFloat dtA = ten / denomA;
                MPFloat dtB = ten / denomB;
                intA = intA.fatten(dtA);
                intB = intB.fatten(dtB);
                DRootTwoGen sol_t = solve_scaled_ODGP_with_parity(intA, intB, 1, parity);
                DRootTwo dx_keep = dx;
                DRootTwo a0_keep = st->alpha0;
                DRootTwoGen sol_x_inner =
                    [sol_t = std::make_shared<DRootTwoGen>(std::move(sol_t)),
                     dx_keep, a0_keep]() mutable -> std::optional<DRootTwo> {
                        auto t = (*sol_t)();
                        if (!t) return std::nullopt;
                        return *t * dx_keep + a0_keep;
                    };
                st->inner_x = std::move(sol_x_inner);
                st->inner_active = true;
            }

            auto alpha = st->inner_x();
            if (!alpha) {
                st->inner_active = false;
                continue;
            }

            DOmega z_pre = DOmega::from_droottwo_vector(*alpha, st->cur_beta, st->k);
            DOmega z_transformed = st->opG_inv * z_pre;
            // Filter: must be inside both convex sets.
            if (!st->setA->inside(z_transformed)) continue;
            if (!st->setB->inside(z_transformed.conj_sq2())) continue;
            return z_transformed;
        }
    };
}

}  // namespace cppgridsynth
