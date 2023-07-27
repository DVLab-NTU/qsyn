/****************************************************************************
  FileName     [ qcirMapping.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <thread>

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
void QCir::ZXMapping(std::stop_token st) {
    updateGateTime();
    ZXGraph bufferGraph;

    bufferGraph.setFileName(_fileName);
    bufferGraph.addProcedure(_procedures);
    bufferGraph.addProcedure("QC2ZX");

    if (verbose >= 5) cout << "Traverse and build the graph... " << endl;

    if (verbose >= 5) cout << "\n> Add boundaries" << endl;
    for (size_t i = 0; i < _qubits.size(); i++) {
        ZXVertex *input = bufferGraph.addInput(_qubits[i]->getId());
        ZXVertex *output = bufferGraph.addOutput(_qubits[i]->getId());
        input->setCol(0);
        bufferGraph.addEdge(input, output, EdgeType::SIMPLE);
    }

    topoTraverse([st, &bufferGraph](QCirGate *gate) {
        if (st.stop_requested()) return;
        if (verbose >= 8) cout << "\n";
        if (verbose >= 5) cout << "> Gate " << gate->getId() << " (" << gate->getTypeStr() << ")" << endl;
        ZXGraph tmp = gate->getZXform();

        for (auto &v : tmp.getVertices()) {
            v->setCol(v->getCol() + gate->getTime() + gate->getDelay());
        }

        bufferGraph.concatenate(tmp);
    });

    size_t max = 0;
    for (auto &v : bufferGraph.getOutputs()) {
        size_t neighborCol = v->getFirstNeighbor().first->getCol();
        if (neighborCol > max) {
            max = neighborCol;
        }
    }
    for (auto &v : bufferGraph.getOutputs()) {
        v->setCol(max + 1);
    }

    if (st.stop_requested()) {
        cerr << "Warning: conversion interrupted." << endl;
    }

    ZXGraph *newGraph = zxGraphMgr.add(zxGraphMgr.getNextID());
    zxGraphMgr.set(std::make_unique<ZXGraph>(std::move(bufferGraph)));

    _ZXGraphList.emplace_back(zxGraphMgr.get());
}

/**
 * @brief Convert QCir to tensor
 */
void QCir::tensorMapping(std::stop_token st) {
    if (verbose >= 3) cout << "Traverse and build the tensor... " << endl;
    updateTopoOrder();
    if (verbose >= 5) cout << "> Add boundary" << endl;

    QTensor<double> *tensor = new QTensor<double>;

    // NOTE: Constucting an identity(_qubit.size()) takes much time and memory.
    //       To make this process interruptible by SIGINT (ctrl-C), we grow the qubit size one by one
    for (size_t i = 0; i < _qubits.size(); ++i) {
        if (st.stop_requested()) {
            cerr << "Warning: conversion interrupted." << endl;
            delete tensor;
            return;
        }
        *tensor = tensordot(*tensor, QTensor<double>::identity(1));
    }

    _qubit2pin.clear();
    for (size_t i = 0; i < _qubits.size(); i++) {
        _qubit2pin[_qubits[i]->getId()] = make_pair(2 * i, 2 * i + 1);
        if (verbose >= 8) cout << "  - Add Qubit " << _qubits[i]->getId() << " output port: " << 2 * i + 1 << endl;
    }

    topoTraverse([st, tensor, this](QCirGate *gate) {
        if (st.stop_requested()) return;
        if (verbose >= 5) cout << "> Gate " << gate->getId() << " (" << gate->getTypeStr() << ")" << endl;
        QTensor<double> tmp = gate->getTSform();
        vector<size_t> ori_pin;
        vector<size_t> new_pin;
        ori_pin.clear();
        new_pin.clear();
        for (size_t np = 0; np < gate->getQubits().size(); np++) {
            new_pin.emplace_back(2 * np);
            BitInfo info = gate->getQubits()[np];
            ori_pin.emplace_back(_qubit2pin[info._qubit].second);
        }
        *tensor = tensordot(*tensor, tmp, ori_pin, new_pin);
        updateTensorPin(gate->getQubits(), *tensor, tmp);
    });

    if (st.stop_requested()) {
        cerr << "Warning: conversion interrupted." << endl;
        return;
    }

    vector<size_t> input_pin, output_pin;
    for (size_t i = 0; i < _qubits.size(); i++) {
        input_pin.emplace_back(_qubit2pin[_qubits[i]->getId()].first);
        output_pin.emplace_back(_qubit2pin[_qubits[i]->getId()].second);
    }
    *tensor = tensor->toMatrix(input_pin, output_pin);

    if (!tensorMgr) tensorMgr = new TensorMgr();

    auto id = tensorMgr->nextID();
    tensorMgr->addTensor(id, "QC");
    tensorMgr->setTensor(id, tensor);

    cout << "Stored the resulting tensor as tensor id " << id << endl;
}

/**
 * @brief Update tensor pin
 *
 * @param pins
 * @param tmp
 */
void QCir::updateTensorPin(vector<BitInfo> const &pins, QTensor<double> &main, QTensor<double> const &gate) {
    // size_t count_pin_used = 0;
    // unordered_map<size_t, size_t> table; // qid to pin (pin0 = ctrl 0 pin1 = ctrl 1)
    if (verbose >= 8) cout << "> Pin Permutation" << endl;
    for (auto it = _qubit2pin.begin(); it != _qubit2pin.end(); ++it) {
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