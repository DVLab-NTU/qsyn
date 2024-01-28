/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_to_tableau.hpp"

#include <gsl/narrow>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>

#include "qcir/gate_type.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/tableau.hpp"
#include "util/phase.hpp"

namespace qsyn {

namespace experimental {

QCir2TableauResultType& QCir2TableauResultType::h(size_t qubit) {
    clifford.h(qubit);
    for (auto& pauli : pauli_rotations) {
        pauli.h(qubit);
    }
    return *this;
}

QCir2TableauResultType& QCir2TableauResultType::s(size_t qubit) {
    clifford.s(qubit);
    for (auto& pauli : pauli_rotations) {
        pauli.s(qubit);
    }
    return *this;
}

QCir2TableauResultType& QCir2TableauResultType::cx(size_t control, size_t target) {
    clifford.cx(control, target);
    for (auto& pauli : pauli_rotations) {
        pauli.cx(control, target);
    }
    return *this;
}

std::optional<QCir2TableauResultType> to_tableau(qcir::QCir const& qcir) {
    // check if all gates are Clifford
    auto const is_allowed_clifford = [](qcir::QCirGate const& gate) {
        return gate.get_type_str() == "h" ||
               gate.get_type_str() == "s" ||
               gate.get_type_str() == "sdg" ||
               gate.get_type_str() == "v" ||
               gate.get_type_str() == "vdg" ||
               gate.get_type_str() == "x" ||
               gate.get_type_str() == "y" ||
               gate.get_type_str() == "z" ||
               gate.get_type_str() == "cx" ||
               gate.get_type_str() == "cz" ||
               gate.get_type_str() == "swap";
    };

    auto const is_rz_like = [](qcir::QCirGate const& gate) {
        return (gate.get_rotation_category() == qcir::GateRotationCategory::rz || gate.get_rotation_category() == qcir::GateRotationCategory::pz) && gate.get_num_qubits() == 1;
    };

    for (auto const& gate : qcir.get_gates()) {
        if (!is_allowed_clifford(*gate) && !is_rz_like(*gate)) {
            spdlog::error("Gate ID {} of type {} is not allowed in at the moment!!", gate->get_id(), gate->get_type_str());
            return std::nullopt;
        }
    }

    QCir2TableauResultType result{StabilizerTableau(qcir.get_num_qubits()), {}};

    for (auto const& gate : qcir.get_gates()) {
        if (gate->get_type_str() == "h") {
            result.h(gate->get_qubits()[0]._qubit);
        } else if (gate->get_type_str() == "s") {
            result.s(gate->get_qubits()[0]._qubit);
        } else if (gate->get_type_str() == "sdg") {
            result.sdg(gate->get_qubits()[0]._qubit);
        } else if (gate->get_type_str() == "v") {
            result.v(gate->get_qubits()[0]._qubit);
        } else if (gate->get_type_str() == "vdg") {
            result.vdg(gate->get_qubits()[0]._qubit);
        } else if (gate->get_type_str() == "x") {
            result.x(gate->get_qubits()[0]._qubit);
        } else if (gate->get_type_str() == "y") {
            result.y(gate->get_qubits()[0]._qubit);
        } else if (gate->get_type_str() == "z") {
            result.z(gate->get_qubits()[0]._qubit);
        } else if (gate->get_type_str() == "cx") {
            result.cx(gate->get_qubits()[0]._qubit, gate->get_qubits()[1]._qubit);
        } else if (gate->get_type_str() == "cz") {
            result.cz(gate->get_qubits()[0]._qubit, gate->get_qubits()[1]._qubit);
        } else if (gate->get_type_str() == "swap") {
            result.swap(gate->get_qubits()[0]._qubit, gate->get_qubits()[1]._qubit);
        } else if (is_rz_like(*gate)) {
            auto pauli_range = std::views::iota(0ul, qcir.get_num_qubits()) |
                               std::views::transform([&gate](auto i) -> Pauli {
                                   return i == gsl::narrow<size_t>(gate->get_qubits()[0]._qubit) ? Pauli::Z : Pauli::I;
                               }) |
                               tl::to<std::vector>();
            result.pauli_rotations.push_back(PauliRotation(pauli_range.begin(), pauli_range.end(), gate->get_phase()));
        } else {
            spdlog::error("Gate {} is not allowed in at the moment!!", gate->get_type_str());
            return std::nullopt;
        }
    }

    return result;
}

}  // namespace experimental

}  // namespace qsyn
