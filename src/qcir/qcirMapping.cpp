/****************************************************************************
  FileName     [ qcir.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define QCir Mapping functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

#include "tensorMgr.h"
#include "zxGraphMgr.h"

using namespace std;
extern ZXGraphMgr *zxGraphMgr;
extern TensorMgr *tensorMgr;
extern size_t verbose;

/**
 * @brief Clear mapping
 */
void QCir::clearMapping() {
    for (size_t i = 0; i < _ZXGraphList.size(); i++) {
        cerr << "Note: Graph " << _ZXGraphList[i]->getId() << " is deleted due to modification(s) !!" << endl;
        _ZXGraphList[i]->reset();
        zxGraphMgr->removeZXGraph(_ZXGraphList[i]->getId());
    }
    _ZXGraphList.clear();
}

/**
 * @brief Mapping QCir to ZX-graph
 */
void QCir::ZXMapping() {
    if (zxGraphMgr == 0) {
        cerr << "Error: ZXMODE is OFF, please turn on before mapping" << endl;
        return;
    }
    if (verbose >= 3) cout << "Traverse and build the graph... " << endl;
    updateTopoOrder();

    ZXGraph *_ZXG = zxGraphMgr->addZXGraph(zxGraphMgr->getNextID());
    _ZXG->setRef((void **)_ZXG);

    if (verbose >= 5) cout << "> Add boundaries" << endl;
    for (size_t i = 0; i < _qubits.size(); i++) {
        ZXVertex *input = _ZXG->addInput(_qubits[i]->getId());
        ZXVertex *output = _ZXG->addOutput(_qubits[i]->getId());
        _ZXG->addEdge(input, output, EdgeType(EdgeType::SIMPLE));
    }

    
    topoTraverse([this, _ZXG](QCirGate *G) {
        if (verbose >= 5) cout << "> Gate " << G->getId() << " (" << G->getTypeStr() << ")" << endl;
        ZXGraph *tmp = G->getZXform();
        if (tmp == NULL) {
            //TODO cleanup when conversion fails
            cerr << "Gate " << G->getId() << " (type: " << G->getTypeStr() << ") is not implemented, the conversion result is wrong!!" << endl;
            return;
        }

        _ZXG->concatenate(tmp, false);
        tmp->disownVertices();
        delete tmp;
    });

    _ZXGraphList.push_back(_ZXG);
}

/**
 * @brief Convert QCir to tensor
 */
void QCir::tensorMapping() {
    if (verbose >= 3) cout << "Traverse and build the tensor... " << endl;
    updateTopoOrder();
    if (verbose >= 5) cout << "> Add boundary" << endl;
    if (!tensorMgr) tensorMgr = new TensorMgr();
    size_t id = tensorMgr->nextID();
    _tensor = tensorMgr->addTensor(id, "QC");
    *_tensor = tensordot(*_tensor, QTensor<double>::identity(_qubits.size()));
    _qubit2pin.clear();
    for (size_t i = 0; i < _qubits.size(); i++) {
        _qubit2pin[_qubits[i]->getId()] = make_pair(2 * i, 2 * i + 1);
        if (verbose >= 8) cout << "  - Add Qubit " << _qubits[i]->getId() << " output port: " << 2 * i + 1 << endl;
    }

    topoTraverse([this](QCirGate *G) {
        if (verbose >= 5) cout << "> Gate " << G->getId() << " (" << G->getTypeStr() << ")" << endl;
        QTensor<double> tmp = G->getTSform();
        vector<size_t> ori_pin;
        vector<size_t> new_pin;
        ori_pin.clear();
        new_pin.clear();
        for (size_t np = 0; np < G->getQubits().size(); np++) {
            new_pin.push_back(2 * np);
            BitInfo info = G->getQubits()[np];
            ori_pin.push_back(_qubit2pin[info._qubit].second);
        }
        *_tensor = tensordot(*_tensor, tmp, ori_pin, new_pin);
        updateTensorPin(G->getQubits(), tmp);
    });

    vector<size_t> input_pin, output_pin;
    for (size_t i = 0; i < _qubits.size(); i++) {
        input_pin.push_back(_qubit2pin[_qubits[i]->getId()].first);
        output_pin.push_back(_qubit2pin[_qubits[i]->getId()].second);
    }
    *_tensor = _tensor->toMatrix(input_pin, output_pin);
    cout << "Stored the resulting tensor as tensor id " << id << endl;
}

/**
 * @brief Update tensor pin
 *
 * @param pins
 * @param tmp
 */
void QCir::updateTensorPin(vector<BitInfo> pins, QTensor<double> tmp) {
    // size_t count_pin_used = 0;
    // unordered_map<size_t, size_t> table; // qid to pin (pin0 = ctrl 0 pin1 = ctrl 1)
    if (verbose >= 8) cout << "> Pin Permutation" << endl;
    for (auto it = _qubit2pin.begin(); it != _qubit2pin.end(); ++it) {
        if (verbose >= 8) {
            // NOTE print old input axis id
            cout << "  - Qubit: " << it->first << " input : " << it->second.first << " -> ";
        }
        it->second.first = _tensor->getNewAxisId(it->second.first);
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
                it->second.second = _tensor->getNewAxisId(_tensor->dimension() + tmp.dimension() - 1);

            else
                it->second.second = _tensor->getNewAxisId(_tensor->dimension() + 2 * ithCtrl + 1);
        } else
            it->second.second = _tensor->getNewAxisId(it->second.second);

        if (verbose >= 8) {
            // NOTE print new axis id
            cout << it->second.second << endl;
        }
    }
}