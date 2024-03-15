/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Implement the trivial optimization ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cassert>
#include <tl/to.hpp>
#include <tuple>

#include "../../convert/qcir_to_zxgraph.hpp"
#include "../qcir.hpp"
#include "../qcir_gate.hpp"
#include "./optimizer.hpp"
#include "extractor/extract.hpp"
#include "qcir/gate_type.hpp"

extern bool stop_requested();

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

    auto gate_list = qcir.get_gates();
    for (auto gate : gate_list) {
        if (stop_requested()) {
            spdlog::warn("optimization interrupted");
            return std::nullopt;
        }
        auto const last_layer = _get_first_layer_gates(result, true);
        auto const qubit      = gate->get_qubit(gate->get_num_qubits() - 1);
        if (last_layer[qubit] == nullptr) {
            result.append(gate->get_operation(), gate->get_qubits());
            continue;
        }
        QCirGate* previous_gate = last_layer[qubit];
        if (is_cx_or_cz_gate(gate)) {
            auto const q2 = gate->get_qubit(gate->get_num_qubits() - 1);
            if (previous_gate->get_id() != last_layer[q2]->get_id()) {
                // 2-qubit gate do not match up
                result.append(previous_gate->get_operation(), previous_gate->get_qubits());
                continue;
            }
            _cancel_cx_or_cz(result, previous_gate, gate);
        } else if (is_single_z_rotation(gate) && is_single_z_rotation(previous_gate)) {
            _fuse_z_phase(result, previous_gate, gate);
        } else if (is_single_x_rotation(gate) && is_single_x_rotation(previous_gate)) {
            _fuse_x_phase(result, previous_gate, gate);
        } else {
            result.append(gate->get_operation(), gate->get_qubits());
        }
    }

    if (!result.get_gate_set().empty()) {
        _partial_zx_optimization(result);
    }

    spdlog::info("Finished trivial optimization");
    return result;
}

/**
 * @brief Get the first layer of the circuit (nullptr if the qubit is empty at the layer)
 * @param QC: the input circuit
 * @param fromLast: Get the last layer
 *
 * @return vector<QCirGate*> with size = circuit->getNqubit()
 */
std::vector<QCirGate*> Optimizer::_get_first_layer_gates(QCir& qcir, bool from_last) {
    std::vector<QCirGate*> gate_list = qcir.get_gates();
    if (from_last) reverse(gate_list.begin(), gate_list.end());
    std::vector<QCirGate*> result;
    std::unordered_set<QubitIdType> blocked;
    for (size_t i = 0; i < qcir.get_num_qubits(); i++) {
        result.emplace_back(nullptr);
    }

    for (auto gate : gate_list) {
        auto const operands            = gate->get_qubits();
        auto const gate_is_not_blocked = std::ranges::all_of(operands, [&](auto operand) { return !blocked.contains(operand); });
        for (auto operand : operands) {
            if (gate_is_not_blocked) result[operand] = gate;
            blocked.emplace(operand);
        }
        if (blocked.size() == qcir.get_num_qubits()) break;
    }

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
    auto prev_op     = prev_gate->get_operation().get_underlying<LegacyGateType>();
    auto op          = gate->get_operation().get_underlying<LegacyGateType>();
    auto const phase = prev_op.get_phase() + op.get_phase();
    if (phase == dvlab::Phase(0)) {
        qcir.remove_gate(prev_gate->get_id());
        return;
    }

    prev_gate->set_operation(LegacyGateType{std::make_tuple(GateRotationCategory::px, 1, phase)});
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
    auto prev_op     = prev_gate->get_operation().get_underlying<PZGate>();
    auto op          = gate->get_operation().get_underlying<PZGate>();
    auto const phase = prev_op.get_phase() + op.get_phase();
    if (phase == dvlab::Phase(0)) {
        qcir.remove_gate(prev_gate->get_id());
        return;
    }
    prev_gate->set_operation(PZGate(phase));
}

/**
 * @brief Cancel if CX-CX / CZ-CZ, otherwise append it.
 * @param QC: the circuit
 * @param previousGate: previous gate
 * @param gate: the incoming gate
 *
 * @return modified circuit
 */
