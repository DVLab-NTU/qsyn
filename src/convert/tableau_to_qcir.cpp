/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_to_qcir.hpp"

#include <cassert>
#include <gsl/narrow>
#include <random>
#include <stack>
#include <tl/adjacent.hpp>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>
#include <unordered_map>
#include <unordered_set>

#include "qcir/basic_gate_type.hpp"
#include "qcir/operation.hpp"
#include "qcir/qcir.hpp"
#include "spdlog/spdlog.h"
#include "tableau/classical_tableau.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "util/graph/digraph.hpp"
#include "util/graph/minimum_spanning_arborescence.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::tableau {

/**
 * @brief Parse a condition expression to extract classical bit and value
 *
 * @param condition_expr Condition expression like "c[0]==1" or "c[1]==0"
 * @return std::pair<std::optional<size_t>, std::optional<size_t>> (classical_bit, value)
 */
std::pair<std::optional<size_t>, std::optional<size_t>> parse_condition_expression(std::string const& condition_expr) {
    // Simple parser for "c[bit]==value" format
    size_t bracket_start = condition_expr.find('[');
    size_t bracket_end   = condition_expr.find(']');
    size_t eq_pos        = condition_expr.find("==");

    if (bracket_start == std::string::npos || bracket_end == std::string::npos || eq_pos == std::string::npos) {
        return {std::nullopt, std::nullopt};
    }

    if (bracket_start >= bracket_end || bracket_end >= eq_pos) {
        return {std::nullopt, std::nullopt};
    }

    try {
        size_t classical_bit = std::stoul(condition_expr.substr(bracket_start + 1, bracket_end - bracket_start - 1));
        size_t value         = std::stoul(condition_expr.substr(eq_pos + 2));
        return {classical_bit, value};
    } catch (std::exception const&) {
        return {std::nullopt, std::nullopt};
    }
}

namespace {

struct AncillaPmcStats {
    std::unordered_map<size_t, size_t> total_epochs_by_ancilla;
    size_t total_classical_bits = 0;
};

AncillaPmcStats count_pmc_epochs_by_ancilla(Tableau const& tableau) {
    AncillaPmcStats stats;
    for (auto const& subtableau : tableau) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&subtableau);
        if (cct == nullptr || !cct->is_classical_control()) {
            continue;
        }
        ++stats.total_epochs_by_ancilla[cct->ancilla_qubit()];
        ++stats.total_classical_bits;
    }
    return stats;
}

void emit_ancilla_reset(qcir::QCir& qcir, size_t ancilla_qubit, bool apply_h_after = false) {
    qcir.append(qcir::ResetGate(), {ancilla_qubit});
    if (apply_h_after) {
        qcir.append(qcir::HGate(), {ancilla_qubit});
    }
}

void add_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op) {
    using COT                  = CliffordOperatorType;
    auto const& [type, qubits] = op;

    switch (type) {
        case COT::h:
            qcir.append(qcir::HGate(), {qubits[0]});
            break;
        case COT::s:
            qcir.append(qcir::SGate(), {qubits[0]});
            break;
        case COT::cx:
            qcir.append(qcir::CXGate(), {qubits[0], qubits[1]});
            break;
        case COT::sdg:
            qcir.append(qcir::SdgGate(), {qubits[0]});
            break;
        case COT::v:
            qcir.append(qcir::SXGate(), {qubits[0]});
            break;
        case COT::vdg:
            qcir.append(qcir::SXdgGate(), {qubits[0]});
            break;
        case COT::x:
            qcir.append(qcir::XGate(), {qubits[0]});
            break;
        case COT::y:
            qcir.append(qcir::YGate(), {qubits[0]});
            break;
        case COT::z:
            qcir.append(qcir::ZGate(), {qubits[0]});
            break;
        case COT::cz:
            qcir.append(qcir::CZGate(), {qubits[0], qubits[1]});
            break;
        case COT::swap:
            qcir.append(qcir::SwapGate(), {qubits[0], qubits[1]});
            break;
        case COT::ecr:
            qcir.append(qcir::ECRGate(), {qubits[0], qubits[1]});
            break;
    }
}

