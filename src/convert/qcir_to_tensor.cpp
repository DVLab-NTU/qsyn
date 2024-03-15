/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define class QCir Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_to_tensor.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <gsl/narrow>
#include <stdexcept>
#include <thread>

#include "fmt/core.h"
#include "qcir/gate_type.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_qubit.hpp"
#include "qsyn/qsyn_type.hpp"
#include "tensor/qtensor.hpp"

extern bool stop_requested();

namespace qsyn {

using Qubit2TensorPinMap = std::unordered_map<QubitIdType, std::pair<size_t, size_t>>;

using qsyn::tensor::QTensor;

template <>
std::optional<QTensor<double>> to_tensor(LegacyGateType const& op) {
    switch (op.get_rotation_category()) {
        case GateRotationCategory::id:
            return QTensor<double>::identity(1);
        case GateRotationCategory::h:
            return QTensor<double>::hbox(2);
        case GateRotationCategory::swap: {
            auto tensor = QTensor<double>{{1.0, 0.0, 0.0, 0.0},
                                          {0.0, 0.0, 1.0, 0.0},
                                          {0.0, 1.0, 0.0, 0.0},
                                          {0.0, 0.0, 0.0, 1.0}};
            return tensor.to_qtensor();
        }
        case GateRotationCategory::ecr: {
            using namespace std::complex_literals;
            auto tensor = QTensor<double>{{0.0, 0.0, 1.0 / std::sqrt(2), 1.i / std::sqrt(2)},
                                          {0.0, 0.0, 1.i / std::sqrt(2), 1.0 / std::sqrt(2)},
                                          {1.0 / std::sqrt(2), -1.i / std::sqrt(2), 0.0, 0.0},
                                          {-1.i / std::sqrt(2), 1.0 / std::sqrt(2), 0.0, 0.0}};
            return tensor.to_qtensor();
        }
        case GateRotationCategory::pz:
            return QTensor<double>::control(QTensor<double>::pzgate(op.get_phase()), op.get_num_qubits() - 1);
        case GateRotationCategory::rz:
            return QTensor<double>::control(QTensor<double>::rzgate(op.get_phase()), op.get_num_qubits() - 1);
        case GateRotationCategory::px:
            return QTensor<double>::control(QTensor<double>::pxgate(op.get_phase()), op.get_num_qubits() - 1);
        case GateRotationCategory::rx:
            return QTensor<double>::control(QTensor<double>::rxgate(op.get_phase()), op.get_num_qubits() - 1);
        case GateRotationCategory::py:
            return QTensor<double>::control(QTensor<double>::pygate(op.get_phase()), op.get_num_qubits() - 1);
        case GateRotationCategory::ry:
            return QTensor<double>::control(QTensor<double>::rygate(op.get_phase()), op.get_num_qubits() - 1);

        default:
            return std::nullopt;
    }
}

/**
 * @brief Convert gate to tensor
 *
 * @param gate
 * @return std::optional<QTensor<double>>
 */
std::optional<QTensor<double>> to_tensor(QCirGate const& gate) {
    return to_tensor(gate.get_operation());
};

namespace {

/**
 * @brief Update tensor pin
 *
 * @param qubit2pin map of reordering qubit to pin
 * @param qubit_infos qubit infos of the new gate
 * @param gate new gate
 * @param main main tensor
 */
void update_tensor_pin(Qubit2TensorPinMap& qubit2pin, QCirGate const& gate, QTensor<double> const& gate_tensor, QTensor<double>& main) {
    spdlog::trace("Pin Permutation");
    for (auto& [qubit, pin] : qubit2pin) {
        auto const [old_out, old_in] = pin;
        auto& [new_out, new_in]      = pin;

        auto operands = gate.get_qubits();

        auto const it = std::ranges::find_if(operands, [qubit = qubit](auto qb) { return qb == qubit; });

        if (it != operands.end()) {
            auto ith_ctrl = std::distance(operands.begin(), it);
            new_out       = main.get_new_axis_id(2 * ith_ctrl);
        } else {
            new_out = main.get_new_axis_id(gate_tensor.dimension() + old_out);
        }
        // NOTE - Order of axis [ gate out/in/out/in... | main out/in/out/in...]
        new_in = main.get_new_axis_id(gate_tensor.dimension() + old_in);
        spdlog::trace("  - Qubit: {} input : {} -> {} output: {} -> {}", qubit, old_in, new_in, old_out, new_out);
    }
}

}  // namespace

/**
 * @brief Convert QCir to tensor
 *
 * @param qcir
 * @return std::optional<QTensor<double>>
 */
std::optional<QTensor<double>> to_tensor(QCir const& qcir) try {
    if (qcir.get_qubits().empty()) {
        spdlog::warn("QCir is empty!!");
        return std::nullopt;
    }
    spdlog::debug("Add boundary");

    QTensor<double> tensor;

    // Constructing an identity with large number of qubits takes much time and memory.
    // To make this process interruptible by SIGINT (ctrl-C), we grow the qubit size one by one
    for (size_t i = 0; i < qcir.get_num_qubits(); ++i) {
        if (stop_requested()) {
            spdlog::warn("Conversion interrupted.");
            return std::nullopt;
        }
        tensor = tensordot(tensor, QTensor<double>::identity(1));
    }

    Qubit2TensorPinMap qubit_to_pins;  // qubit -> (output, input)
    for (auto const& qubit_id : qcir.get_qubits() | std::views::transform([](auto const& qubit) { return qubit->get_id(); })) {
        auto const oi_pair = std::make_pair(2 * qubit_id, 2 * qubit_id + 1);
        qubit_to_pins.emplace(qubit_id, oi_pair);
        spdlog::trace("  - Add Qubit: {} input: {} output: {}", qubit_id, oi_pair.second, oi_pair.first);
    }

    for (auto const& gate : qcir.get_gates()) {
        if (stop_requested()) {
            spdlog::warn("Conversion interrupted.");
            return std::nullopt;
        }
        spdlog::debug("Gate {} ({})", gate->get_id(), gate->get_type_str());
        auto const gate_tensor = to_tensor(*gate);
        if (!gate_tensor.has_value()) {
            spdlog::error("Conversion of Gate {} ({}) to Tensor is not supported yet!!", gate->get_id(), gate->get_type_str());
            return std::nullopt;
        }
        std::vector<size_t> main_tensor_output_pins;
        std::vector<size_t> gate_tensor_input_pins;
        for (size_t np = 0; np < gate->get_num_qubits(); np++) {
            gate_tensor_input_pins.emplace_back(2 * np + 1);
            auto const qubit_id = gate->get_qubit(np);
            main_tensor_output_pins.emplace_back(qubit_to_pins[qubit_id].first);
        }
        // [tmp]x[tensor]
        tensor = tensordot(*gate_tensor, tensor, gate_tensor_input_pins, main_tensor_output_pins);
        update_tensor_pin(qubit_to_pins, *gate, *gate_tensor, tensor);
    }

    if (stop_requested()) {
        spdlog::warn("Conversion interrupted.");
        return std::nullopt;
    }

    std::vector<size_t> output_pins, input_pins;
    for (auto const& [output, input] : qcir.get_qubits() | std::views::transform([&](auto const& qubit) { return qubit_to_pins[qubit->get_id()]; })) {
        output_pins.emplace_back(output);
        input_pins.emplace_back(input);
    }

    tensor = tensor.to_matrix(output_pins, input_pins);

    return tensor;
} catch (std::bad_alloc const& e) {
    spdlog::error("Memory allocation failed!!");
    return std::nullopt;
}

}  // namespace qsyn
