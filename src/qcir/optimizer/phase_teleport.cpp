/****************************************************************************
  PackageName  [ qcir/optimize ]
  Synopsis     [ phase teleport optimization ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <sul/dynamic_bitset.hpp>
#include <vector>

#include "qcir/basic_gate_type.hpp"
#include "qcir/operation.hpp"
#include "qcir/qcir.hpp"
#include "qsyn/qsyn_type.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/tableau.hpp"
#include "tableau/tableau_optimization.hpp"

bool stop_requested();

namespace qsyn::qcir {

/**
 * @brief local namespace for phase teleport optimization.
 *
 */
namespace {

/**
 * @brief Set phase to the operation if it is a single-qubit (P|R)(X|Y|Z) gate. Otherwise, throw an exception.
 *
 * @param op
 * @param phase
 */
void set_phase(Operation& op, Phase const& phase) {
    if (op.is<PZGate>()) {
        op = PZGate(phase);
    } else if (op.is<RZGate>()) {
        op = RZGate(phase);
    } else if (op.is<PXGate>()) {
        op = PXGate(phase);
    } else if (op.is<RXGate>()) {
        op = RXGate(phase);
    } else if (op.is<PYGate>()) {
        op = PYGate(phase);
    } else if (op.is<RYGate>()) {
        op = RYGate(phase);
    } else {
        throw std::runtime_error("Operation does not have a phase field.");
    }
}

/**
 * @brief Get the phase of the operation if it is a single-qubit (P|R)(X|Y|Z) gate. Otherwise, return an empty optional.
 *
 * @param op
 * @return Phase
 */
std::optional<Phase>
get_phase(Operation const& op) {
    if (op.is<PZGate>()) {
        return op.get_underlying<PZGate>().get_phase();
    } else if (op.is<RZGate>()) {
        return op.get_underlying<RZGate>().get_phase();
    } else if (op.is<PXGate>()) {
        return op.get_underlying<PXGate>().get_phase();
    } else if (op.is<RXGate>()) {
        return op.get_underlying<RXGate>().get_phase();
    } else if (op.is<PYGate>()) {
        return op.get_underlying<PYGate>().get_phase();
    } else if (op.is<RYGate>()) {
        return op.get_underlying<RYGate>().get_phase();
    } else {
        return std::nullopt;
    }
}

/**
 * @brief check if the rotation is a (P|R)(X|Y|Z) gate.
 *
 * @param op
 * @return true
 * @return false
 */
bool is_single_qubit_rotation(Operation const& op) {
    return (op.is<PZGate>() || op.is<RZGate>() || op.is<PXGate>() || op.is<RXGate>() || op.is<PYGate>() || op.is<RYGate>());
}

std::optional<std::pair<tableau::PauliRotationTableau, std::vector<size_t>>>
to_tableau_with_gate_ids(QCir const& qcir) {
    tableau::Tableau tabl{qcir.get_num_qubits()};
    std::vector<size_t> gate_ids;

    for (auto const& gate : qcir.get_gates()) {
        if (stop_requested()) {
            return std::nullopt;
        }
        // only allow Clifford gates and single-qubit (P|R)(X|Y|Z) gates
        if (!is_clifford(gate->get_operation()) && !is_single_qubit_rotation(gate->get_operation())) {
            spdlog::error("Phase teleport only supports circuits with only Clifford gates and single-qubit (P|R)(X|Y|Z) gates.");
            spdlog::error("Gate {} is not supported!!", gate->get_operation().get_repr());
            return std::nullopt;
        }
        if (!append_to_tableau(gate->get_operation(), tabl, gate->get_qubits())) {
            spdlog::error("Gate {} is not supported!!", gate->get_operation().get_repr());
            return std::nullopt;
        }
        // if this gate is a single-qubit phase gate, record the gate id and the index of the rotation
        if (auto ph = get_phase(gate->get_operation()); ph.has_value()) {
            if (ph->denominator() > 2) {
                assert(tabl.size() == 2);
                gate_ids.emplace_back(gate->get_id());
            }
        }
    }

    return std::make_pair(std::get<tableau::PauliRotationTableau>(tabl.back()), gate_ids);
}

/**
 * @brief remove identity rotations and gate id correspondences.
 *
 * @param rotations
 * @param gate_ids
 */
void remove_identities_teleport(
    tableau::PauliRotationTableau& rotations,
    std::vector<size_t>& gate_ids) {
    for (size_t i = 0; i < rotations.size(); ++i) {
        if (rotations[i].phase() == dvlab::Phase(0)) {
            gate_ids[i] = SIZE_MAX;
        }
    }

    std::erase_if(rotations, [](auto const& rotation) {
        return rotation.phase() == dvlab::Phase(0) ||
               rotation.pauli_product().is_identity();
    });

    std::erase_if(gate_ids, [](auto const& gate_id) {
        return gate_id == SIZE_MAX;
    });
}

/**
 * @brief get whether the phase of the operation is negated compared to the rotation.
 *
 * @param qcir
 * @param rotations
 * @param gate_ids
 * @return sul::dynamic_bitset<> if the ith bit is true, the ith rotation has a negated phase compared to the operation.
 */
sul::dynamic_bitset<> get_negated_phases(
    QCir const& qcir,
    tableau::PauliRotationTableau const& rotations,
    std::vector<size_t> const& gate_ids) {
    sul::dynamic_bitset<> negated(rotations.size(), false);

    for (size_t i = 0; i < rotations.size(); ++i) {
        negated[i] = get_phase(qcir.get_gate(gate_ids[i])->get_operation())->numerator() != rotations[i].phase().numerator();
    }

    return negated;
}

/**
 * @brief merge rotations with the same Pauli product and reflect the phase changes to the operations.
 *
 * @param qcir
 * @param rotations
 * @param gate_ids
 */
void merge_rotations_teleport(
    QCir& qcir,
    tableau::PauliRotationTableau& rotations,
    std::vector<size_t>& gate_ids) {
    auto const negated = get_negated_phases(qcir, rotations, gate_ids);

    for (size_t i = 0; i < rotations.size(); ++i) {
        if (rotations[i].phase() == dvlab::Phase(0)) {
            continue;
        }
        for (size_t j = i + 1; j < rotations.size(); ++j) {
            if (!tableau::is_commutative(rotations[i], rotations[j])) {
                break;
            }
            if (rotations[i].pauli_product() == rotations[j].pauli_product()) {
                auto new_op_i = qcir.get_gate(gate_ids[i])->get_operation();
                auto new_op_j = qcir.get_gate(gate_ids[j])->get_operation();

                spdlog::trace("== Merging gate {} and gate {} ==", gate_ids[i], gate_ids[j]);
                spdlog::trace("    Gate        {}: {} {}", gate_ids[i], qcir.get_gate(gate_ids[i])->get_operation().get_repr(), rotations[i].phase());
                spdlog::trace("    Gate        {}: {} {}", gate_ids[j], qcir.get_gate(gate_ids[j])->get_operation().get_repr(), rotations[j].phase());

                rotations[i].phase() += rotations[j].phase();
                rotations[j].phase() = dvlab::Phase(0);

                // Due to conjugations, the phase of the operation may be negated in the Pauli rotation tableau.
                // The new phase is the sum of the two phases if there is no relative negation between the two rotations
                // and the difference between the two phases if there is a relative negation.
                bool const one_rotation_negated = negated[i] ^ negated[j];
                auto const new_phase            = *get_phase(new_op_i) + (one_rotation_negated ? -*get_phase(new_op_j) : *get_phase(new_op_j));
                set_phase(new_op_i, new_phase);
                set_phase(new_op_j, dvlab::Phase(0));

                qcir.get_gate(gate_ids[i])->set_operation(new_op_i);
                qcir.get_gate(gate_ids[j])->set_operation(new_op_j);
                spdlog::trace("    Merged gate {}: {} {}", gate_ids[i], qcir.get_gate(gate_ids[i])->get_operation().get_repr(), rotations[i].phase());
            }
        }
    }

    for (size_t i = 0; i < rotations.size(); ++i) {
        if (rotations[i].phase() == dvlab::Phase(0)) {
            qcir.remove_gate(gate_ids[i]);
        }
    }

    remove_identities_teleport(rotations, gate_ids);
}

/**
 * @brief Conjugation view specifically for phase teleport optimization, and not for general use.
 *
 */
class ConjugationView : public tableau::PauliProductTrait<ConjugationView> {
public:
    ConjugationView(
        tableau::PauliRotationTableau& rotations,
        size_t upto) : _rotations{rotations}, _upto{upto} {}