void Optimizer::_cancel_cx_or_cz(QCir& qcir, QCirGate* prev_gate, QCirGate* gate) {
    if (prev_gate->get_operation() == CXGate{} &&
        gate->get_operation() == CXGate{} &&
        prev_gate->get_qubits() == gate->get_qubits()) {
        qcir.remove_gate(prev_gate->get_id());
        return;
    } else if (prev_gate->get_operation() == CZGate{} &&
               gate->get_operation() == CZGate{}) {
        if ((prev_gate->get_qubit(0) == gate->get_qubit(0) &&
             prev_gate->get_qubit(1) == gate->get_qubit(1)) ||
            (prev_gate->get_qubit(0) == gate->get_qubit(1) &&
             prev_gate->get_qubit(1) == gate->get_qubit(0))) {
            qcir.remove_gate(prev_gate->get_id());
            return;
        } else {
            qcir.append(gate->get_operation(), gate->get_qubits());
        }
    }

    qcir.append(gate->get_operation(), gate->get_qubits());
}

namespace {

size_t match_gate_sequence(std::vector<std::string> const& type_seq,
                           std::vector<std::string> const& target_seq) {
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
                                        size_t seq_len, std::vector<std::string> const& seq) {
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
            replaced.append(gate->get_operation(), gate->get_qubits());
            continue;
        }

        if (gate_num != 0) {
            replaced.append(gate->get_operation(), gate->get_qubits());
            gate_num--;
            if (gate_num == 0) {
                replace_count = seq_len;
            }
            continue;
        }

        if (replace_count == 0) {
            replaced.append(gate->get_operation(), gate->get_qubits());
            continue;
        }

        if (replace_count == seq_len) {
            for (auto& type : seq) {
                replaced.add_gate(type, gate->get_qubits(), dvlab::Phase(0), true);
            }
            replace_count--;
            continue;
        }
        replace_count--;
    }

    return replaced;
}

std::vector<std::string> zx_optimize(std::vector<std::string> const& partial) {
    QCir qcir(1);

    for (std::string const& type : partial) {
        auto gate_type                                 = str_to_gate_type(type);
        auto const& [category, num_qubits, gate_phase] = gate_type.value();
        if (gate_phase.has_value())
            qcir.add_gate(type, QubitIdList{0}, gate_phase.value(), true);
        else
            qcir.add_gate(type, QubitIdList{0}, dvlab::Phase(0), true);
    }

    auto zx = to_zxgraph(qcir).value();
    zx.add_procedure("QC2ZX");

    extractor::Extractor ext(&zx, nullptr /*, std::nullopt */);
    QCir* result = ext.extract();

    auto const gate_list = result->get_gates();
    std::vector<std::string> opt_partial;
    opt_partial.reserve(gate_list.size());
    for (auto gate : gate_list) {
        opt_partial.emplace_back(gate->get_type_str());
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
                       return gate->get_type_str();
                   }) |
                   tl::to<std::vector>();
        };

        std::vector<std::string> type_seq =
            get_type_sequence(qcir.get_gates(), gsl::narrow<QubitIdType>(qubit));

        std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> replacements;
        while (!type_seq.empty()) {
            std::vector<std::string> partial;

            while (!type_seq.empty()) {
                std::string const type = type_seq[0];
                type_seq.erase(type_seq.begin());
                if (type == "cx" || type == "cz" || type == "ecr") {
                    break;
                }
                partial.emplace_back(type);
            }

            if (partial.size() >= 3) {
                auto opt_partial = zx_optimize(partial);
                std::vector<std::string> replaced_h_opt_partial;
                for (auto const& g : opt_partial) {
                    if (g == "h") {
                        replaced_h_opt_partial.emplace_back("s");
                        replaced_h_opt_partial.emplace_back("sx");
                        replaced_h_opt_partial.emplace_back("s");
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
            std::vector<std::string> const updated_type_seq =
                get_type_sequence(qcir.get_gates(), gsl::narrow<QubitIdType>(qubit));

            size_t const g = match_gate_sequence(updated_type_seq, lhs);
            qcir           = replace_single_qubit_gate_sequence(qcir, gsl::narrow<QubitIdType>(qubit), g, lhs.size(), rhs);
        }
    }
}

}  // namespace qsyn::qcir
