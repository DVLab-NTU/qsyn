/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ BBMerge and FastTMerge on QCir ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./optimizer.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>


#include "qcir/basic_gate_type.hpp"
#include "qcir/operation.hpp"
#include "qcir/qcir.hpp"
#include "tableau/optimize/t_merge.hpp"
#include "util/phase.hpp"

namespace qsyn::qcir {

namespace {

size_t count_t_gates(QCir const& qcir) {
    size_t count = 0;
    for (auto const* gate : qcir.get_gates()) {
        auto const& op = gate->get_operation();
        if (op.is<PZGate>() && op.get_underlying<PZGate>().get_phase().denominator() == 4) {
            ++count;
        }
    }
    return count;
}

bool is_t_family_phase(dvlab::Phase const& phase) {
    return phase.denominator() == 4;
}

bool has_ccx_or_ccz(QCir const& qcir) {
    for (auto const* gate : qcir.get_gates()) {
        auto const& op = gate->get_operation();
        if (!op.is<ControlGate>()) {
            continue;
        }
        auto const& cg = op.get_underlying<ControlGate>();
        if (cg.get_num_ctrls() != 2) {
            continue;
        }
        auto const& target = cg.get_target_operation();
        if (target == XGate() || target == ZGate()) {
            return true;
        }
    }
    return false;
}

char const* cc_decomposition_label(CcDecomposition method) {
    return method == CcDecomposition::Cpp ? "C++" : "Rust";
}

std::optional<experimental::TMergeGate> operation_to_t_merge_gate(
    Operation const& op,
    QubitIdList const& qubits) {
    using Kind = experimental::TMergeGate::Kind;

    if (op.is<HGate>()) {
        return experimental::TMergeGate{Kind::h, static_cast<size_t>(qubits[0]), 0};
    }
    if (op == XGate()) {
        return experimental::TMergeGate{Kind::x, static_cast<size_t>(qubits[0]), 0};
    }
    if (op == ZGate()) {
        return experimental::TMergeGate{Kind::z, static_cast<size_t>(qubits[0]), 0};
    }
    if (op == SdgGate()) {
        return experimental::TMergeGate{Kind::sdg, static_cast<size_t>(qubits[0]), 0};
    }
    if (op == SGate()) {
        return experimental::TMergeGate{Kind::s, static_cast<size_t>(qubits[0]), 0};
    }
    if (op.is<PZGate>()) {
        auto const& phase = op.get_underlying<PZGate>().get_phase();
        if (phase.denominator() == 2 && phase == dvlab::Phase(1, 2)) {
            return experimental::TMergeGate{Kind::s, static_cast<size_t>(qubits[0]), 0};
        }
        if (phase.denominator() == 2 && phase == dvlab::Phase(-1, 2)) {
            return experimental::TMergeGate{Kind::sdg, static_cast<size_t>(qubits[0]), 0};
        }
        if (is_t_family_phase(phase)) {
            return experimental::TMergeGate{
                Kind::t,
                static_cast<size_t>(qubits[0]),
                0,
                phase.numerator() < 0};
        }
    }
    if (op.is<ControlGate>()) {
        auto const& cg = op.get_underlying<ControlGate>();
        if (cg.get_num_ctrls() == 1 && cg.get_target_operation() == XGate()) {
            return experimental::TMergeGate{
                Kind::cx,
                static_cast<size_t>(qubits[0]),
                static_cast<size_t>(qubits[1])};
        }
        if (cg.get_num_ctrls() == 1 && cg.get_target_operation() == ZGate()) {
            return std::nullopt;  // handled in parse_for_t_merge via H-CX-H expansion
        }
    }

    spdlog::error("TMerge: unsupported gate {}", op.get_repr());
    return std::nullopt;
}

struct ParsedCircuit {
    std::vector<experimental::TMergeGate> gates;
    std::vector<size_t> t_gate_ids;
};

std::optional<ParsedCircuit> parse_for_t_merge(QCir const& qcir) {
    ParsedCircuit parsed;
    for (auto const* gate : qcir.get_gates()) {
        auto const& op     = gate->get_operation();
        auto const& qubits = gate->get_qubits();

        // Expand CZ to H-CX-H on target
        if (op.is<ControlGate>()) {
            auto const& cg = op.get_underlying<ControlGate>();
            if (cg.get_num_ctrls() == 1 && cg.get_target_operation() == ZGate()) {
                auto const ctrl   = static_cast<size_t>(qubits[0]);
                auto const target = static_cast<size_t>(qubits[1]);
                parsed.gates.push_back({experimental::TMergeGate::Kind::h, target, 0});
                parsed.gates.push_back({experimental::TMergeGate::Kind::cx, ctrl, target});
                parsed.gates.push_back({experimental::TMergeGate::Kind::h, target, 0});
                continue;
            }
        }

        auto mg = operation_to_t_merge_gate(op, qubits);
        if (!mg.has_value()) {
            return std::nullopt;
        }
        parsed.gates.push_back(*mg);
        if (mg->kind == experimental::TMergeGate::Kind::t) {
            parsed.t_gate_ids.push_back(gate->get_id());
        }
    }
    return parsed;
}

void apply_merge_plan(QCir& qcir, ParsedCircuit const& parsed, experimental::TMergePlan const& plan) {
    if (plan.t_outcomes.size() != parsed.t_gate_ids.size()) {
        spdlog::error("TMerge: internal error: plan size mismatch");
        return;
    }

    std::vector<size_t> to_remove;
    to_remove.reserve(parsed.t_gate_ids.size());

    for (size_t i = 0; i < parsed.t_gate_ids.size(); ++i) {
        auto const gate_id = parsed.t_gate_ids[i];
        switch (plan.t_outcomes[i]) {
            case experimental::TMergeOutcome::keep:
                break;
            case experimental::TMergeOutcome::remove:
                to_remove.push_back(gate_id);
                break;
            case experimental::TMergeOutcome::replace_with_s:
                qcir.get_gate(gate_id)->set_operation(SGate());
                break;
            case experimental::TMergeOutcome::replace_with_sdg:
                qcir.get_gate(gate_id)->set_operation(SdgGate());
                break;
        }
    }

    std::ranges::sort(to_remove, std::ranges::greater{});
    to_remove.erase(std::unique(to_remove.begin(), to_remove.end()), to_remove.end());
    for (auto const id : to_remove) {
        qcir.remove_gate(id);
    }
}

std::optional<QCir> run_t_merge_path(QCir qcir, bool fast_t_merge, CcDecomposition cc_decomp) {
    auto normalized = to_basic_gates(qcir, cc_decomp);
    if (!normalized.has_value()) {
        spdlog::error("TMerge: to_basic_gates failed; circuit may contain unsupported gates.");
        return std::nullopt;
    }

    auto parsed = parse_for_t_merge(*normalized);
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    auto const n_qubits = normalized->get_num_qubits();
    auto const rank     = experimental::rank_vector(parsed->gates, n_qubits);
    auto const plan     = fast_t_merge
                              ? experimental::fast_t_merge_decisions(parsed->gates, n_qubits, rank)
                              : experimental::bb_merge_decisions(parsed->gates, n_qubits, rank);

    apply_merge_plan(*normalized, *parsed, plan);
    return normalized;
}

void t_merge_impl(QCir& qcir, bool fast_t_merge, CcDecomposition cc_decomp) {
    auto const t_before = count_t_gates(qcir);

    auto merged = run_t_merge_path(qcir, fast_t_merge, cc_decomp);
    if (!merged.has_value()) {
        return;
    }

    qcir = std::move(*merged);

    auto const t_after = count_t_gates(qcir);
    spdlog::info(
        "{}: T count{} ({} CCX/CCZ decompose)",
        fast_t_merge ? "FastTMerge" : "BBMerge",
        t_after,
        cc_decomposition_label(cc_decomp));
}

}  // namespace

void bb_merge(QCir& qcir) {
    t_merge_impl(qcir, false, CcDecomposition::Rust);
}

void fast_t_merge(QCir& qcir, std::optional<CcDecomposition> cc_decomp) {
    if (cc_decomp.has_value()) {
        t_merge_impl(qcir, true, *cc_decomp);
        return;
    }

    if (!has_ccx_or_ccz(qcir)) {
        t_merge_impl(qcir, true, CcDecomposition::Rust);
        return;
    }

    auto const rust_result = run_t_merge_path(qcir, true, CcDecomposition::Rust);
    auto const cpp_result  = run_t_merge_path(qcir, true, CcDecomposition::Cpp);

    if (!rust_result.has_value() && !cpp_result.has_value()) {
        return;
    }
    if (!rust_result.has_value()) {
        qcir = std::move(*cpp_result);
    } else if (!cpp_result.has_value()) {
        qcir = std::move(*rust_result);
    } else {
        auto const rust_t = count_t_gates(*rust_result);
        auto const cpp_t  = count_t_gates(*cpp_result);
        if (cpp_t <= rust_t) {
            spdlog::info("FastTMerge: CCX/CCZ decompose compare Rust T={} vs C++ T={} -> using C++", rust_t, cpp_t);
            qcir = std::move(*cpp_result);
        } else {
            spdlog::info("FastTMerge: CCX/CCZ decompose compare Rust T={} vs C++ T={} -> using Rust", rust_t, cpp_t);
            qcir = std::move(*rust_result);
        }
    }
}

}  // namespace qsyn::qcir
