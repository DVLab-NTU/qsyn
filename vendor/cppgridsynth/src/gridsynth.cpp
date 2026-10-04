// SPDX-License-Identifier: MIT
#include "cppgridsynth/gridsynth.hpp"

#include <chrono>
#include <iostream>
#include <utility>
#include <variant>

#include "cppgridsynth/diophantine.hpp"
#include "cppgridsynth/domega_unitary.hpp"
#include "cppgridsynth/grid_op.hpp"
#include "cppgridsynth/mymath.hpp"
#include "cppgridsynth/quantum_gate.hpp"
#include "cppgridsynth/synthesis_of_cliffordT.hpp"
#include "cppgridsynth/tdgp.hpp"
#include "cppgridsynth/to_upright.hpp"

namespace cppgridsynth {

// =====================================================================
//  EpsilonRegion
// =====================================================================
namespace {

Ellipse build_epsilon_ellipse(const MPFloat& theta, const MPFloat& epsilon,
                              const ZRootTwo& scale) {
    MPFloat z_x = cos(-theta / MPFloat(2));
    MPFloat z_y = sin(-theta / MPFloat(2));
    MPFloat scale_real = scale.to_real();
    MPFloat one(1);
    MPFloat eps_inv = one / epsilon;
    MPFloat eps_inv_2 = eps_inv * eps_inv;
    MPFloat eps_inv_4 = eps_inv_2 * eps_inv_2;
    MPFloat D2_aa = MPFloat(64) * eps_inv_4 / scale_real;
    MPFloat D2_dd = MPFloat(4)  * eps_inv_2 / scale_real;

    // D = D1 * D2 * D3, where D2 = diag(D2_aa, D2_dd) and D1, D3 are
    // rotations.  Computing the resulting symmetric 2x2 matrix:
    //   D1 = [[zx, -zy], [zy, zx]],  D3 = [[zx, zy], [-zy, zx]]
    //   D = D1 * diag(α, β) * D3
    //     = [[α zx² + β zy²,   (α-β) zx zy],
    //        [(α-β) zx zy,      α zy² + β zx²]]
    MPFloat A = D2_aa * z_x * z_x + D2_dd * z_y * z_y;
    MPFloat B = (D2_aa - D2_dd) * z_x * z_y;
    MPFloat D = D2_aa * z_y * z_y + D2_dd * z_x * z_x;

    MPFloat dist = sqrt(one - epsilon * epsilon / MPFloat(4)) * sqrt(scale_real);
    MPFloat px = dist * z_x;
    MPFloat py = dist * z_y;
    return Ellipse(A, B, D, px, py);
}

}  // namespace

EpsilonRegion::EpsilonRegion(const MPFloat& theta, const MPFloat& epsilon,
                             const ZRootTwo& scale)
    : ConvexSet(build_epsilon_ellipse(theta, epsilon, scale)),
      theta_(theta),
      epsilon_(epsilon),
      scale_(scale) {
    d_ = sqrt(MPFloat(1) - epsilon * epsilon / MPFloat(4)) * sqrt(scale.to_real());
    z_x_ = cos(-theta / MPFloat(2));
    z_y_ = sin(-theta / MPFloat(2));
}

bool EpsilonRegion::inside(const DOmega& u) const {
    MPFloat cos_sim = z_x_ * u.real() + z_y_ * u.imag();
    DRootTwo norm_sq = DRootTwo::from_domega(u.conj() * u);
    return norm_sq <= scale_ && cos_sim >= d_;
}

std::optional<std::pair<MPFloat, MPFloat>>
EpsilonRegion::intersect(const DOmega& u0, const DOmega& v) const {
    DOmega a = v.conj() * v;
    DOmega b = DOmega::from_int(2) * v.conj() * u0;
    DOmega c = u0.conj() * u0 - DOmega::from_zroottwo(scale_);

    MPFloat vz = z_x_ * v.real() + z_y_ * v.imag();
    MPFloat rhs = d_ - z_x_ * u0.real() - z_y_ * u0.imag();
    auto t = solve_quadratic(a.real(), b.real(), c.real());
    if (!t) return std::nullopt;
    MPFloat t0 = t->first, t1 = t->second;
    if (vz > 0) {
        MPFloat t2 = rhs / vz;
        return std::make_pair(t0 > t2 ? t0 : t2, t1);
    } else if (vz < 0) {
        MPFloat t2 = rhs / vz;
        return std::make_pair(t0, t1 < t2 ? t1 : t2);
    } else {
        if (rhs <= 0) return std::make_pair(t0, t1);
        return std::nullopt;
    }
}

// =====================================================================
//  UnitDisk
// =====================================================================
namespace {
Ellipse build_unit_disk_ellipse(const ZRootTwo& scale) {
    MPFloat s_inv = MPFloat(1) / scale.to_real();
    return Ellipse(s_inv, MPFloat(0), s_inv, MPFloat(0), MPFloat(0));
}
}

UnitDisk::UnitDisk(const ZRootTwo& scale)
    : ConvexSet(build_unit_disk_ellipse(scale)), scale_(scale) {}

bool UnitDisk::inside(const DOmega& u) const {
    DRootTwo norm_sq = DRootTwo::from_domega(u.conj() * u);
    return norm_sq <= scale_;
}

std::optional<std::pair<MPFloat, MPFloat>>
UnitDisk::intersect(const DOmega& u0, const DOmega& v) const {
    DOmega a = v.conj() * v;
    DOmega b = DOmega::from_int(2) * v.conj() * u0;
    DOmega c = u0.conj() * u0 - DOmega::from_zroottwo(scale_);
    return solve_quadratic(a.real(), b.real(), c.real());
}

// =====================================================================
//  Core algorithm.
// =====================================================================
namespace {

struct TDGPSets {
    const ConvexSet* setA;
    const ConvexSet* setB;
    GridOp opG;
    Ellipse A_upright;
    Ellipse B_upright;
    Rectangle bboxA;
    Rectangle bboxB;
};

struct TimeAndUnitary {
    std::optional<DOmegaUnitary> u;
    double tdgp_ms;
    double diop_ms;
};

TimeAndUnitary gridsynth_with_fixed_k(const TDGPSets& s, long k, bool has_phase,
                                      const GridsynthConfig& cfg,
                                      LoopController& lc) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();
    DOmegaGen sol = solve_TDGP(*s.setA, *s.setB, s.opG,
                                s.A_upright, s.B_upright,
                                s.bboxA, s.bboxB,
                                k, cfg.verbose);
    double tdgp_ms = 0.0;
    double diop_ms = 0.0;