/**
 * @brief Add a classical controlled clifford gate to QCir
 *
 * @param qcir The QCir to add the gate to
 * @param op The clifford operator to wrap in classical control
 * @param classical_bit The classical bit to control on
 * @param classical_value The value to check (typically 1)
 */
void add_classical_controlled_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op, size_t classical_bit, size_t classical_value = 1) {
    using COT                  = CliffordOperatorType;
    auto const& [type, qubits] = op;
    switch (type) {
        case COT::h:
            qcir.append(qcir::HGate(), {qubits[0]}, classical_bit, classical_value);
            break;
        case COT::s:
            qcir.append(qcir::SGate(), {qubits[0]}, classical_bit, classical_value);
            break;
        case COT::cx:
            qcir.append(qcir::CXGate(), {qubits[0], qubits[1]}, classical_bit, classical_value);
            break;
        case COT::sdg:
            qcir.append(qcir::SdgGate(), {qubits[0]}, classical_bit, classical_value);
            break;
        case COT::v:
            qcir.append(qcir::SXGate(), {qubits[0]}, classical_bit, classical_value);
            break;
        case COT::vdg:
            qcir.append(qcir::SXdgGate(), {qubits[0]}, classical_bit, classical_value);
            break;
        case COT::x:
            qcir.append(qcir::XGate(), {qubits[0]}, classical_bit, classical_value);
            break;
        case COT::y:
            qcir.append(qcir::YGate(), {qubits[0]}, classical_bit, classical_value);
            break;
        case COT::z:
            qcir.append(qcir::ZGate(), {qubits[0]}, classical_bit, classical_value);
            break;
        case COT::cz:
            qcir.append(qcir::CZGate(), {qubits[0], qubits[1]}, classical_bit, classical_value);
            break;
        case COT::swap:
            qcir.append(qcir::SwapGate(), {qubits[0], qubits[1]}, classical_bit, classical_value);
            break;
        case COT::ecr:
            qcir.append(qcir::ECRGate(), {qubits[0], qubits[1]}, classical_bit, classical_value);
            break;
    }
}

bool append_cct_to_qcir(
    qcir::QCir& qcir,
    ClassicalControlTableau const& cct,
    StabilizerTableauSynthesisStrategy const& cct_strategy,
    size_t n_qubits,
    std::optional<size_t> classical_bit_override = std::nullopt,
    bool append_reset                            = false) {
    size_t const ancilla_qubit = cct.ancilla_qubit();
    if (ancilla_qubit >= n_qubits) {
        spdlog::error("Ancilla qubit {} is out of range for n_qubits {}", ancilla_qubit, n_qubits);
        return false;
    }

    // ponytail: default NONE -> X to keep CCT conversion runnable until per-CCT
    // measurement typing is fully propagated; upgrade path is explicit CCT typing.
    auto measurement_type = cct.measurement_type();
    if (measurement_type == MeasurementType::none) {
        measurement_type = MeasurementType::X;
    }

    std::optional<size_t> classical_bit_opt = classical_bit_override;
    if (!classical_bit_opt.has_value() && cct.has_classical_bit_id()) {
        classical_bit_opt = cct.classical_bit_id();
    }
    size_t classical_bit = 0;
    if (classical_bit_opt.has_value()) {
        classical_bit = *classical_bit_opt;
        if (classical_bit >= qcir.get_num_classical_bits()) {
            spdlog::error(
                "Classical bit {} is out of range for QCir classical pool size {}",
                classical_bit,
                qcir.get_num_classical_bits());
            return false;
        }
    } else {
        classical_bit = qcir.allocate_fresh_classical_bit();
    }
    switch (measurement_type) {
        case MeasurementType::none:
            break;
        case MeasurementType::Z:
            qcir.append(qcir::MeasurementGate(qcir::MeasurementBasis::Z), ancilla_qubit, classical_bit);
            break;
        case MeasurementType::X:
            qcir.append(qcir::MeasurementGate(qcir::MeasurementBasis::X), ancilla_qubit, classical_bit);
            break;
    }

    if (!qcir.is_classical_measured(classical_bit)) {
        spdlog::error("Classical bit {} was not marked as measured after measurement gate", classical_bit);
        return false;
    }

    auto const clifford_ops = extract_clifford_operators(cct.operations(), cct_strategy);
    for (auto const& op : clifford_ops) {
        if (stop_requested()) {
            return false;
        }
        add_classical_controlled_clifford_gate(qcir, op, classical_bit, 1);
    }

    if (append_reset) {
        // Enforce measure -> (if-else)* -> reset on reused ancilla lines.
        qcir.append(qcir::ResetGate(), {ancilla_qubit});
    }
    return true;
}

