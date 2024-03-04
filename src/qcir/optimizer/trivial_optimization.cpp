/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Implement the trivial optimization ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cassert>

#include "../../convert/qcir_to_zxgraph.hpp"
#include "../qcir.hpp"
#include "../qcir_gate.hpp"
#include "./optimizer.hpp"
#include "extractor/extract.hpp"

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

    auto gate_list = qcir.get_topologically_ordered_gates();
    for (auto gate : gate_list) {
        if (stop_requested()) {
            spdlog::warn("optimization interrupted");
            return std::nullopt;
        }
        auto const last_layer = _get_first_layer_gates(result, true);
        auto const qubit      = gate->get_targets()._qubit;
        if (last_layer[qubit] == nullptr) {
            Optimizer::_add_gate_to_circuit(result, gate, false);
            continue;
        }
        QCirGate* previous_gate = last_layer[qubit];
        if (is_double_qubit_gate(gate)) {
            auto const q2 = gate->get_targets()._qubit;
            if (previous_gate->get_id() != last_layer[q2]->get_id()) {
                // 2-qubit gate do not match up
                Optimizer::_add_gate_to_circuit(result, gate, false);
                continue;
            }
            _cancel_double_gate(result, previous_gate, gate);
        } else if (is_single_z_rotation(gate) && is_single_z_rotation(previous_gate)) {
            _fuse_z_phase(result, previous_gate, gate);
        } else if (is_single_x_rotation(gate) && is_single_x_rotation(previous_gate)) {
            _fuse_x_phase(result, previous_gate, gate);
        } else if (gate->get_rotation_category() == previous_gate->get_rotation_category()) {
            result.remove_gate(previous_gate->get_id());
        } else {
            Optimizer::_add_gate_to_circuit(result, gate, false);
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
    std::vector<QCirGate*> gate_list = qcir.update_topological_order();
    if (from_last) reverse(gate_list.begin(), gate_list.end());
    std::vector<QCirGate*> result;
    std::vector<bool> blocked;
    for (size_t i = 0; i < qcir.get_num_qubits(); i++) {
        result.emplace_back(nullptr);
        blocked.emplace_back(false);
    }

    for (auto gate : gate_list) {
        auto const qubits              = gate->get_qubits();
        auto const gate_is_not_blocked = all_of(qubits.begin(), qubits.end(), [&](QubitInfo qubit) { return !blocked[qubit._qubit]; });
        for (auto q : qubits) {
            if (gate_is_not_blocked) result[q._qubit] = gate;
            blocked[q._qubit] = true;
        }
        if (all_of(blocked.begin(), blocked.end(), [](bool block) { return block; })) break;
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
    auto const phase = prev_gate->get_phase() + gate->get_phase();
    if (phase == dvlab::Phase(0)) {
        qcir.remove_gate(prev_gate->get_id());
        return;
    }
    if (prev_gate->get_rotation_category() == GateRotationCategory::px)
        prev_gate->set_phase(phase);
    else {
        QubitIdList qubit_list;
        qubit_list.emplace_back(prev_gate->get_targets()._qubit);
        qcir.remove_gate(prev_gate->get_id());
        qcir.add_gate("px", qubit_list, phase, true);
    }
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
    auto const phase = prev_gate->get_phase() + gate->get_phase();
    if (phase == dvlab::Phase(0)) {
        qcir.remove_gate(prev_gate->get_id());
        return;
    }
    if (prev_gate->get_rotation_category() == GateRotationCategory::pz)
        prev_gate->set_phase(phase);
    else {
        QubitIdList qubit_list;
        qubit_list.emplace_back(prev_gate->get_targets()._qubit);
        qcir.remove_gate(prev_gate->get_id());
        qcir.add_gate("p", qubit_list, phase, true);
    }
}

/**
 * @brief Cancel if CX-CX / CZ-CZ, otherwise append it.
 * @param QC: the circuit
 * @param previousGate: previous gate
 * @param gate: the incoming gate
 *
 * @return modified circuit
 */
void Optimizer::_cancel_double_gate(QCir& qcir, QCirGate* prev_gate, QCirGate* gate) {
    if (!is_double_qubit_gate(prev_gate) || !is_double_qubit_gate(gate)) {
        Optimizer::_add_gate_to_circuit(qcir, gate, false);
        return;
    }

    auto const prev_qubits = prev_gate->get_qubits();
    auto const gate_qubits = gate->get_qubits();
    if ((prev_qubits[0]._qubit != gate_qubits[0]._qubit || prev_qubits[1]._qubit != gate_qubits[1]._qubit) &&
        (prev_qubits[0]._qubit != gate_qubits[1]._qubit || prev_qubits[1]._qubit != gate_qubits[0]._qubit)) {
        Optimizer::_add_gate_to_circuit(qcir, gate, false);
        return;
    }

    if (prev_gate->get_rotation_category() != gate->get_rotation_category()) {
        Optimizer::_add_gate_to_circuit(qcir, gate, false);
        return;
    }
    if (prev_gate->is_cz() || prev_gate->get_control()._qubit == gate->get_control()._qubit)
        qcir.remove_gate(prev_gate->get_id());
    else
        Optimizer::_add_gate_to_circuit(qcir, gate, false);
}

static size_t _match_gate_sequence(std::vector<std::string> const& type_seq,
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

static QCir _replace_gate_sequence(QCir& qcir, QubitIdType qubit, size_t gate_num,
                                   size_t seq_len, std::vector<std::string> const& seq) {
    QCir replaced;
    replaced.add_procedures(qcir.get_procedures());
    replaced.add_qubits(qcir.get_num_qubits());
    replaced.set_gate_set(qcir.get_gate_set());

    qcir.update_topological_order();
    auto const gate_list = qcir.get_topologically_ordered_gates();
    size_t replace_count = 0;

    if (gate_num == 0) {
        replace_count = seq_len;
    }

    for (auto gate : gate_list) {
        auto bit_range = gate->get_qubits() |
                         std::views::transform([](QubitInfo const& qb) { return qb._qubit; });
        if (gate->get_targets()._qubit != qubit) {
            replaced.add_gate(gate->get_type_str(), {bit_range.begin(), bit_range.end()}, gate->get_phase(), true);
            continue;
        }

        if (gate_num != 0) {
            replaced.add_gate(gate->get_type_str(), {bit_range.begin(), bit_range.end()}, gate->get_phase(), true);
            gate_num--;
            if (gate_num == 0) {
                replace_count = seq_len;
            }
            continue;
        }

        if (replace_count == 0) {
            replaced.add_gate(gate->get_type_str(), {bit_range.begin(), bit_range.end()}, gate->get_phase(), true);
            continue;
        }

        if (replace_count == seq_len) {
            for (auto& type : seq) {
                replaced.add_gate(type, {bit_range.begin(), bit_range.end()}, dvlab::Phase(0), true);
            }
            replace_count--;
            continue;
        }
        replace_count--;
    }

    return replaced;
}

static std::vector<std::string> _zx_optimize(std::vector<std::string> partial) {
    QCir qcir;
    qcir.add_qubits(1);

    for (std::string type : partial) {
        auto gate_type                                 = str_to_gate_type(type);
        auto const& [category, num_qubits, gate_phase] = gate_type.value();
        if (gate_phase.has_value())
            qcir.add_gate(type, QubitIdList{0}, gate_phase.value(), true);
        else
            qcir.add_gate(type, QubitIdList{0}, dvlab::Phase(0), true);
    }

    auto zx = to_zxgraph(qcir, 3).value();
    zx.add_procedure("QC2ZX");

    extractor::Extractor ext(&zx, nullptr, std::nullopt);
    QCir* result = ext.extract();

    result->update_topological_order();
    auto const gate_list = result->get_topologically_ordered_gates();
    std::vector<std::string> opt_partial;
    for (auto gate : gate_list) {
        opt_partial.emplace_back(gate->get_type_str());
    }

    return opt_partial;
}

void Optimizer::_partial_zx_optimization(QCir& qcir) {
    for (size_t i = 0; i < qcir.get_num_qubits(); i++) {
        qcir.update_topological_order();
        auto const gate_list = qcir.get_topologically_ordered_gates();
        std::vector<std::string> type_seq;
        for (auto gate : gate_list) {
            if ((size_t)gate->get_targets()._qubit == i || (size_t)gate->get_control()._qubit == i) {
                type_seq.emplace_back(gate->get_type_str());
            }
        }

        std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> replace_rules;
        while (type_seq.size()) {
            std::vector<std::string> partial;
            while (type_seq.size()) {
                std::string type = type_seq[0];
                type_seq.erase(type_seq.begin());
                if (type == "cx" || type == "cz" || type == "ecr") {
                    break;
                }
                partial.emplace_back(type);
            }

            if (partial.size() >= 3) {
                auto opt_partial = _zx_optimize(partial);
                std::vector<std::string> replaced_h_opt_partial;
                for (auto g : opt_partial) {
                    if (g == "h") {
                        replaced_h_opt_partial.emplace_back("s");
                        replaced_h_opt_partial.emplace_back("sx");
                        replaced_h_opt_partial.emplace_back("s");
                    } else {
                        replaced_h_opt_partial.emplace_back(g);
                    }
                }
                if (replaced_h_opt_partial.size() < partial.size()) {
                    replace_rules.emplace_back(std::make_pair(partial, replaced_h_opt_partial));
                }
            }
        }

        for (auto const& [lhs, rhs] : replace_rules) {
            qcir.update_topological_order();
            auto const updated_gate_list = qcir.get_topologically_ordered_gates();
            std::vector<std::string> updated_type_seq;
            for (auto gate : updated_gate_list) {
                if ((size_t)gate->get_targets()._qubit == i || (size_t)gate->get_control()._qubit == i) {
                    updated_type_seq.emplace_back(gate->get_type_str());
                }
            }

            size_t g      = _match_gate_sequence(updated_type_seq, lhs);
            QCir replaced = _replace_gate_sequence(qcir, i, g, lhs.size(), rhs);
            qcir          = replaced;
        }
    }
}

}  // namespace qsyn::qcir
