#include "convert/tableau_to_qcir.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/phase.hpp"

namespace qsyn::tableau {

std::optional<qcir::QCir>
BasicPauliRotationsSynthesisStrategy::synthesize(
    PauliRotationTableau const& rotations) const {
    if (rotations.empty()) {
        return qcir::QCir{0};
    }

    size_t num_qubits = rotations.front().n_qubits();
    
    auto qcir = qcir::QCir{num_qubits};
    auto final_clifford = StabilizerTableau{num_qubits};
    auto copy_rotations = rotations;

    while (!copy_rotations.empty()) {
        auto [ops, qubit] = extract_clifford_operators(copy_rotations.front());
        Phase phase = copy_rotations.front().phase();
        copy_rotations.erase(copy_rotations.begin());
        
        for (auto op : ops) {
            detail::add_clifford_gate(qcir, op);
            detail::add_clifford_gate(copy_rotations, op);
        }
        qcir.append(qcir::PZGate(phase), {qubit});
        adjoint_inplace(ops);
        for (auto op : std::views::reverse(ops)) {
            detail::prepend_clifford_gate(final_clifford, op);
        }
    }

    auto const final_clifford_circ = to_qcir(final_clifford, AGSynthesisStrategy{});
    if (!final_clifford_circ) {
        return std::nullopt;
    }
    qcir.compose(*final_clifford_circ);

    return qcir;
}

}  // namespace qsyn::tableau