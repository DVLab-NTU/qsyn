/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define class QCir Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_to_tensor.hpp"

#include <cstddef>
#include <thread>

#include "fmt/core.h"
#include "qcir/qcir.hpp"
#include "qcir/qcir_qubit.hpp"
#include "tensor/qtensor.hpp"
#include "util/logger.hpp"

extern dvlab::Logger LOGGER;
extern bool stop_requested();

namespace qsyn {

using Qubit2TensorPinMap = std::unordered_map<size_t, std::pair<size_t, size_t>>;

using qsyn::tensor::QTensor;

/**
 * @brief Update tensor pin
 *
 * @param pins
 * @param tmp
 */
void update_tensor_pin(Qubit2TensorPinMap &qubit2pin, std::vector<QubitInfo> const &pins, QTensor<double> &main, QTensor<double> const &gate) {
    LOGGER.trace("Pin Permutation");
    for (auto &[qubit, pin] : qubit2pin) {
        std::string trace = fmt::format("  - Qubit: {} input : {} -> ", qubit, pin.first);
        pin.first = main.get_new_axis_id(pin.first);
        trace += fmt::format("{} output: {} -> ", pin.first, pin.second);

        bool connected = false;
        bool target = false;
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
        LOGGER.trace("{}", trace);
    }
}

std::optional<QTensor<double>> to_tensor(QCirGate *gate) {
    switch (gate->get_type()) {
        // single-qubit gates
        case GateType::h:
            return QTensor<double>::hbox(2);
        case GateType::z:
            return QTensor<double>::zgate();
        case GateType::p:
            return QTensor<double>::pzgate(gate->get_phase());
        case GateType::rz:
            return QTensor<double>::rzgate(gate->get_phase());
        case GateType::s:
            return QTensor<double>::pzgate(dvlab::Phase(1, 2));
        case GateType::t:
            return QTensor<double>::pzgate(dvlab::Phase(1, 4));
        case GateType::sdg:
            return QTensor<double>::pzgate(dvlab::Phase(-1, 2));
        case GateType::tdg:
            return QTensor<double>::pzgate(dvlab::Phase(-1, 4));
        case GateType::x:
            return QTensor<double>::xgate();
        case GateType::px:
            return QTensor<double>::pxgate(gate->get_phase());
        case GateType::rx:
            return QTensor<double>::rxgate(gate->get_phase());
        case GateType::sx:
            return QTensor<double>::pxgate(dvlab::Phase(1, 2));
        case GateType::y:
            return QTensor<double>::ygate();
        case GateType::py:
            return QTensor<double>::pygate(gate->get_phase());
        case GateType::ry:
            return QTensor<double>::rygate(gate->get_phase());
        case GateType::sy:
            return QTensor<double>::pygate(dvlab::Phase(1, 2));
            // double-qubit gates

        case GateType::cx:
            return QTensor<double>::control(QTensor<double>::xgate(), 1);
        case GateType::cz:
            return QTensor<double>::control(QTensor<double>::zgate(), 1);
        // case GateType::SWAP:
        //     return detail::getSwapZXform(gate);

        // multi-qubit gates
        case GateType::mcrz:
            return QTensor<double>::control(QTensor<double>::rzgate(gate->get_phase()), gate->get_qubits().size() - 1);
        case GateType::mcp:
            return QTensor<double>::control(QTensor<double>::pzgate(gate->get_phase()), gate->get_qubits().size() - 1);
        case GateType::ccz:
            return QTensor<double>::control(QTensor<double>::zgate(), 2);
        case GateType::ccx:
            return QTensor<double>::control(QTensor<double>::xgate(), 2);
        case GateType::mcrx:
            return QTensor<double>::control(QTensor<double>::rxgate(gate->get_phase()), gate->get_qubits().size() - 1);
        case GateType::mcpx:
            return QTensor<double>::control(QTensor<double>::pxgate(gate->get_phase()), gate->get_qubits().size() - 1);
        case GateType::mcry:
            return QTensor<double>::control(QTensor<double>::rygate(gate->get_phase()), gate->get_qubits().size() - 1);
        case GateType::mcpy:
            return QTensor<double>::control(QTensor<double>::pygate(gate->get_phase()), gate->get_qubits().size() - 1);

        default:
            return std::nullopt;
    }
};

/**
 * @brief Convert QCir to tensor
 */
std::optional<QTensor<double>> to_tensor(QCir const &qcir) {
    qcir.update_topological_order();
    LOGGER.debug("Add boundary");

    QTensor<double> tensor;

    // NOTE: Constucting an identity(_qubit.size()) takes much time and memory.
    //       To make this process interruptible by SIGINT (ctrl-C), we grow the qubit size one by one
    for (size_t i = 0; i < qcir.get_qubits().size(); ++i) {
        if (stop_requested()) {
            std::cerr << "Warning: conversion interrupted." << std::endl;
            return std::nullopt;
        }
        tensor = tensordot(tensor, QTensor<double>::identity(1));
    }

    Qubit2TensorPinMap qubit2pin;
    for (size_t i = 0; i < qcir.get_qubits().size(); i++) {
        qubit2pin[qcir.get_qubits()[i]->get_id()] = std::make_pair(2 * i, 2 * i + 1);
        LOGGER.trace("  - Add Qubit {} input port: {}", qcir.get_qubits()[i]->get_id(), 2 * i);
    }

    qcir.topological_traverse([&tensor, &qubit2pin](QCirGate *gate) {
        if (stop_requested()) return;
        LOGGER.debug("Gate {} ({})", gate->get_id(), gate->get_type_str());
        auto tmp = to_tensor(gate);
        assert(tmp.has_value());
        std::vector<size_t> ori_pin;
        std::vector<size_t> new_pin;
        for (size_t np = 0; np < gate->get_qubits().size(); np++) {
            new_pin.emplace_back(2 * np);
            QubitInfo info = gate->get_qubits()[np];
            ori_pin.emplace_back(qubit2pin[info._qubit].second);
        }
        tensor = tensordot(tensor, *tmp, ori_pin, new_pin);
        update_tensor_pin(qubit2pin, gate->get_qubits(), tensor, *tmp);
    });

    if (stop_requested()) {
        std::cerr << "Warning: conversion interrupted." << std::endl;
        return std::nullopt;
    }

    std::vector<size_t> input_pin, output_pin;
    for (size_t i = 0; i < qcir.get_qubits().size(); i++) {
        input_pin.emplace_back(qubit2pin[qcir.get_qubits()[i]->get_id()].first);
        output_pin.emplace_back(qubit2pin[qcir.get_qubits()[i]->get_id()].second);
    }
    tensor = tensor.to_matrix(input_pin, output_pin);

    return tensor;
}

}  // namespace qsyn