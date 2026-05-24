// SPDX-License-Identifier: MIT
#include "cppgridsynth/to_upright.hpp"

#include <cmath>
#include <iostream>

#include "cppgridsynth/mymath.hpp"

namespace cppgridsynth {

namespace {

EllipsePair shift_ellipse_pair(EllipsePair ep, long n) {
    ZRootTwo lambda_n = LAMBDA().pow(n);
    ZRootTwo lambda_inv_n = LAMBDA().pow(-n);
    MPFloat ln = lambda_n.to_real();
    MPFloat lin = lambda_inv_n.to_real();
    ep.A.a() *= lin;
    ep.A.d() *= ln;
    ep.B.a() *= ln;
    ep.B.d() *= lin;
    if (n & 1) ep.B.b() = -ep.B.b();
    return ep;
}

struct StepResult {
    EllipsePair ep;
    GridOp opG_l;
    GridOp opG_r;
    bool done;
};

StepResult reduction(EllipsePair ep, GridOp opG_l, GridOp opG_r, GridOp new_opG) {
    Ellipse newA = ep.A.transform(new_opG);
    Ellipse newB = ep.B.transform(new_opG.conj_sq2());
    return {EllipsePair(std::move(newA), std::move(newB)),
            std::move(opG_l),
            new_opG * opG_r,
            false};
}

StepResult step_lemma(EllipsePair ep, GridOp opG_l, GridOp opG_r, int /*verbose*/) {
    const Ellipse& A = ep.A;
    const Ellipse& B = ep.B;

    if (B.b() < 0) {
        GridOp Z(ZOmega(0, 0, 0, 1), ZOmega(0, -1, 0, 0));
        return reduction(std::move(ep), std::move(opG_l), std::move(opG_r), Z);
    }
    if (A.bias() * B.bias() < MPFloat(1)) {
        GridOp X(ZOmega(0, 1, 0, 0), ZOmega(0, 0, 0, 1));
        return reduction(std::move(ep), std::move(opG_l), std::move(opG_r), X);
    }
    if (ep.bias() > MPFloat("33.971") || ep.bias() < MPFloat("0.029437")) {
        MPFloat n_mpf = log(ep.bias()) / log(LAMBDA().to_real()) / MPFloat(8);
        long n = static_cast<long>(std::round(n_mpf.to_double()));
        GridOp S(ZOmega(-1, 0, 1, 1), ZOmega(1, -1, 1, 0));
        return reduction(std::move(ep), std::move(opG_l), std::move(opG_r), S.pow(n));
    }
    if (ep.skew() <= MPFloat(15)) {
        return {std::move(ep), std::move(opG_l), std::move(opG_r), true};
    }
    if (ep.bias() > MPFloat("5.8285") || ep.bias() < MPFloat("0.17157")) {
        MPFloat n_mpf = log(ep.bias()) / log(LAMBDA().to_real()) / MPFloat(4);
        long n = static_cast<long>(std::round(n_mpf.to_double()));
        EllipsePair shifted = shift_ellipse_pair(std::move(ep), n);
        GridOp OP_SIGMA_L, OP_SIGMA_R;
        if (n >= 0) {
            OP_SIGMA_L = GridOp(ZOmega(-1, 0, 1, 1), ZOmega(0, 1, 0, 0)).pow(n);
            OP_SIGMA_R = GridOp(ZOmega(0, 0, 0, 1), ZOmega(1, -1, 1, 0)).pow(n);
        } else {
            OP_SIGMA_L = GridOp(ZOmega(-1, 0, 1, -1), ZOmega(0, 1, 0, 0)).pow(-n);
            OP_SIGMA_R = GridOp(ZOmega(0, 0, 0, 1), ZOmega(1, 1, 1, 0)).pow(-n);
        }
        return {std::move(shifted), opG_l * OP_SIGMA_L, OP_SIGMA_R * opG_r, false};
    }
    if (MPFloat("0.24410") <= A.bias() && A.bias() <= MPFloat("4.0968") &&
        MPFloat("0.24410") <= B.bias() && B.bias() <= MPFloat("4.0968")) {
        GridOp R(ZOmega(0, 0, 1, 0), ZOmega(1, 0, 0, 0));
        return reduction(std::move(ep), std::move(opG_l), std::move(opG_r), R);
    }
    if (A.b() >= 0 && A.bias() <= MPFloat("1.6969")) {
        GridOp K(ZOmega(-1, -1, 0, 0), ZOmega(0, -1, 1, 0));
        return reduction(std::move(ep), std::move(opG_l), std::move(opG_r), K);
    }
    if (A.b() >= 0 && B.bias() <= MPFloat("1.6969")) {
        GridOp Kc(ZOmega(1, -1, 0, 0), ZOmega(0, -1, -1, 0));
        return reduction(std::move(ep), std::move(opG_l), std::move(opG_r), Kc);
    }
    if (A.b() >= 0) {
        MPFloat min_bias = A.bias();
        if (B.bias() < min_bias) min_bias = B.bias();
        long n = 1;
        mpz_class q = floorsqrt(min_bias / MPFloat(4));
        if (q.fits_slong_p()) n = std::max<long>(1, q.get_si());
        GridOp An(ZOmega(0, 0, 0, 1), ZOmega(0, 1, 0, 2 * n));
        return reduction(std::move(ep), std::move(opG_l), std::move(opG_r), An);
    }
    {
        MPFloat min_bias = A.bias();
        if (B.bias() < min_bias) min_bias = B.bias();
        long n = 1;
        mpz_class q = floorsqrt(min_bias / MPFloat(2));
        if (q.fits_slong_p()) n = std::max<long>(1, q.get_si());
        GridOp Bn(ZOmega(0, 0, 0, 1), ZOmega(n, 1, -n, 0));
        return reduction(std::move(ep), std::move(opG_l), std::move(opG_r), Bn);
    }
}

}  // namespace

GridOp to_upright_ellipse_pair(const Ellipse& ellipseA,
                               const Ellipse& ellipseB,
                               int verbose) {
    EllipsePair ep(ellipseA.normalize(), ellipseB.normalize());
    GridOp OP_I;
    GridOp opG_l = OP_I, opG_r = OP_I;

    while (true) {
        auto r = step_lemma(std::move(ep), std::move(opG_l), std::move(opG_r), verbose);
        ep = std::move(r.ep);
        opG_l = std::move(r.opG_l);
        opG_r = std::move(r.opG_r);
        if (r.done) break;
    }
    return opG_l * opG_r;
}

UprightResult to_upright_set_pair(const ConvexSet& setA,
                                  const ConvexSet& setB,
                                  std::optional<GridOp> opG_in,
                                  int verbose) {
    GridOp opG = opG_in ? *opG_in
                        : to_upright_ellipse_pair(setA.ellipse(), setB.ellipse(), verbose);
    Ellipse A_upright = setA.ellipse().transform(opG);
    Ellipse B_upright = setB.ellipse().transform(opG.conj_sq2());
    Rectangle bboxA = A_upright.bbox();
    Rectangle bboxB = B_upright.bbox();
    return {opG, A_upright, B_upright, bboxA, bboxB};
}

}  // namespace cppgridsynth