bool append_gadget_cct_to_qcir(
    qcir::QCir& qcir,
    ClassicalControlTableau const& cct,
    StabilizerTableauSynthesisStrategy const& cct_strategy) {
    auto const clifford_ops = extract_clifford_operators(cct.operations(), cct_strategy);
    for (auto const& op : clifford_ops) {
        if (stop_requested()) {
            return false;
        }
        add_clifford_gate(qcir, op);
    }
    return true;
}

}  // namespace

namespace detail {

using COT = CliffordOperatorType;

void add_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op) {
    auto const& [type, qubits] = op;

    switch (type) {
        case COT::h:
            qcir.append(qcir::HGate(), {qubits[0]});
            break;
        case COT::s:
            qcir.append(qcir::SGate(), {qubits[0]});
            break;
        case COT::cx:
            qcir.append(qcir::CXGate(), {qubits[0], qubits[1]});
            break;
        case COT::sdg:
            qcir.append(qcir::SdgGate(), {qubits[0]});
            break;
        case COT::v:
            qcir.append(qcir::SXGate(), {qubits[0]});
            break;
        case COT::vdg:
            qcir.append(qcir::SXdgGate(), {qubits[0]});
            break;
        case COT::x:
            qcir.append(qcir::XGate(), {qubits[0]});
            break;
        case COT::y:
            qcir.append(qcir::YGate(), {qubits[0]});
            break;
        case COT::z:
            qcir.append(qcir::ZGate(), {qubits[0]});
            break;
        case COT::cz:
            qcir.append(qcir::CZGate(), {qubits[0], qubits[1]});
            break;
        case COT::swap:
            qcir.append(qcir::SwapGate(), {qubits[0], qubits[1]});
            break;
        case COT::ecr:
            qcir.append(qcir::ECRGate(), {qubits[0], qubits[1]});
            break;
    }
}

void prepend_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op) {
    auto const& [type, qubits] = op;
    switch (type) {
        case COT::h:
            qcir.prepend(qcir::HGate(), {qubits[0]});
            break;
        case COT::s:
            qcir.prepend(qcir::SGate(), {qubits[0]});
            break;
        case COT::cx:
            qcir.prepend(qcir::CXGate(), {qubits[0], qubits[1]});
            break;
        case COT::sdg:
            qcir.prepend(qcir::SdgGate(), {qubits[0]});
            break;
        case COT::v:
            qcir.prepend(qcir::SXGate(), {qubits[0]});
            break;
        case COT::vdg:
            qcir.prepend(qcir::SXdgGate(), {qubits[0]});
            break;
        case COT::x:
            qcir.prepend(qcir::XGate(), {qubits[0]});
            break;
        case COT::y:
            qcir.prepend(qcir::YGate(), {qubits[0]});
            break;
        case COT::z:
            qcir.prepend(qcir::ZGate(), {qubits[0]});
            break;
        case COT::cz:
            qcir.prepend(qcir::CZGate(), {qubits[0], qubits[1]});
            break;
        case COT::swap:
            qcir.prepend(qcir::SwapGate(), {qubits[0], qubits[1]});
            break;
        case COT::ecr:
            qcir.prepend(qcir::ECRGate(), {qubits[0], qubits[1]});
            break;
    }
}

