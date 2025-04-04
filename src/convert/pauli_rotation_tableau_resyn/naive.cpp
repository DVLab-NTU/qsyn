
#include "convert/tableau_to_qcir.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/phase.hpp"

namespace qsyn::tableau {

std::optional<qcir::QCir>
NaivePauliRotationsSynthesisStrategy::synthesize(
    std::vector<PauliRotation> const& rotations) const {
    if (rotations.empty()) {
        return qcir::QCir{0};
    }

    auto qcir = qcir::QCir{rotations.front().n_qubits()};

    for (auto const& rotation : rotations) {
        auto [ops, qubit] = extract_clifford_operators(rotation);

        for (auto const& op : ops) {
            detail::add_clifford_gate(qcir, op);
        }

        qcir.append(qcir::PZGate(rotation.phase()), {qubit});

        adjoint_inplace(ops);

        for (auto const& op : ops) {
            detail::add_clifford_gate(qcir, op);
        }
    }

    return qcir;
}

}  // namespace qsyn::tableau
