// SPDX-License-Identifier: MIT
//
// Top-level gridsynth pipeline: angle/epsilon → DOmegaUnitary → gate string.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cppgridsynth/domega_unitary.hpp"
#include "cppgridsynth/loop_controller.hpp"
#include "cppgridsynth/quantum_circuit.hpp"
#include "cppgridsynth/region.hpp"
#include "cppgridsynth/ring.hpp"

namespace cppgridsynth {

struct GridsynthConfig {
    std::optional<int> dps;     // mpmath-style decimal places; auto-derived if unset
    int seed = 0;
    int dloop = 10;
    int floop = 10;
    double dtimeout_ms = -1.0;  // <0 ⇒ no timeout
    double ftimeout_ms = -1.0;
    int verbose = 0;
    bool measure_time = false;
    bool up_to_phase = false;

    // Reified loop controller (resolved from {d,f}{loop, timeout}).
    LoopController make_loop_controller() const {
        double dt = dtimeout_ms < 0 ? std::numeric_limits<double>::infinity() : dtimeout_ms;
        double ft = ftimeout_ms < 0 ? std::numeric_limits<double>::infinity() : ftimeout_ms;
        return LoopController(dloop, floop, dt, ft);
    }
};

// =====================================================================
//  Convex-set bodies used by the algorithm.
// =====================================================================
class EpsilonRegion final : public ConvexSet {
public:
    EpsilonRegion(const MPFloat& theta, const MPFloat& epsilon,
                  const ZRootTwo& scale = ZRootTwo(1, 0));

    bool inside(const DOmega& u) const override;
    std::optional<std::pair<MPFloat, MPFloat>> intersect(const DOmega& u0,
                                                         const DOmega& v) const override;

private:
    MPFloat theta_;
    MPFloat epsilon_;
    ZRootTwo scale_;
    MPFloat d_;
    MPFloat z_x_;
    MPFloat z_y_;
};

class UnitDisk final : public ConvexSet {
public:
    explicit UnitDisk(const ZRootTwo& scale = ZRootTwo(1, 0));
    bool inside(const DOmega& u) const override;
    std::optional<std::pair<MPFloat, MPFloat>> intersect(const DOmega& u0,
                                                         const DOmega& v) const override;
private:
    ZRootTwo scale_;
};

// =====================================================================
//  Public entry points.
// =====================================================================
DOmegaUnitary gridsynth(const MPFloat& theta,
                        const MPFloat& epsilon,
                        GridsynthConfig cfg);

QuantumCircuit gridsynth_circuit(const MPFloat& theta,
                                 const MPFloat& epsilon,
                                 const std::vector<int>& wires,
                                 GridsynthConfig cfg);

std::string gridsynth_gates(const MPFloat& theta,
                            const MPFloat& epsilon,
                            GridsynthConfig cfg);

// String / numeric input convenience overloads (parse with MPFloat).
DOmegaUnitary  gridsynth(const std::string& theta_str,
                         const std::string& epsilon_str,
                         GridsynthConfig cfg);
QuantumCircuit gridsynth_circuit(const std::string& theta_str,
                                 const std::string& epsilon_str,
                                 const std::vector<int>& wires,
                                 GridsynthConfig cfg);
std::string    gridsynth_gates(const std::string& theta_str,
                               const std::string& epsilon_str,
                               GridsynthConfig cfg);

// Distance from Rz(theta) to the unitary encoded by `gates` (same metric as
// pygridsynth.gridsynth.error).  Caller should set working precision via
// work_dps() when evaluating at high dps.
MPFloat gridsynth_error(const MPFloat& theta,
                        const std::string& gates,
                        MPFloat phase = MPFloat());

}  // namespace cppgridsynth
