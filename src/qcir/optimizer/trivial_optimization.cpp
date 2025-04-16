/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Implement the trivial optimization ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <tl/to.hpp>
#include <tuple>

#include "../../convert/qcir_to_zxgraph.hpp"
#include "../qcir.hpp"
#include "../qcir_gate.hpp"
#include "./optimizer.hpp"
#include "extractor/extract.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/operation.hpp"
#include "zx/simplifier/simplify.hpp"

namespace qsyn::qcir {

/**
 * @brief Trivial optimization
 *
 * @return QCir*
 */
std::optional<QCir> Optimizer::trivial_optimization(QCir const& qcir) {
    spdlog::info("Start trivial optimization");

    reset(qcir);
    QCir result{qcir.get_num_qubits()};

    result.set_filename(qcir.get_filename());
    result.add_procedures(qcir.get_procedures());
    result.set_gate_set(qcir.get_gate_set());

    for (auto gate : qcir.get_gates()) {
        if (stop_requested()) {
            spdlog::warn("optimization interrupted");
            return std::nullopt;
        }
        auto const qubit         = gate->get_qubit(gate->get_num_qubits() - 1);
        auto const previous_gate = result.get_last_gate(qubit);
        if (previous_gate == nullptr) {
            result.append(*gate);
            continue;
        }
        if ((previous_gate->get_operation() == CXGate() && gate->get_operation() == CXGate()) ||
            (previous_gate->get_operation() == CYGate() && gate->get_operation() == CYGate()) ||
            (previous_gate->get_operation().is<ECRGate>() && gate->get_operation().is<ECRGate>()) ||
            (previous_gate->get_operation().is<HGate>() && gate->get_operation().is<HGate>())) {
            if (previous_gate->get_qubits() == gate->get_qubits()) {
                result.remove_gate(previous_gate->get_id());
            } else {
                result.append(*gate);
            }
        } else if ((previous_gate->get_operation() == CZGate() && gate->get_operation() == CZGate()) ||
                   (previous_gate->get_operation() == SwapGate() && gate->get_operation() == SwapGate())) {
            if ((previous_gate->get_qubit(0) == gate->get_qubit(0) &&
                 previous_gate->get_qubit(1) == gate->get_qubit(1)) ||
                (previous_gate->get_qubit(0) == gate->get_qubit(1) &&
                 previous_gate->get_qubit(1) == gate->get_qubit(0))) {
                result.remove_gate(previous_gate->get_id());
            } else {
                result.append(*gate);
            }
        } else if (is_single_z_rotation(*gate) && is_single_z_rotation(*previous_gate)) {
            _fuse_z_phase(result, previous_gate, gate);
        } else if (is_single_x_rotation(*gate) && is_single_x_rotation(*previous_gate)) {
            _fuse_x_phase(result, previous_gate, gate);
        } else {
            result.append(*gate);
        }
    }

    if (!result.get_gate_set().empty()) {
        _partial_zx_optimization(result);
    }

    spdlog::info("Finished trivial optimization");
    return result;
}

/**
 * @brief Fuse the incoming XPhase gate with the last layer in circuit
 * @param QC: the circuit
 * @param previousGate: previous gate
 * @param gate: the incoming gate
 *
 * @return modified circuit
 */
void Optimizer::_fuse_x_phase(QCir& qcir, QCirGate* prev_gate, QCirGate* gate) {
    auto const get_underlying_phase = [](QCirGate const& gate) {
        if (gate.get_operation().is<PXGate>()) {
            return gate.get_operation().get_underlying<PXGate>().get_phase();
        }
        if (gate.get_operation().is<RXGate>()) {
            return gate.get_operation().get_underlying<RXGate>().get_phase();
        }
        throw std::runtime_error("Invalid gate type");
    };

    auto const phase = get_underlying_phase(*prev_gate) + get_underlying_phase(*gate);
    if (phase == dvlab::Phase(0)) {
        qcir.remove_gate(prev_gate->get_id());
        return;
    }

    prev_gate->set_operation(PXGate(phase));
}

/**
 * @brief Fuse the incoming ZPhase gate with the last layer in circuit
 * @param QC: the circuit
 * @param previousGate: previous gate
 * @param gate: the incoming gate
 *
 * @return modified circuit
 */
void Optimizer::_fuse_z_phase(QCir& qcir, QCirGate* prev_gate, QCirGate* gate) {
    auto const get_underlying_phase = [](QCirGate const& gate) {
        if (gate.get_operation().is<PZGate>()) {
            return gate.get_operation().get_underlying<PZGate>().get_phase();
        }
        if (gate.get_operation().is<RZGate>()) {
            return gate.get_operation().get_underlying<RZGate>().get_phase();
        }
        throw std::runtime_error("Invalid gate type");
    };

    auto const phase = get_underlying_phase(*prev_gate) + get_underlying_phase(*gate);
    if (phase == dvlab::Phase(0)) {
        qcir.remove_gate(prev_gate->get_id());
        return;
    }
    prev_gate->set_operation(PZGate(phase));
}

namespace {

size_t match_gate_sequence(std::vector<Operation> const& type_seq,
                           std::vector<Operation> const& target_seq) {
    if (type_seq.size() < target_seq.size()) {
        return type_seq.size();
    }

    for (size_t i = 0; i < type_seq.size() - target_seq.size() + 1; i++) {
        bool match = true;
        for (size_t j = 0; j < target_seq.size(); j++) {
            if (type_seq[i + j] != target_seq[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return i;
        }
    }
    return type_seq.size();
}

QCir replace_single_qubit_gate_sequence(QCir& qcir, QubitIdType qubit, size_t gate_num,
                                        size_t seq_len, std::vector<Operation> const& seq) {
    QCir replaced;
    replaced.add_procedures(qcir.get_procedures());
    replaced.add_qubits(qcir.get_num_qubits());
    replaced.set_gate_set(qcir.get_gate_set());

    auto const& gate_list = qcir.get_gates();
    size_t replace_count  = 0;

    if (gate_num == 0) {
        replace_count = seq_len;
    }

    for (auto gate : gate_list) {
        if (gate->get_qubit(0) != qubit) {
            replaced.append(*gate);
            continue;
        }

        if (gate_num != 0) {
            replaced.append(*gate);
            gate_num--;
            if (gate_num == 0) {
                replace_count = seq_len;
            }
            continue;
        }

        if (replace_count == 0) {
            replaced.append(*gate);
            continue;
        }

        if (replace_count == seq_len) {
            for (auto& op : seq) {
                replaced.append(op, gate->get_qubits());
            }
            replace_count--;
            continue;
        }
        replace_count--;
    }

    return replaced;
}

std::vector<Operation> zx_optimize(std::vector<qcir::Operation> const& partial) {
    QCir qcir(1);

    for (auto const& op : partial) {
        qcir.append(op, {0});
    }

    auto zx = to_zxgraph(qcir).value();

    zx::simplify::full_reduce(zx);

    constexpr extractor::ExtractorConfig config{
        .sort_frontier        = false,
        .sort_neighbors       = false,
        .permute_qubits       = false,
        .filter_duplicate_cxs = false,
        .reduce_czs           = false,
        .dynamic_order        = false,
        .block_size           = 1,
        .optimize_level       = 0,
        .pred_coeff           = 0.7,
    };

    extractor::Extractor ext(&zx, config, nullptr, false);
    QCir* result = ext.extract();

    auto const gate_list = result->get_gates();
    std::vector<Operation> opt_partial;
    opt_partial.reserve(gate_list.size());
    for (auto gate : gate_list) {
        opt_partial.emplace_back(gate->get_operation());
    }

    return opt_partial;
}

}  // namespace

void Optimizer::_partial_zx_optimization(QCir& qcir) {
    for (auto const qubit : std::views::iota(0ul, qcir.get_num_qubits())) {
        auto const get_type_sequence = [](auto const& gate_list, QubitIdType qubit) {
            return gate_list |
                   std::views::filter([qubit](auto gate) {
                       return dvlab::contains(gate->get_qubits(), qubit);
                   }) |
                   std::views::transform([](auto gate) {
                       return gate->get_operation();
                   }) |
                   tl::to<std::vector>();
        };

        auto op_seq = get_type_sequence(qcir.get_gates(), qubit);

        std::vector<std::pair<std::vector<Operation>, std::vector<Operation>>> replacements;
        while (!op_seq.empty()) {
            std::vector<Operation> partial;

            while (!op_seq.empty()) {
                auto const op = op_seq[0];
                op_seq.erase(op_seq.begin());
                if (op.get_num_qubits() > 1) {
                    break;
                }
                partial.emplace_back(op);
            }

            if (partial.size() >= 3) {
                auto opt_partial = zx_optimize(partial);
                std::vector<Operation> replaced_h_opt_partial;
                for (auto const& g : opt_partial) {
                    if (g.is<HGate>()) {
                        replaced_h_opt_partial.emplace_back(SGate());
                        replaced_h_opt_partial.emplace_back(SXGate());
                        replaced_h_opt_partial.emplace_back(SGate());
                    } else {
                        replaced_h_opt_partial.emplace_back(g);
                    }
                }
                if (replaced_h_opt_partial.size() < partial.size()) {
                    replacements.emplace_back(std::make_pair(partial, replaced_h_opt_partial));
                }
            }
        }

        for (auto const& [lhs, rhs] : replacements) {
            auto const updated_type_seq =
                get_type_sequence(qcir.get_gates(), qubit);

            size_t const g = match_gate_sequence(updated_type_seq, lhs);
            qcir           = replace_single_qubit_gate_sequence(qcir, qubit, g, lhs.size(), rhs);
        }
    }
}

}  // namespace qsyn::qcir
