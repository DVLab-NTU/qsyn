/****************************************************************************
  FileName     [ qcirMapping.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <thread>

#include "cmdParser.h"
#include "qcir.h"
#include "qcirGate.h"
#include "qcirQubit.h"
#include "qtensor.h"
#include "tensorMgr.h"
#include "zxGraph.h"
#include "zxGraphMgr.h"

using namespace std;
extern ZXGraphMgr zxGraphMgr;
extern TensorMgr *tensorMgr;
extern size_t verbose;

using Qubit2TensorPinMap = std::unordered_map<size_t, std::pair<size_t, size_t>>;

/**
 * @brief Clear mapping
 */
void QCir::clearMapping() {
    for (size_t i = 0; i < _ZXGraphList.size(); i++) {
        cerr << "Note: Graph " << _ZXGraphList[i]->getId() << " is deleted due to modification(s) !!" << endl;
        zxGraphMgr.remove(_ZXGraphList[i]->getId());
    }
    _ZXGraphList.clear();
}

/**
 * @brief Mapping QCir to ZXGraph
 */
std::optional<ZXGraph> QCir::toZX() {
    updateGateTime();
    ZXGraph g;

    if (verbose >= 5) cout << "Traverse and build the graph... " << endl;

    if (verbose >= 5) cout << "\n> Add boundaries" << endl;
    for (size_t i = 0; i < _qubits.size(); i++) {
        ZXVertex *input = g.addInput(_qubits[i]->getId());
        ZXVertex *output = g.addOutput(_qubits[i]->getId());
        input->setCol(0);
        g.addEdge(input, output, EdgeType::SIMPLE);
    }

    topoTraverse([&g](QCirGate *gate) {
        if (cli.stop_requested()) return;
        if (verbose >= 8) cout << "\n";
        if (verbose >= 5) cout << "> Gate " << gate->getId() << " (" << gate->getTypeStr() << ")" << endl;
        ZXGraph tmp = gate->getZXform();

        for (auto &v : tmp.getVertices()) {
            v->setCol(v->getCol() + gate->getTime() + gate->getDelay());
        }

        g.concatenate(tmp);
    });

    size_t max = 0;
    for (auto &v : g.getOutputs()) {
        size_t neighborCol = v->getFirstNeighbor().first->getCol();
        if (neighborCol > max) {
            max = neighborCol;
        }
    }
    for (auto &v : g.getOutputs()) {
        v->setCol(max + 1);
    }

    if (cli.stop_requested()) {
        cerr << "Warning: conversion interrupted." << endl;
        return std::nullopt;
    }

    return g;
}

/**
 * @brief Update tensor pin
 *
 * @param pins
 * @param tmp
 */
void updateTensorPin(Qubit2TensorPinMap &qubit2pin, vector<BitInfo> const &pins, QTensor<double> &main, QTensor<double> const &gate) {
    // size_t count_pin_used = 0;
    // unordered_map<size_t, size_t> table; // qid to pin (pin0 = ctrl 0 pin1 = ctrl 1)
    if (verbose >= 8) cout << "> Pin Permutation" << endl;
    for (auto it = qubit2pin.begin(); it != qubit2pin.end(); ++it) {
        if (verbose >= 8) {
            // NOTE print old input axis id
            cout << "  - Qubit: " << it->first << " input : " << it->second.first << " -> ";
        }
        it->second.first = main.getNewAxisId(it->second.first);
        if (verbose >= 8) {
            // NOTE print new input axis id
            cout << it->second.first << " | ";
            // NOTE print new output axis id
            cout << " output: " << it->second.second << " -> ";
        }

        bool connected = false;
        bool target = false;
        size_t ithCtrl = 0;
        for (size_t i = 0; i < pins.size(); i++) {
            if (pins[i]._qubit == it->first) {
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
                it->second.second = main.getNewAxisId(main.dimension() + gate.dimension() - 1);

            else
                it->second.second = main.getNewAxisId(main.dimension() + 2 * ithCtrl + 1);
        } else
            it->second.second = main.getNewAxisId(it->second.second);

        if (verbose >= 8) {
            // NOTE print new axis id
            cout << it->second.second << endl;
        }
    }
}

/**
 * @brief Convert QCir to tensor
 */
std::optional<QTensor<double>> QCir::toTensor() {
    if (verbose >= 3) cout << "Traverse and build the tensor... " << endl;
    updateTopoOrder();
    if (verbose >= 5) cout << "> Add boundary" << endl;

    QTensor<double> tensor;

    // NOTE: Constucting an identity(_qubit.size()) takes much time and memory.
    //       To make this process interruptible by SIGINT (ctrl-C), we grow the qubit size one by one
    for (size_t i = 0; i < _qubits.size(); ++i) {
        if (cli.stop_requested()) {
            cerr << "Warning: conversion interrupted." << endl;
            return std::nullopt;
        }
        tensor = tensordot(tensor, QTensor<double>::identity(1));
    }

    Qubit2TensorPinMap qubit2pin;
    for (size_t i = 0; i < _qubits.size(); i++) {
        qubit2pin[_qubits[i]->getId()] = make_pair(2 * i, 2 * i + 1);
        if (verbose >= 8) cout << "  - Add Qubit " << _qubits[i]->getId() << " output port: " << 2 * i + 1 << endl;
    }

    topoTraverse([&tensor, &qubit2pin](QCirGate *gate) {
        if (cli.stop_requested()) return;
        if (verbose >= 5) cout << "> Gate " << gate->getId() << " (" << gate->getTypeStr() << ")" << endl;
        QTensor<double> tmp = gate->getTSform();
        vector<size_t> ori_pin;
        vector<size_t> new_pin;
        ori_pin.clear();
        new_pin.clear();
        for (size_t np = 0; np < gate->getQubits().size(); np++) {
            new_pin.emplace_back(2 * np);
            BitInfo info = gate->getQubits()[np];
            ori_pin.emplace_back(qubit2pin[info._qubit].second);
        }
        tensor = tensordot(tensor, tmp, ori_pin, new_pin);
        updateTensorPin(qubit2pin, gate->getQubits(), tensor, tmp);
    });

    if (cli.stop_requested()) {
        cerr << "Warning: conversion interrupted." << endl;
        return std::nullopt;
    }

    vector<size_t> input_pin, output_pin;
    for (size_t i = 0; i < _qubits.size(); i++) {
        input_pin.emplace_back(qubit2pin[_qubits[i]->getId()].first);
        output_pin.emplace_back(qubit2pin[_qubits[i]->getId()].second);
    }
    tensor = tensor.toMatrix(input_pin, output_pin);

    // cout << "Stored the resulting tensor as tensor id " << id << endl;
    return tensor;
}