void add_clifford_gate(PauliRotationTableau& rotations, CliffordOperator const& op) {
    auto const& [type, qubits] = op;

    switch (type) {
        case COT::h:
            for (auto& rot : rotations) {
                rot.h(qubits[0]);
            }
            break;
        case COT::s:
            for (auto& rot : rotations) {
                rot.s(qubits[0]);
            }
            break;
        case COT::cx:
            for (auto& rot : rotations) {
                rot.cx(qubits[0], qubits[1]);
            }
            break;
        case COT::v:
            for (auto& rot : rotations) {
                rot.h(qubits[0]);
                rot.s(qubits[0]);
                rot.h(qubits[0]);
            }
            break;
        default:
            spdlog::error("Invalid Clifford operator type {}. The operation is skipped.", to_string(type));
            break;
    }
}

void add_clifford_gate(StabilizerTableau& tableau, CliffordOperator const& op) {
    auto const& [type, qubits] = op;
    switch (type) {
        case COT::h:
            tableau.h(qubits[0]);
            break;
        case COT::s:
            tableau.s(qubits[0]);
            break;
        case COT::cx:
            tableau.cx(qubits[0], qubits[1]);
            break;
        case COT::sdg:
            tableau.sdg(qubits[0]);
            break;
        case COT::v:
            tableau.v(qubits[0]);
            break;
        case COT::vdg:
            tableau.vdg(qubits[0]);
            break;
        default:
            spdlog::error("Invalid Clifford operator type {}. The operation is skipped.", to_string(type));
            break;
    }
}

void prepend_clifford_gate(StabilizerTableau& tableau, CliffordOperator const& op) {
    tableau.prepend(op);
}

}  // namespace detail

/**
 * @brief convert a stabilizer tableau to a QCir.
 *
 * @param clifford - pass by value on purpose
 * @return std::optional<qcir::QCir>
 */
std::optional<qcir::QCir> to_qcir(StabilizerTableau const& clifford, StabilizerTableauSynthesisStrategy const& strategy) {
    qcir::QCir qcir{clifford.n_qubits()};
    for (auto const& op : extract_clifford_operators(clifford, strategy)) {
        if (stop_requested()) {
            return std::nullopt;
        }
        add_clifford_gate(qcir, op);
    }

    return qcir;
}

std::optional<qcir::QCir> NaivePauliRotationsSynthesisStrategy::synthesize(std::vector<PauliRotation> const& rotations) const {
    if (rotations.empty()) {
        return qcir::QCir{0};
    }

    auto qcir = qcir::QCir{rotations.front().n_qubits()};

    for (auto const& rotation : rotations) {
        if (rotation.is_CZ()) {
            std::vector<size_t> z_qubits;
            for (size_t i = 0; i < rotation.n_qubits(); ++i) {
                if (rotation.is_z(i)) z_qubits.push_back(i);
            }
            if (z_qubits.size() == 2) {
                // Represent "CZ with phase" as a controlled-Z rotation.
                // `CZGate()` is a special case of `ControlGate(PZGate(phase))` with phase = pi.
                qcir.append(qcir::ControlGate(qcir::PZGate(rotation.phase())), {z_qubits[0], z_qubits[1]});
            }
            continue;
        }
        auto [ops, qubit] = extract_clifford_operators(rotation);

        for (auto const& op : ops) {
            add_clifford_gate(qcir, op);
        }

        qcir.append(qcir::PZGate(rotation.phase()), {qubit});

        adjoint_inplace(ops);

        for (auto const& op : ops) {
            add_clifford_gate(qcir, op);
        }
    }

    return qcir;
}

/**
 * @brief convert a Pauli rotation to a QCir. This is a naive implementation.
 *
 * @param pauli_rotation
 * @return qcir::QCir
 */
std::optional<qcir::QCir> to_qcir(
    std::vector<PauliRotation> const& pauli_rotations,
    PauliRotationsSynthesisStrategy const& strategy) {
    return strategy.synthesize(pauli_rotations);
}

/**
 * @brief convert a ClassicalControlTableau to a QCir.
 *
 * This function implements the cct_strategy:
 * 1. Adds a measurement gate that measures the ancilla qubit to a classical bit
 * 2. Extracts clifford operators from cct.operations() using the provided synthesis strategy
 *    Note: These operations reference the reference qubit(s) where gates were applied (not the ancilla)
 * 3. Applies each operator as a classically controlled gate (controlled by the classical bit)
 *
 * @param cct The ClassicalControlTableau to convert
 * @param cct_strategy The synthesis strategy to use for extracting clifford operators
 * @param n_qubits number of qubits in the QCir
 * @return std::optional<qcir::QCir> The converted QCir, or nullopt on failure
 */