    std::optional<DOmegaUnitary> u_approx;
    while (true) {
        auto z_opt = sol();
        if (!z_opt) break;
        DOmega z = *z_opt;
        if ((z * z.conj()).residue() == 0) continue;
        if (has_phase) z = z * DOmega(ZOmega(0, -1, 1, 0), 1);
        DRootTwo xi = DRootTwo::from_int(1) - DRootTwo::from_domega(z.conj() * z);
        auto t1 = clock::now();
        auto w_var = diophantine_dyadic(xi, cfg.seed, lc);
        auto t2 = clock::now();
        diop_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();
        if (std::holds_alternative<DOmega>(w_var)) {
            DOmega w = std::get<DOmega>(w_var);
            z = z.reduce_denomexp();
            w = w.reduce_denomexp();
            if (z.k() > w.k()) w = w.renew_denomexp(z.k());
            else if (z.k() < w.k()) z = z.renew_denomexp(w.k());

            long k1 = (z + w).reduce_denomexp().k();
            long k2 = (z + w.mul_by_omega()).reduce_denomexp().k();
            long k3 = (z + w.mul_by_omega_inv()).reduce_denomexp().k();
            if (has_phase) {
                if (k1 <= k2 && k1 <= k3) u_approx = DOmegaUnitary(z, w, -1);
                else                       u_approx = DOmegaUnitary(z, w.mul_by_omega_inv(), -1);
            } else {
                if (k1 <= k2) u_approx = DOmegaUnitary(z, w, 0);
                else          u_approx = DOmegaUnitary(z, w.mul_by_omega(), 0);
            }
            break;
        }
    }
    auto te = clock::now();
    tdgp_ms = std::chrono::duration<double, std::milli>(te - t0).count() - diop_ms;
    return {u_approx, tdgp_ms, diop_ms};
}

DOmegaUnitary gridsynth_exact(const MPFloat& theta, const MPFloat& epsilon,
                              GridsynthConfig& cfg) {
    EpsilonRegion eps_region(theta, epsilon);
    UnitDisk unit_disk;
    auto upright = to_upright_set_pair(eps_region, unit_disk, std::nullopt, cfg.verbose);

    TDGPSets s{&eps_region, &unit_disk,
               upright.opG, upright.ellipseA_upright, upright.ellipseB_upright,
               upright.bboxA, upright.bboxB};

    LoopController lc = cfg.make_loop_controller();
    long k = 0;
    while (true) {
        auto r = gridsynth_with_fixed_k(s, k, /*has_phase=*/false, cfg, lc);
        if (cfg.measure_time) {
            std::cout << "k=" << k << "  TDGP " << r.tdgp_ms << " ms  diop "
                      << r.diop_ms << " ms\n";
        }
        if (r.u) return *r.u;
        ++k;
    }
}

DOmegaUnitary gridsynth_up_to_phase(const MPFloat& theta, const MPFloat& epsilon,
                                    GridsynthConfig& cfg) {
    EpsilonRegion eps_region0(theta, epsilon);
    UnitDisk unit_disk0;
    EpsilonRegion eps_region1(theta, epsilon, ZRootTwo(2, 1));
    UnitDisk unit_disk1(ZRootTwo(2, -1));

    GridOp opG = to_upright_ellipse_pair(eps_region0.ellipse(),
                                         unit_disk0.ellipse(), cfg.verbose);
    auto u0 = to_upright_set_pair(eps_region0, unit_disk0, opG, cfg.verbose);
    auto u1 = to_upright_set_pair(eps_region1, unit_disk1, opG, cfg.verbose);

    TDGPSets s0{&eps_region0, &unit_disk0,
                u0.opG, u0.ellipseA_upright, u0.ellipseB_upright,
                u0.bboxA, u0.bboxB};
    TDGPSets s1{&eps_region1, &unit_disk1,
                u1.opG, u1.ellipseA_upright, u1.ellipseB_upright,
                u1.bboxA, u1.bboxB};

    LoopController lc = cfg.make_loop_controller();
    long k = 0;
    bool has_phase = false;
    while (true) {
        auto r = gridsynth_with_fixed_k(has_phase ? s1 : s0, k, has_phase, cfg, lc);
        if (cfg.measure_time) {
            std::cout << "k=" << k << " phase=" << has_phase
                      << "  TDGP " << r.tdgp_ms << " ms  diop "
                      << r.diop_ms << " ms\n";
        }
        if (r.u) return *r.u;
        if (k >= 2) {
            has_phase = !has_phase;
            if (has_phase) ++k;
        } else {
            if (k == 0 && !has_phase) { k = 1; has_phase = false; }
            else if (k == 1 && !has_phase) { k = 0; has_phase = true; }
            else if (k == 0 && has_phase)  { k = 1; has_phase = true; }
            else if (k == 1 && has_phase)  { k = 2; has_phase = false; }
        }
    }
}

}  // namespace

