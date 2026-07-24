/****************************************************************************
  PackageName  [ qcir / scanning_gate_removal ]
  Synopsis     [ ScanningGateRemovalPass + multi-pass workflow. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./scanning_gate_removal.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <vector>

#include "convert/qcir_to_tensor.hpp"
#include "qcir/basic_gate_type.hpp"
#include "tensor/qfactor.hpp"
#include "tensor/tensor.hpp"

namespace qsyn::qcir {

namespace {

[[nodiscard]] QCir copy_without_gate(QCir const& circ, size_t skip_id) {
    QCir out{circ.get_num_qubits()};
    for (auto const* g : circ.get_gates()) {
        if (g->get_id() == skip_id) continue;
        out.append(g->get_operation(), g->get_qubits());
    }
    return out;
}

[[nodiscard]] bool is_removable_gate(QCirGate const& gate, ScanningGateRemovalOptions const& opt) {
    auto const& op = gate.get_operation();
    if (opt.remove_u3 && op.is<UGate>()) return true;
    if (opt.remove_cx && gate.get_num_qubits() >= 2) {
        auto const t = op.get_type();
        if (t.size() >= 2 && t[0] == 'c' && t[1] == 'x') return true;
    }
    return false;
}

}  // namespace

double unitary_residual(QCir const& trial, tensor::QTensor<double> const& target) {
    auto tens_opt = to_tensor(trial);
    if (!tens_opt.has_value()) return 1.0;
    auto tens = tens_opt->to_matrix();
    if (tens.shape() != target.shape()) return 1.0;
    return 1.0 - tensor::cosine_similarity(tens, target);
}

QCir scanning_gate_removal_pass(QCir const& compiled, tensor::QTensor<double> const& target_unitary,
                                ScanningGateRemovalOptions const& opt) {
    QCir cur = compiled;

    std::vector<size_t> gate_ids;
    gate_ids.reserve(cur.get_num_gates());
    for (auto const* g : cur.get_gates()) {
        gate_ids.push_back(g->get_id());
    }

    tensor::qfactor::QFactorOptions qopt;
    qopt.tolerance = opt.synthesis_epsilon;
    qopt.verbosity = 0;

    for (auto const gid : gate_ids) {
        QCirGate const* gate = nullptr;
        for (auto const* g : cur.get_gates()) {
            if (g->get_id() == gid) {
                gate = g;
                break;
            }
        }
        if (gate == nullptr || !is_removable_gate(*gate, opt)) continue;

        auto trial = copy_without_gate(cur, gid);
        if (trial.get_num_gates() == 0) continue;

        if (gate->get_operation().is<UGate>()) {
            auto res = tensor::qfactor::instantiate(trial, target_unitary, qopt);
            if (res.converged || res.final_residual <= opt.synthesis_epsilon) {
                spdlog::debug("scanning_gate_removal: removed U3 gate {}", gid);
                cur = std::move(trial);
            }
            continue;
        }

        if (!circuit_respects_coupling(trial, opt.coupling)) continue;

        auto const res = unitary_residual(trial, target_unitary);
        if (res <= opt.synthesis_epsilon) {
            spdlog::debug("scanning_gate_removal: removed CX gate {} (residual {:.3e})", gid, res);
            cur = std::move(trial);
        }
    }

    return cur;
}

QCir scanning_gate_removal_workflow(QCir const& compiled, tensor::QTensor<double> const& target_unitary,
                                    ScanningGateRemovalOptions const& opt) {
    QCir cur = compiled;
    for (int pass = 0; pass < opt.max_passes; ++pass) {
        auto const before = cur.get_num_gates();
        auto       next   = scanning_gate_removal_pass(cur, target_unitary, opt);
        if (next.get_num_gates() == before) break;
        cur = std::move(next);
    }
    cur.add_procedure("ScanningGateRemoval");
    return cur;
}

[[nodiscard]] QCir topology_align_cx(QCir const& circ, tensor::QTensor<double> const& target,
                                   ScanningGateRemovalOptions const& opt) {
    if (opt.coupling.all_to_all) return circ;

    QCir out{circ.get_num_qubits()};
    bool changed = false;

    for (auto const* g : circ.get_gates()) {
        if (g->get_num_qubits() >= 2) {
            auto const t = g->get_operation().get_type();
            if (t.size() >= 2 && t[0] == 'c' && t[1] == 'x') {
                auto qs = g->get_qubits();
                if (qs.size() >= 2 && !opt.coupling.allows_cx(qs[0], qs[1]) &&
                    opt.coupling.allows_cx(qs[1], qs[0])) {
                    QCir trial{circ.get_num_qubits()};
                    for (auto const* h : circ.get_gates()) {
                        if (h->get_id() == g->get_id()) {
                            trial.append(qcir::CXGate(), {qs[1], qs[0]});
                        } else {
                            trial.append(h->get_operation(), h->get_qubits());
                        }
                    }
                    if (unitary_residual(trial, target) <= opt.synthesis_epsilon) {
                        out.append(qcir::CXGate(), {qs[1], qs[0]});
                        changed = true;
                        continue;
                    }
                }
            }
        }
        out.append(g->get_operation(), g->get_qubits());
    }

    if (changed) out.add_procedure("TopologyAlign-CX");
    return changed ? out : circ;
}

QCir gate_deletion_optimization_workflow(QCir const& compiled,
                                         tensor::QTensor<double> const& target_unitary,
                                         ScanningGateRemovalOptions const& opt) {
    auto cur = scanning_gate_removal_workflow(compiled, target_unitary, opt);
    for (int pass = 0; pass < opt.max_passes; ++pass) {
        auto const before = cur.get_num_gates();
        cur               = scanning_gate_removal_pass(cur, target_unitary, opt);
        if (cur.get_num_gates() == before) break;
    }
    cur = topology_align_cx(cur, target_unitary, opt);
    cur.add_procedure("GateDeletion-Opt");
    return cur;
}

}  // namespace qsyn::qcir
