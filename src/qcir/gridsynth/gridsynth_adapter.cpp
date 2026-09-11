#include "qcir/gridsynth/gridsynth_adapter.hpp"

#include <cmath>
#include <stdexcept>

#include "cppgridsynth/gridsynth.hpp"
#include "cppgridsynth/mpfloat.hpp"
#include "cppgridsynth/quantum_gate.hpp"

namespace qsyn::qcir::gridsynth_detail {

namespace {

SynthGateKind kind_from_cppgridsynth(cppgridsynth::GateKind k) {
    using cppgridsynth::GateKind;
    switch (k) {
        case GateKind::H:
            return SynthGateKind::H;
        case GateKind::T:
            return SynthGateKind::T;
        case GateKind::S:
            return SynthGateKind::S;
        case GateKind::X:
            return SynthGateKind::X;
        case GateKind::W:
            return SynthGateKind::W;
        default:
            throw std::runtime_error("unexpected gate in GridSynth output");
    }
}

}  // namespace

std::optional<std::vector<SynthGate>>
synthesize_rz(GridsynthRequest const& req) {
    if (req.dps && *req.dps <= 0) {
        throw std::invalid_argument("dps must be greater than zero");
    }
    if (req.dloop <= 0 || req.floop <= 0) {
        throw std::invalid_argument("loop limits must be greater than zero");
    }
    if (!std::isfinite(req.dtimeout_ms) || !std::isfinite(req.ftimeout_ms) ||
        req.dtimeout_ms == 0.0 || req.ftimeout_ms == 0.0) {
        throw std::invalid_argument(
            "timeouts must be finite and either negative (disabled) or greater than zero");
    }

    cppgridsynth::MPFloat const epsilon(req.epsilon);
    if (!epsilon.is_finite() || epsilon <= 0 || epsilon >= 2) {
        throw std::invalid_argument("epsilon must be a number strictly between zero and two");
    }

    cppgridsynth::GridsynthConfig cfg;
    cfg.seed        = req.seed;
    cfg.dloop       = req.dloop;
    cfg.floop       = req.floop;
    cfg.dtimeout_ms = req.dtimeout_ms;
    cfg.ftimeout_ms = req.ftimeout_ms;
    cfg.verbose     = req.verbose;
    if (req.dps) {
        cfg.dps = *req.dps;
    }

    cppgridsynth::MPFloat const pi = cppgridsynth::MPFloat::pi();
    cppgridsynth::MPFloat const theta =
        pi * cppgridsynth::MPFloat(req.theta_numer) /
        cppgridsynth::MPFloat(req.theta_denom);

    // Match pygridsynth/cppgridsynth gridsynth_gates(): no decompose_phase_gate().
    // Global phase stays in circuit.phase(); W gates in gates_ are skipped in qsyn.
    auto circuit =
        cppgridsynth::gridsynth_circuit(theta, epsilon, {0}, cfg);

    std::vector<SynthGate> out;
    out.reserve(circuit.size());
    for (auto const& g : circuit.gates()) {
        out.push_back({kind_from_cppgridsynth(g->kind())});
    }
    return out;
}

}  // namespace qsyn::qcir::gridsynth_detail
