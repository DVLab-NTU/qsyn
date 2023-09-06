/****************************************************************************
  FileName     [ qcirMapping.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./toTensor.hpp"

#include <cstddef>
#include <thread>

#include "./qcir.hpp"
#include "./qcirQubit.hpp"
#include "fmt/core.h"
#include "util/logger.hpp"

using namespace std;

extern dvlab::utils::Logger logger;
extern bool stop_requested();

using Qubit2TensorPinMap = std::unordered_map<size_t, std::pair<size_t, size_t>>;

/**
 * @brief Update tensor pin
 *
 * @param pins
 * @param tmp
 */
void updateTensorPin(Qubit2TensorPinMap &qubit2pin, vector<BitInfo> const &pins, QTensor<double> &main, QTensor<double> const &gate) {
    logger.trace("Pin Permutation");
    for (auto &[qubit, pin] : qubit2pin) {
        std::string trace = fmt::format("  - Qubit: {} input : {} -> ", qubit, pin.first);
        pin.first = main.getNewAxisId(pin.first);
        trace += fmt::format("{} output: {} -> ", pin.first, pin.second);

        bool connected = false;
        bool target = false;
        size_t ithCtrl = 0;
        for (size_t i = 0; i < pins.size(); i++) {
            if (pins[i]._qubit == qubit) {
                connected = true;
                if (pins[i]._isTarget)
                    target = true;
                else
                    ithCtrl = i;
                break;
            }
        }
        if (connected) {
            if (target)
                pin.second = main.getNewAxisId(main.dimension() + gate.dimension() - 1);

            else
                pin.second = main.getNewAxisId(main.dimension() + 2 * ithCtrl + 1);
        } else
            pin.second = main.getNewAxisId(pin.second);

        trace += fmt::format("{}", pin.second);
        logger.trace("{}", trace);
    }
}

std::optional<QTensor<double>> toTensor(QCirGate *gate) {
    switch (gate->getType()) {
        // single-qubit gates
        case GateType::H:
            return QTensor<double>::hbox(2);
        case GateType::Z:
            return QTensor<double>::zgate();
        case GateType::P:
            return QTensor<double>::pzgate(gate->getPhase());
        case GateType::RZ:
            return QTensor<double>::rzgate(gate->getPhase());
        case GateType::S:
            return QTensor<double>::pzgate(Phase(1, 2));
        case GateType::T:
            return QTensor<double>::pzgate(Phase(1, 4));
        case GateType::SDG:
            return QTensor<double>::pzgate(Phase(-1, 2));
        case GateType::TDG:
            return QTensor<double>::pzgate(Phase(-1, 4));
        case GateType::X:
            return QTensor<double>::xgate();
        case GateType::PX:
            return QTensor<double>::pxgate(gate->getPhase());
        case GateType::RX:
            return QTensor<double>::rxgate(gate->getPhase());
        case GateType::SX:
            return QTensor<double>::pxgate(Phase(1, 2));
        case GateType::Y:
            return QTensor<double>::ygate();
        case GateType::PY:
            return QTensor<double>::pygate(gate->getPhase());
        case GateType::RY:
            return QTensor<double>::rygate(gate->getPhase());
        case GateType::SY:
            return QTensor<double>::pygate(Phase(1, 2));
            // double-qubit gates

        case GateType::CX:
            return QTensor<double>::control(QTensor<double>::xgate(), 1);
        case GateType::CZ:
            return QTensor<double>::control(QTensor<double>::zgate(), 1);
        // case GateType::SWAP:
        //     return detail::getSwapZXform(gate);

        // multi-qubit gates
        case GateType::MCRZ:
            return QTensor<double>::control(QTensor<double>::rzgate(gate->getPhase()), gate->getQubits().size() - 1);
        case GateType::MCP:
            return QTensor<double>::control(QTensor<double>::pzgate(gate->getPhase()), gate->getQubits().size() - 1);
        case GateType::CCZ:
            return QTensor<double>::control(QTensor<double>::zgate(), 2);
        case GateType::CCX:
            return QTensor<double>::control(QTensor<double>::xgate(), 2);
        case GateType::MCRX:
            return QTensor<double>::control(QTensor<double>::rxgate(gate->getPhase()), gate->getQubits().size() - 1);
        case GateType::MCPX:
            return QTensor<double>::control(QTensor<double>::pxgate(gate->getPhase()), gate->getQubits().size() - 1);
        case GateType::MCRY:
            return QTensor<double>::control(QTensor<double>::rygate(gate->getPhase()), gate->getQubits().size() - 1);
        case GateType::MCPY:
            return QTensor<double>::control(QTensor<double>::pygate(gate->getPhase()), gate->getQubits().size() - 1);

        default:
            return std::nullopt;
    }
};

/**
 * @brief Convert QCir to tensor
 */
std::optional<QTensor<double>> toTensor(QCir const &qcir) {
    qcir.updateTopoOrder();
    logger.debug("Add boundary");

    QTensor<double> tensor;

    // NOTE: Constucting an identity(_qubit.size()) takes much time and memory.
    //       To make this process interruptible by SIGINT (ctrl-C), we grow the qubit size one by one
    for (size_t i = 0; i < qcir.getQubits().size(); ++i) {
        if (stop_requested()) {
            cerr << "Warning: conversion interrupted." << endl;
            return std::nullopt;
        }
        tensor = tensordot(tensor, QTensor<double>::identity(1));
    }

    Qubit2TensorPinMap qubit2pin;
    for (size_t i = 0; i < qcir.getQubits().size(); i++) {
        qubit2pin[qcir.getQubits()[i]->getId()] = make_pair(2 * i, 2 * i + 1);
        logger.trace("  - Add Qubit {} input port: {}", qcir.getQubits()[i]->getId(), 2 * i);
    }

    qcir.topoTraverse([&tensor, &qubit2pin](QCirGate *gate) {
        if (stop_requested()) return;
        logger.debug("Gate {} ({})", gate->getId(), gate->getTypeStr());
        auto tmp = toTensor(gate);
        assert(tmp.has_value());
        vector<size_t> ori_pin;
        vector<size_t> new_pin;
        for (size_t np = 0; np < gate->getQubits().size(); np++) {
            new_pin.emplace_back(2 * np);
            BitInfo info = gate->getQubits()[np];
            ori_pin.emplace_back(qubit2pin[info._qubit].second);
        }
        tensor = tensordot(tensor, *tmp, ori_pin, new_pin);
        updateTensorPin(qubit2pin, gate->getQubits(), tensor, *tmp);
    });

    if (stop_requested()) {
        cerr << "Warning: conversion interrupted." << endl;
        return std::nullopt;
    }

    vector<size_t> input_pin, output_pin;
    for (size_t i = 0; i < qcir.getQubits().size(); i++) {
        input_pin.emplace_back(qubit2pin[qcir.getQubits()[i]->getId()].first);
        output_pin.emplace_back(qubit2pin[qcir.getQubits()[i]->getId()].second);
    }
    tensor = tensor.toMatrix(input_pin, output_pin);

    return tensor;
}