    ConjugationView& h(size_t qubit) noexcept override {
        for (size_t i = 0; i < _upto; ++i) {
            _rotations.get()[i].h(qubit);
        }
        return *this;
    }

    ConjugationView& s(size_t qubit) noexcept override {
        for (size_t i = 0; i < _upto; ++i) {
            _rotations.get()[i].s(qubit);
        }
        return *this;
    }

    ConjugationView& cx(size_t control, size_t target) noexcept override {
        for (size_t i = 0; i < _upto; ++i) {
            _rotations.get()[i].cx(control, target);
        }
        return *this;
    }

private:
    std::reference_wrapper<tableau::PauliRotationTableau> _rotations;
    size_t _upto;
};

/**
 * @brief remove Clifford rotations from the Pauli rotation tableau and delete the tracking of corresponding gate ids.
 *
 * @param rotations
 * @param gate_ids
 */
void remove_clifford_rotations_teleport(
    tableau::PauliRotationTableau& rotations,
    std::vector<size_t>& gate_ids) {
    for (size_t i = 0; i < rotations.size(); ++i) {
        if (rotations[i].phase().denominator() >= 2 || rotations[i].phase() == dvlab::Phase(0)) {
            continue;
        }

        auto conjugation_view = ConjugationView{rotations, i};

        auto [ops, qubit] = tableau::extract_clifford_operators(rotations[i]);

        conjugation_view.apply(ops);

        if (rotations[i].phase() == dvlab::Phase(1, 2)) {
            conjugation_view.s(qubit);
        } else if (rotations[i].phase() == dvlab::Phase(-1, 2)) {
            conjugation_view.sdg(qubit);
        } else {
            assert(rotations[i].phase() == dvlab::Phase(1));
            conjugation_view.z(qubit);
        }
        rotations[i].phase() = dvlab::Phase(0);

        adjoint_inplace(ops);

        conjugation_view.apply(ops);
    }

    remove_identities_teleport(rotations, gate_ids);
}

}  // namespace

/**
 * @brief merge phases of operations without changing placements of other gates.
 *
 * @param qcir
 */
void phase_teleport(QCir& qcir) {
    auto tabl_opt = to_tableau_with_gate_ids(qcir);
    if (!tabl_opt.has_value()) {
        return;
    }
    auto [rotations, gate_ids] = *std::move(tabl_opt);

    auto n_rotations = SIZE_MAX;

    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        n_rotations = rotations.size();
        merge_rotations_teleport(qcir, rotations, gate_ids);
        remove_clifford_rotations_teleport(rotations, gate_ids);
    } while (rotations.size() < n_rotations);
}

}  // namespace qsyn::qcir