// =====================================================================
//  Public entry points.
// =====================================================================
DOmegaUnitary gridsynth(const MPFloat& theta, const MPFloat& epsilon,
                        GridsynthConfig cfg) {
    if (!cfg.dps) cfg.dps = dps_for_epsilon(epsilon);
    auto guard = work_dps(*cfg.dps);
    if (cfg.up_to_phase) return gridsynth_up_to_phase(theta, epsilon, cfg);
    return gridsynth_exact(theta, epsilon, cfg);
}

QuantumCircuit gridsynth_circuit(const MPFloat& theta, const MPFloat& epsilon,
                                 const std::vector<int>& wires,
                                 GridsynthConfig cfg) {
    if (!cfg.dps) cfg.dps = dps_for_epsilon(epsilon);
    auto guard = work_dps(*cfg.dps);

    DOmegaUnitary u_approx = gridsynth(theta, epsilon, cfg);
    QuantumCircuit circuit = decompose_domega_unitary(u_approx, wires, cfg.up_to_phase);

    if (u_approx.n() != 0) {
        MPFloat new_phase = circuit.phase() + MPFloat::pi() / MPFloat(8);
        circuit.set_phase(new_phase);
    }
    return circuit;
}

std::string gridsynth_gates(const MPFloat& theta, const MPFloat& epsilon,
                            GridsynthConfig cfg) {
    QuantumCircuit c = gridsynth_circuit(theta, epsilon, {0}, cfg);
    return c.to_simple_str();
}

DOmegaUnitary gridsynth(const std::string& theta_str, const std::string& epsilon_str,
                        GridsynthConfig cfg) {
    if (!cfg.dps) cfg.dps = dps_for_epsilon(MPFloat(epsilon_str));
    auto guard = work_dps(*cfg.dps);
    return gridsynth(MPFloat(theta_str), MPFloat(epsilon_str), cfg);
}

QuantumCircuit gridsynth_circuit(const std::string& theta_str,
                                 const std::string& epsilon_str,
                                 const std::vector<int>& wires,
                                 GridsynthConfig cfg) {
    if (!cfg.dps) cfg.dps = dps_for_epsilon(MPFloat(epsilon_str));
    auto guard = work_dps(*cfg.dps);
    return gridsynth_circuit(MPFloat(theta_str), MPFloat(epsilon_str), wires, cfg);
}

std::string gridsynth_gates(const std::string& theta_str,
                            const std::string& epsilon_str,
                            GridsynthConfig cfg) {
    if (!cfg.dps) cfg.dps = dps_for_epsilon(MPFloat(epsilon_str));
    auto guard = work_dps(*cfg.dps);
    return gridsynth_gates(MPFloat(theta_str), MPFloat(epsilon_str), cfg);
}

namespace {

MPComplex doomega_to_mpcomplex(const DOmega& u) {
    return MPComplex(u.real(), u.imag());
}

}  // namespace

MPFloat gridsynth_error(const MPFloat& theta,
                        const std::string& gates,
                        MPFloat phase) {
    CMatrix target = Rz_matrix(theta);
    DOmegaUnitary u_approx = DOmegaUnitary::from_gates(gates);
    MPComplex approx00 =
        MPComplex::from_phase(phase) *
        doomega_to_mpcomplex(u_approx.to_matrix()[0][0]);

    MPComplex prod = target(0, 0).conj() * approx00;
    MPFloat inner = prod.re;
    MPFloat val = MPFloat(1) - inner * inner;
    if (val < MPFloat(0)) val = MPFloat(0);
    return MPFloat(2) * sqrt(val);
}

}  // namespace cppgridsynth
