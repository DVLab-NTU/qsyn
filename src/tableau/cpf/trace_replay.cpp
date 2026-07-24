/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Build an interleaved Tableau from a QCir, preserving the
                 original Clifford / non-Clifford segmentation. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./trace_replay.hpp"

#include <spdlog/spdlog.h>

#include <variant>

#include "convert/qcir_to_tableau.hpp"
#include "qcir/operation.hpp"
#include "qcir/qcir_gate.hpp"
#include "tableau/stabilizer_tableau.hpp"

namespace qsyn::experimental::cpf {

std::optional<Tableau> trace_replay(qcir::QCir const& qcir) {
    Tableau result{qcir.get_num_qubits()};
    // The initial subtableau pushed by Tableau{n_qubits} is a Clifford
    // (identity) StabilizerTableau, so we start in the Clifford phase.
    bool prev_clifford = true;

    for (auto const& gate : qcir.get_gates()) {
        bool const gate_clifford = qcir::is_clifford(gate->get_operation());

        // Whenever we transition from a non-Clifford rotation block to a
        // new Clifford gate, push a fresh identity StabilizerTableau at the
        // back so the new Clifford does not get absorbed into the previous
        // segment (which would defeat the whole purpose of trace_replay).
        //
        // We do NOT push anything when transitioning the other way; the
        // existing `implement_mcr/implement_mcp` helpers already start a
        // new rotation list when they see a StabilizerTableau at the back.
        if (gate_clifford && !prev_clifford) {
            result.push_back(StabilizerTableau{result.n_qubits()});
        }

        if (!append_to_tableau(gate->get_operation(), result, gate->get_qubits())) {
            spdlog::error("Gate type {} is not supported by trace_replay!!", gate->get_operation().get_type());
            return std::nullopt;
        }

        prev_clifford = gate_clifford;
    }

    return result;
}

}  // namespace qsyn::experimental::cpf
