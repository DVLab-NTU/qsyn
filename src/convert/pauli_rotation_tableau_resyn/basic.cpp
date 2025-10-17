#include "convert/tableau_to_qcir.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/phase.hpp"

namespace qsyn::tableau {

// dirty code, TODO: clean up
std::optional<qcir::QCir>
BasicPauliRotationsSynthesisStrategy::_partial_synthesize(
    PauliRotationTableau const& rotations, StabilizerTableau& residual_clifford, bool backward) const {
    auto pop_target_rotation = [&](PauliRotationTableau& rots) {
        auto rot = backward ? std::move(rots.back()) : std::move(rots.front());
        if (backward) {
            rots.pop_back();
        } else {
            rots.erase(rots.begin());
        }
        return rot;
    };

    if (rotations.empty()) {
        return qcir::QCir{0};
    }

    size_t num_qubits = rotations.front().n_qubits();

    auto qcir           = qcir::QCir{num_qubits};
    auto copy_rotations = rotations;

    while (!copy_rotations.empty()) {
        auto target_rotation = pop_target_rotation(copy_rotations);
        auto [ops, qubit]    = extract_clifford_operators(target_rotation);
        Phase phase          = target_rotation.phase();

        for (auto op : ops) {
            detail::add_clifford_gate(copy_rotations, op);
            if (!backward) {
                detail::add_clifford_gate(qcir, op);
            } else {
                detail::add_clifford_gate(residual_clifford, op);
            }
        }

        if (!backward) {
            qcir.append(qcir::PZGate(phase), {qubit});
        }

        adjoint_inplace(ops);
        for (auto op : std::views::reverse(ops)) {
            if (!backward) {
                detail::prepend_clifford_gate(residual_clifford, op);
            } else {
                detail::prepend_clifford_gate(qcir, op);
            }
        }

        if (backward) {
            qcir.prepend(qcir::PZGate(phase), {qubit});
        }
    }

    return qcir;
}

std::optional<qcir::QCir>
BasicPauliRotationsSynthesisStrategy::synthesize(
    PauliRotationTableau const& rotations) const {
    auto partial_result = partial_synthesize(rotations);
    if (!partial_result) {
        return std::nullopt;
    }

    auto [qcir, final_clifford] = std::move(*partial_result);

    auto const final_clifford_circ = to_qcir(final_clifford, AGSynthesisStrategy{});
    if (!final_clifford_circ) {
        return std::nullopt;
    }
    qcir.compose(*final_clifford_circ);

    return qcir;
}

}  // namespace qsyn::tableau