std::optional<qcir::QCir> to_qcir(
    ClassicalControlTableau const& cct,
    StabilizerTableauSynthesisStrategy const& cct_strategy, size_t n_qubits) {
    if (stop_requested()) {
        return std::nullopt;
    }

    size_t const n_classical_bits = cct.has_classical_bit_id() ? (cct.classical_bit_id() + 1) : 0;
    qcir::QCir qcir{n_qubits, n_classical_bits};
    if (cct.is_gadget()) {
        if (!append_gadget_cct_to_qcir(qcir, cct, cct_strategy)) {
            return std::nullopt;
        }
        return qcir;
    }
    if (!append_cct_to_qcir(qcir, cct, cct_strategy, n_qubits, std::nullopt, true)) {
        return std::nullopt;
    }
    return qcir;
}

/**
 * @brief convert a stabilizer tableau and a list of Pauli rotations to a QCir.
 *
 * @param clifford
 * @param pauli_rotations
 * @return qcir::QCir
 */
std::optional<qcir::QCir> to_qcir(Tableau const& tableau, StabilizerTableauSynthesisStrategy const& st_strategy, PauliRotationsSynthesisStrategy const& pr_strategy, StabilizerTableauSynthesisStrategy const& cct_strategy) {
    size_t n_qubits = tableau.n_qubits();

    // Validate tableau has qubits
    if (n_qubits == 0) {
        spdlog::error("Tableau has 0 qubits");
        return std::nullopt;
    }

    AncillaPmcStats const pmc_stats = count_pmc_epochs_by_ancilla(tableau);
    assert(pmc_stats.total_classical_bits >= pmc_stats.total_epochs_by_ancilla.size());

    qcir::QCir qcir{n_qubits, std::max(tableau.export_classical_bit_count(), pmc_stats.total_classical_bits)};
    std::unordered_set<size_t> plus_after_reset_ancilla;
    std::unordered_map<size_t, size_t> pmc_seen_by_ancilla;

    // Set initial state metadata for ancilla qubits
    if (tableau.n_ancilla() > 0) {
        auto const& initial_states = tableau.ancilla_initial_states();
        for (auto const& [ancilla_index, initial_state] : initial_states) {
            if (stop_requested()) {
                return std::nullopt;
            }

            if (ancilla_index >= n_qubits) {
                continue;
            }

            qcir.set_qubit_type(ancilla_index, qcir::QubitType::ancilla);

            auto effective_state = initial_state;
            if (initial_state == qsyn::tableau::AncillaInitialState::PLUS &&
                pmc_stats.total_epochs_by_ancilla.contains(ancilla_index)) {
                effective_state = qsyn::tableau::AncillaInitialState::ZERO;
                plus_after_reset_ancilla.insert(ancilla_index);
            }

            switch (effective_state) {
                case qsyn::tableau::AncillaInitialState::ZERO:
                    qcir.set_initial_state(ancilla_index, qcir::QubitInitialState::zero);
                    break;
                case qsyn::tableau::AncillaInitialState::ONE:
                    qcir.set_initial_state(ancilla_index, qcir::QubitInitialState::one);
                    break;
                case qsyn::tableau::AncillaInitialState::PLUS:
                    qcir.set_initial_state(ancilla_index, qcir::QubitInitialState::plus);
                    break;
                case qsyn::tableau::AncillaInitialState::MINUS:
                    qcir.set_initial_state(ancilla_index, qcir::QubitInitialState::minus);
                    break;
            }
        }
    }

    if (tableau.n_ancilla() > 0) {
        size_t const ancilla_base = n_qubits - tableau.n_ancilla();
        for (size_t q = ancilla_base; q < n_qubits; ++q) {
            emit_ancilla_reset(qcir, q, plus_after_reset_ancilla.contains(q));
        }
    }

    size_t pmc_epoch_index = 0;
    for (size_t i = 0; i < tableau.size(); ++i) {
        spdlog::debug("Converting subtableau {} to qcir", i);
        auto const& subtableau = tableau[i];
        if (stop_requested()) {
            return std::nullopt;
        }
        if (auto const* st = std::get_if<StabilizerTableau>(&subtableau)) {
            auto const qc_fragment = to_qcir(*st, st_strategy);
            if (!qc_fragment) {
                spdlog::error("Failed to convert stabilizer subtableau to qcir");
                return std::nullopt;
            }
            if (qc_fragment->get_num_qubits() > n_qubits) {
                spdlog::error("Fragment has {} qubits but expected at most {} qubits",
                              qc_fragment->get_num_qubits(),
                              n_qubits);
                return std::nullopt;
            }
            auto padded_fragment = *qc_fragment;
            if (padded_fragment.get_num_qubits() < n_qubits) {
                padded_fragment.add_qubits(n_qubits - padded_fragment.get_num_qubits());
            }
            qcir.compose(padded_fragment);
            continue;
        }
        if (auto const* pr = std::get_if<std::vector<PauliRotation>>(&subtableau)) {
            auto const qc_fragment = to_qcir(*pr, pr_strategy);
            if (!qc_fragment) {
                spdlog::error("Failed to convert pauli-rotation subtableau to qcir");
                return std::nullopt;
            }
            if (qc_fragment->get_num_qubits() > n_qubits) {
                spdlog::error("Fragment has {} qubits but expected at most {} qubits",
                              qc_fragment->get_num_qubits(),
                              n_qubits);
                return std::nullopt;
            }
            auto padded_fragment = *qc_fragment;
            if (padded_fragment.get_num_qubits() < n_qubits) {
                padded_fragment.add_qubits(n_qubits - padded_fragment.get_num_qubits());
            }
            qcir.compose(padded_fragment);
            continue;
        }
        auto const* cct = std::get_if<ClassicalControlTableau>(&subtableau);
        if (cct == nullptr) {
            spdlog::error("Unsupported subtableau variant at index {}", i);
            return std::nullopt;
        }
        if (cct->is_classical_control()) {
            size_t const ancilla_qubit = cct->ancilla_qubit();
            size_t classical_bit       = cct->has_classical_bit_id() ? cct->classical_bit_id() : pmc_epoch_index;
            if (!append_cct_to_qcir(
                    qcir,
                    *cct,
                    cct_strategy,
                    n_qubits,
                    classical_bit,
                    false)) {
                spdlog::error("Failed to convert CCT subtableau to qcir");
                return std::nullopt;
            }
            ++pmc_epoch_index;
            size_t& seen = pmc_seen_by_ancilla[ancilla_qubit];
            ++seen;
            auto const total_it = pmc_stats.total_epochs_by_ancilla.find(ancilla_qubit);
            if (total_it != pmc_stats.total_epochs_by_ancilla.end() && seen < total_it->second) {
                emit_ancilla_reset(qcir, ancilla_qubit);
            }
            continue;
        }
        if (!append_gadget_cct_to_qcir(qcir, *cct, cct_strategy)) {
            spdlog::error("Failed to convert gadget CCT subtableau to qcir");
            return std::nullopt;
        }
    }
    qcir.set_export_schedule_width(tableau.export_ancilla_depth());
    return qcir;
}

std::optional<qcir::QCir> tableau_to_qcir_hopt_naive(Tableau const& tableau) {
    HOptSynthesisStrategy clifford_strategy;
    NaivePauliRotationsSynthesisStrategy rotation_strategy;
    return to_qcir(tableau, clifford_strategy, rotation_strategy, clifford_strategy);
}

std::optional<qcir::QCir> to_qcir(
    Tableau const& tableau,
    StabilizerTableauSynthesisStrategy const& st_strategy,
    PauliRotationsSynthesisStrategy const& pr_strategy,
    SynthesisType synthesis_type) {
    if (synthesis_type != SynthesisType::eager) {
        spdlog::error("Only eager tableau-to-qcir synthesis is available on feat/ancilla after merge");
        return std::nullopt;
    }
    return to_qcir(tableau, st_strategy, pr_strategy, st_strategy);
}

}  // namespace qsyn::tableau
