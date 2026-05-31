#include "qcir/gridsynth/gridsynth_pass.hpp"

#include <spdlog/spdlog.h>

#include "qcir/basic_gate_type.hpp"
#include "qcir/gridsynth/gridsynth_adapter.hpp"

namespace qsyn::qcir {

namespace {

std::optional<Operation> operation_from_synth(gridsynth_detail::SynthGateKind kind) {
    using gridsynth_detail::SynthGateKind;
    switch (kind) {
        case SynthGateKind::H:
            return HGate();
        case SynthGateKind::T:
            return TGate();
        case SynthGateKind::S:
            return SGate();
        case SynthGateKind::X:
            return XGate();
        case SynthGateKind::W:
            // Global phase only; omit from QCir (unitary unchanged up to global phase).
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

gridsynth_detail::GridsynthRequest make_request(dvlab::Phase const& phase,
                                                GridsynthOptions const& opts) {
    auto const& r = phase.get_rational();
    gridsynth_detail::GridsynthRequest req;
    req.theta_numer = std::to_string(r.numerator());
    req.theta_denom = std::to_string(r.denominator());
    req.epsilon     = opts.epsilon;
    req.seed        = opts.seed;
    req.dloop       = opts.dloop;
    req.floop       = opts.floop;
    req.dtimeout_ms = opts.dtimeout_ms;
    req.ftimeout_ms = opts.ftimeout_ms;
    req.verbose     = opts.verbose;
    req.dps         = opts.dps;
    return req;
}

}  // namespace

std::optional<QCir> gridsynth_decompose(QCir const& qcir,
                                        GridsynthOptions const& opts) {
    if (opts.epsilon.empty()) {
        spdlog::error("GridSynth: --epsilon is required");
        return std::nullopt;
    }

    QCir result{qcir.get_num_qubits()};
    result.add_procedures(qcir.get_procedures());
    if (auto const& gs = qcir.get_gate_set(); !gs.empty()) {
        result.set_gate_set(gs);
    }

    size_t replaced    = 0;
    size_t skipped_crz = 0;

    for (auto const& gate : qcir.get_gates()) {
        auto const& op = gate->get_operation();

        if (auto rz = op.get_underlying_if<RZGate>()) {
            auto const wire = gate->get_qubit(0);
            auto request    = make_request(rz->get_phase(), opts);

            std::optional<std::vector<gridsynth_detail::SynthGate>> synth;
            try {
                synth = gridsynth_detail::synthesize_rz(request);
            } catch (std::exception const& ex) {
                spdlog::error("GridSynth failed on gate {} ({}): {}",
                              gate->get_id(),
                              op.get_repr(),
                              ex.what());
                return std::nullopt;
            }

            if (!synth.has_value()) {
                spdlog::error("GridSynth failed on gate {}", gate->get_id());
                return std::nullopt;
            }

            for (auto const& g : *synth) {
                auto qsyn_op = operation_from_synth(g.kind);
                if (!qsyn_op.has_value()) {
                    continue;
                }
                result.append(*qsyn_op, {wire});
            }
            ++replaced;
            continue;
        }

        if (op.is<ControlGate>()) {
            auto target = op.get_underlying<ControlGate>().get_target_operation();
            if (target.is<RZGate>()) {
                ++skipped_crz;
                spdlog::warn(
                    "GridSynth: gate {} ({}) left unchanged; controlled-RZ is "
                    "not decomposed (v1: rz only), so the output is not fully "
                    "Clifford+T",
                    gate->get_id(),
                    op.get_repr());
            }
        }

        result.append(op, gate->get_qubits());
    }

    if (skipped_crz > 0) {
        spdlog::warn(
            "GridSynth: {} controlled-RZ gate(s) unchanged (v1: rz only)",
            skipped_crz);
    }
    spdlog::info("GridSynth: decomposed {} RZ gate(s)", replaced);
    return result;
}

}  // namespace qsyn::qcir
