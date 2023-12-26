/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define class QCir Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_to_tensor.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
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

/**
 * @brief Update tensor pin
 *
 * @param pins
 * @param tmp
 */
void update_tensor_pin(Qubit2TensorPinMap &qubit2pin, std::vector<QubitInfo> const &pins, QTensor<double> &main, QTensor<double> const &gate) {
    spdlog::trace("Pin Permutation");
    for (auto &[qubit, pin] : qubit2pin) {
        std::string trace = fmt::format("  - Qubit: {} input : {} -> ", qubit, pin.first);
        pin.first         = main.get_new_axis_id(pin.first);
        trace += fmt::format("{} output: {} -> ", pin.first, pin.second);

        bool connected  = false;
        bool target     = false;
        size_t ith_ctrl = 0;
        for (size_t i = 0; i < pins.size(); i++) {
            if (pins[i]._qubit == qubit) {
                connected = true;
                if (pins[i]._isTarget)
                    target = true;
                else
                    ith_ctrl = i;
                break;
            }
        }
        if (connected) {
            if (target)
                pin.second = main.get_new_axis_id(main.dimension() + gate.dimension() - 1);

            else
                pin.second = main.get_new_axis_id(main.dimension() + 2 * ith_ctrl + 1);
        } else
            pin.second = main.get_new_axis_id(pin.second);

        trace += fmt::format("{}", pin.second);
        spdlog::trace("{}", trace);
    }
}

std::optional<QTensor<double>> to_tensor(QCirGate *gate) {
    switch (gate->get_rotation_category()) {
        // single-qubit gates
        case GateRotationCategory::id:
            return QTensor<double>::identity(1);
        case GateRotationCategory::h:
            return QTensor<double>::hbox(2);
        case GateRotationCategory::swap: {
            auto tensor = QTensor<double>{{1.0, 0.0, 0.0, 0.0},
                                          {0.0, 0.0, 1.0, 0.0},
                                          {0.0, 1.0, 0.0, 0.0},
                                          {0.0, 0.0, 0.0, 1.0}};
            tensor.reshape({2, 2, 2, 2});
            return tensor;
        }
        case GateRotationCategory::pz:
            return QTensor<double>::control(QTensor<double>::pzgate(gate->get_phase()), gate->get_num_qubits() - 1);
        case GateRotationCategory::rz:
            return QTensor<double>::control(QTensor<double>::rzgate(gate->get_phase()), gate->get_num_qubits() - 1);
        case GateRotationCategory::px:
            return QTensor<double>::control(QTensor<double>::pxgate(gate->get_phase()), gate->get_num_qubits() - 1);
        case GateRotationCategory::rx:
            return QTensor<double>::control(QTensor<double>::rxgate(gate->get_phase()), gate->get_num_qubits() - 1);
        case GateRotationCategory::py:
            return QTensor<double>::control(QTensor<double>::pygate(gate->get_phase()), gate->get_num_qubits() - 1);
        case GateRotationCategory::ry:
            return QTensor<double>::control(QTensor<double>::rygate(gate->get_phase()), gate->get_num_qubits() - 1);

        default:
            return std::nullopt;
    }
};

/**
 * @brief Convert QCir to tensor
 */
std::optional<QTensor<double>> to_tensor(QCir const &qcir) {
    if (qcir.get_qubits().empty()) {
        spdlog::warn("QCir is empty!!");
        return std::nullopt;
    }
    qcir.update_topological_order();
    spdlog::debug("Add boundary");

    QTensor<double> tensor;

    // NOTE: Constucting an identity(_qubit.size()) takes much time and memory.
    //       To make this process interruptible by SIGINT (ctrl-C), we grow the qubit size one by one
    try {
        for (size_t i = 0; i < qcir.get_qubits().size(); ++i) {
            if (stop_requested()) {
                spdlog::warn("Conversion interrupted.");
                return std::nullopt;
            }
            tensor = tensordot(tensor, QTensor<double>::identity(1));
        }
    } catch (std::bad_alloc const &e) {
        spdlog::error("Memory allocation failed!!");
        return std::nullopt;
    }

    Qubit2TensorPinMap qubit2pin;
    for (size_t i = 0; i < qcir.get_qubits().size(); i++) {
        qubit2pin[qcir.get_qubits()[i]->get_id()] = std::make_pair(2 * i, 2 * i + 1);
        spdlog::trace("  - Add Qubit {} input port: {}", qcir.get_qubits()[i]->get_id(), 2 * i);
    }

    try {
        qcir.topological_traverse([&tensor, &qubit2pin](QCirGate *gate) {
            if (stop_requested()) return;
            spdlog::debug("Gate {} ({})", gate->get_id(), gate->get_type_str());
            auto tmp = to_tensor(gate);
            assert(tmp.has_value());
            std::vector<size_t> ori_pin;
            std::vector<size_t> new_pin;
            for (size_t np = 0; np < gate->get_qubits().size(); np++) {
                new_pin.emplace_back(2 * np);
                auto const info = gate->get_qubits()[np];
                ori_pin.emplace_back(qubit2pin[info._qubit].second);
            }
            tensor = tensordot(tensor, *tmp, ori_pin, new_pin);
            update_tensor_pin(qubit2pin, gate->get_qubits(), tensor, *tmp);
        });
    } catch (std::bad_alloc const &e) {
        spdlog::error("Memory allocation failed!!");
        return std::nullopt;
    }

    if (stop_requested()) {
        spdlog::warn("Conversion interrupted.");
        return std::nullopt;
    }

    std::vector<size_t> input_pin, output_pin;
    for (size_t i = 0; i < qcir.get_qubits().size(); i++) {
        input_pin.emplace_back(qubit2pin[qcir.get_qubits()[i]->get_id()].first);
        output_pin.emplace_back(qubit2pin[qcir.get_qubits()[i]->get_id()].second);
    }
    try {
        tensor = tensor.to_matrix(input_pin, output_pin);
    } catch (std::bad_alloc const &e) {
        spdlog::error("Memory allocation failed!!");
        return std::nullopt;
    }

    return tensor;
}

}  // namespace qsyn
