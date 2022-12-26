/****************************************************************************
  FileName     [ qcirGate2ZX.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate mapping functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "qcir.h"
#include "zxGraph.h"

extern size_t verbose;

/// @brief map single qubit gate to zx
/// @param vt
/// @param ph
/// @return
ZXGraph *QCirGate::mapSingleQubitGate(VertexType vt, Phase ph) {
    ZXGraph *g = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if (verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;
    ZXVertex *in = g->addInput(qubit);
    ZXVertex *gate = g->addVertex(qubit, vt, ph);
    ZXVertex *out = g->addOutput(qubit);
    g->addEdge(in, gate, EdgeType::SIMPLE);
    g->addEdge(gate, out, EdgeType::SIMPLE);
    g->setInputHash(qubit, in);
    g->setOutputHash(qubit, out);
    if (verbose >= 5) cout << "***********************************" << endl;
    return g;
}

/**
 * @brief Make combination of `k` from `verVec`
 *
 * @param verVec
 * @param k
 * @return vector<vector<ZXVertex* > >
 */
vector<vector<ZXVertex *>> QCirGate::makeCombi(vector<ZXVertex *> verVec, int k) {
    vector<vector<ZXVertex *>> comb;
    vector<ZXVertex *> tmp;
    makeCombiUtil(comb, tmp, verVec, 0, k);
    return comb;
}

/**
 * @brief Make combination of `k` from `verVec`.
 *        Function that will be called in `makeCombi`
 *
 * @param comb
 * @param tmp
 * @param verVec
 * @param left
 * @param k
 */
void QCirGate::makeCombiUtil(vector<vector<ZXVertex *>> &comb, vector<ZXVertex *> &tmp, vector<ZXVertex *> verVec, int left, int k) {
    if (k == 0) {
        comb.push_back(tmp);
        return;
    }
    for (int i = left; i < (int)verVec.size(); ++i) {
        tmp.push_back(verVec[i]);
        makeCombiUtil(comb, tmp, verVec, i + 1, k - 1);
        tmp.pop_back();
    }
}

// Double or More Qubit Gate

/**
 * @brief get ZX-graph of CX
 *
 * @return ZXGraph*
 */
ZXGraph *CXGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;
    if (verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;
    ZXVertex *in_ctrl = temp->addInput(ctrl_qubit);
    ZXVertex *in_targ = temp->addInput(targ_qubit);
    ZXVertex *ctrl = temp->addVertex(ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex *targX = temp->addVertex(targ_qubit, VertexType::X, Phase(0));
    ZXVertex *out_ctrl = temp->addOutput(ctrl_qubit);
    ZXVertex *out_targ = temp->addOutput(targ_qubit);
    temp->addEdge(in_ctrl, ctrl, EdgeType::SIMPLE);
    temp->addEdge(ctrl, out_ctrl, EdgeType::SIMPLE);
    temp->addEdge(in_targ, targX, EdgeType::SIMPLE);
    temp->addEdge(targX, out_targ, EdgeType::SIMPLE);
    temp->addEdge(ctrl, targX, EdgeType::SIMPLE);
    temp->setInputHash(ctrl_qubit, in_ctrl);
    temp->setOutputHash(ctrl_qubit, out_ctrl);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    if (verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

/**
 * @brief get ZX-graph of CCX
 *        Decomposed into 21 vertices (6X + 6Z + 4T + 3Tdg + 2H)
 *
 * @return ZXGraph*
 */
ZXGraph *CCXGate::getZXform()  //
{
    ZXGraph *temp = new ZXGraph(_id);

    size_t ctrl_qubit_1 = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t ctrl_qubit_2 = _qubits[0]._isTarget ? _qubits[2]._qubit : (_qubits[1]._isTarget ? _qubits[2]._qubit : _qubits[1]._qubit);
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : (_qubits[1]._isTarget ? _qubits[1]._qubit : _qubits[2]._qubit);

    // Build vertices table and adjacency pairs of edges
    vector<pair<pair<VertexType, Phase>, size_t>> Vertices_info = {{{VertexType::H_BOX, Phase(1)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, ctrl_qubit_2}, {{VertexType::H_BOX, Phase(1)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::Z, Phase(-1, 4)}, ctrl_qubit_1}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, ctrl_qubit_2}};

    vector<pair<size_t, size_t>> adj_pair = {{0, 2}, {1, 2}, {2, 3}, {4, 5}, {3, 5}, {5, 6}, {6, 8}, {1, 7}, {7, 8}, {8, 9}, {9, 11}, {10, 11}, {4, 10}, {10, 14}, {11, 13}, {7, 12}, {14, 15}, {12, 15}, {13, 16}, {14, 17}, {17, 19}, {15, 18}, {18, 20}, {19, 20}};

    vector<ZXVertex *> Vertices_list = {};

    if (verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;
    ZXVertex *in_ctrl_1 = temp->addInput(ctrl_qubit_1);
    ZXVertex *in_ctrl_2 = temp->addInput(ctrl_qubit_2);
    ZXVertex *in_targ = temp->addInput(targ_qubit);
    for (size_t i = 0; i < Vertices_info.size(); i++) {
        Vertices_list.push_back(temp->addVertex(Vertices_info[i].second, Vertices_info[i].first.first, Vertices_info[i].first.second));
    }
    ZXVertex *out_ctrl_1 = temp->addOutput(ctrl_qubit_1);
    ZXVertex *out_ctrl_2 = temp->addOutput(ctrl_qubit_2);
    ZXVertex *out_targ = temp->addOutput(targ_qubit);

    temp->addEdge(in_ctrl_1, Vertices_list[4], EdgeType::SIMPLE);
    temp->addEdge(in_ctrl_2, Vertices_list[1], EdgeType::SIMPLE);
    temp->addEdge(in_targ, Vertices_list[0], EdgeType::SIMPLE);
    temp->addEdge(out_ctrl_1, Vertices_list[19], EdgeType::SIMPLE);
    temp->addEdge(out_ctrl_2, Vertices_list[20], EdgeType::SIMPLE);
    temp->addEdge(out_targ, Vertices_list[16], EdgeType::SIMPLE);
    for (size_t i = 0; i < adj_pair.size(); i++) {
        temp->addEdge(Vertices_list[adj_pair[i].first], Vertices_list[adj_pair[i].second], EdgeType::SIMPLE);
    }

    temp->setInputHash(ctrl_qubit_1, in_ctrl_1);
    temp->setOutputHash(ctrl_qubit_1, out_ctrl_1);
    temp->setInputHash(ctrl_qubit_2, in_ctrl_2);
    temp->setOutputHash(ctrl_qubit_2, out_ctrl_2);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    if (verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

/**
 * @brief get ZX-graph of CZ
 *
 * @return ZXGraph*
 */
ZXGraph *CZGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;
    if (verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;
    ZXVertex *in_ctrl = temp->addInput(ctrl_qubit);
    ZXVertex *in_targ = temp->addInput(targ_qubit);
    ZXVertex *ctrl = temp->addVertex(ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex *targZ = temp->addVertex(targ_qubit, VertexType::Z, Phase(0));
    ZXVertex *out_ctrl = temp->addOutput(ctrl_qubit);
    ZXVertex *out_targ = temp->addOutput(targ_qubit);
    temp->addEdge(in_ctrl, ctrl, EdgeType::SIMPLE);
    temp->addEdge(ctrl, out_ctrl, EdgeType::SIMPLE);
    temp->addEdge(in_targ, targZ, EdgeType::SIMPLE);
    temp->addEdge(targZ, out_targ, EdgeType::SIMPLE);
    temp->addEdge(ctrl, targZ, EdgeType(EdgeType::HADAMARD));
    temp->setInputHash(ctrl_qubit, in_ctrl);
    temp->setOutputHash(ctrl_qubit, out_ctrl);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    if (verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

// Y Gate
// NOTE - Cannot use mapSingleQubitGate

/**
 * @brief get ZX-graph of Y = iXZ
 *
 * @return ZXGraph*
 */
ZXGraph *YGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if (verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;
    ZXVertex *in = temp->addInput(qubit);
    ZXVertex *X = temp->addVertex(qubit, VertexType::X, Phase(1));
    ZXVertex *Z = temp->addVertex(qubit, VertexType::Z, Phase(1));
    ZXVertex *out = temp->addOutput(qubit);
    temp->addEdge(in, X, EdgeType::SIMPLE);
    temp->addEdge(X, Z, EdgeType::SIMPLE);
    temp->addEdge(Z, out, EdgeType::SIMPLE);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if (verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

/**
 * @brief get ZX-graph of SY = S。SX。Sdg
 *
 * @return ZXGraph*
 */
ZXGraph *SYGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if (verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;
    ZXVertex *in = temp->addInput(qubit);
    ZXVertex *S = temp->addVertex(qubit, VertexType::Z, Phase(1, 2));
    ZXVertex *SX = temp->addVertex(qubit, VertexType::X, Phase(1, 2));
    ZXVertex *Sdg = temp->addVertex(qubit, VertexType::Z, Phase(-1, 2));
    ZXVertex *out = temp->addOutput(qubit);
    temp->addEdge(in, S, EdgeType::SIMPLE);
    temp->addEdge(S, SX, EdgeType::SIMPLE);
    temp->addEdge(SX, Sdg, EdgeType::SIMPLE);
    temp->addEdge(Sdg, out, EdgeType::SIMPLE);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if (verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

/**
 * @brief get ZX-graph of cnp
 *
 * @return ZXGraph*
 */
ZXGraph *CnPGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    Phase phase = Phase(1, pow(2, _qubits.size() - 1));
    Rational ratio = _rotatePhase / Phase(1);
    phase = phase * ratio;
    if (verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;
    vector<ZXVertex *> verVec;
    for (const auto bitinfo : _qubits) {
        size_t qubit = bitinfo._qubit;
        ZXVertex *in = temp->addInput(qubit);
        ZXVertex *v = temp->addVertex(qubit, VertexType::Z, phase);
        ZXVertex *out = temp->addOutput(qubit);
        temp->addEdge(in, v, EdgeType::SIMPLE);
        temp->addEdge(v, out, EdgeType::SIMPLE);
        temp->setInputHash(qubit, in);
        temp->setOutputHash(qubit, out);
        verVec.push_back(v);
    }

    for (size_t k = 2; k <= verVec.size(); k++) {
        vector<vector<ZXVertex *>> comb = makeCombi(verVec, k);
        for (size_t i = 0; i < comb.size(); i++) {
            if (k % 2)
                temp->addGadget(phase, comb[i]);
            else
                temp->addGadget(-phase, comb[i]);
        }
    }
    return temp;
}

/**
 * @brief get ZX-graph of crz
 *
 * @return ZXGraph*
 */
// TODO - Implentment the MCRZ version.
ZXGraph *CrzGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    Phase phase = Phase(1, pow(2, _qubits.size() - 1));
    Rational ratio = _rotatePhase / Phase(1);
    phase = phase * ratio;
    if (verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;
    vector<ZXVertex *> verVec;
    for (const auto bitinfo : _qubits) {
        size_t qubit = bitinfo._qubit;
        ZXVertex *in = temp->addInput(qubit);
        ZXVertex *v = temp->addVertex(qubit, VertexType::Z, bitinfo._isTarget ? phase : Phase(0));
        ZXVertex *out = temp->addOutput(qubit);
        temp->addEdge(in, v, EdgeType::SIMPLE);
        temp->addEdge(v, out, EdgeType::SIMPLE);
        temp->setInputHash(qubit, in);
        temp->setOutputHash(qubit, out);
        verVec.push_back(v);
    }

    for (size_t k = 2; k <= verVec.size(); k++) {
        vector<vector<ZXVertex *>> comb = makeCombi(verVec, k);
        for (size_t i = 0; i < comb.size(); i++) {
            if (k % 2)
                temp->addGadget(phase, comb[i]);
            else
                temp->addGadget(-phase, comb[i]);
        }
    }
    return temp;
}

/**
 * @brief get ZX-graph of CnRX
 *
 * @return ZXGraph*
 */
ZXGraph *CnRXGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    Phase phase = Phase(1, pow(2, _qubits.size() - 1));
    Rational ratio = _rotatePhase / Phase(1);
    phase = phase * ratio;
    if (verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;
    vector<ZXVertex *> verVec;
    size_t targetQubit = _qubits[_qubits.size() - 1]._qubit;
    for (const auto bitinfo : _qubits) {
        size_t qubit = bitinfo._qubit;
        ZXVertex *in = temp->addInput(qubit);
        ZXVertex *v = temp->addVertex(qubit, VertexType::Z, phase);
        ZXVertex *out = temp->addOutput(qubit);
        if (qubit != targetQubit) {
            temp->addEdge(in, v, EdgeType::SIMPLE);
            temp->addEdge(v, out, EdgeType::SIMPLE);
        } else {
            temp->addEdge(in, v, EdgeType::HADAMARD);
            temp->addEdge(v, out, EdgeType::HADAMARD);
        }
        temp->setInputHash(qubit, in);
        temp->setOutputHash(qubit, out);
        verVec.push_back(v);
    }

    for (size_t k = 2; k <= verVec.size(); k++) {
        vector<vector<ZXVertex *>> comb = makeCombi(verVec, k);
        for (size_t i = 0; i < comb.size(); i++) {
            if (k % 2)
                temp->addGadget(phase, comb[i]);
            else
                temp->addGadget(-phase, comb[i]);
        }
    }
    return temp;
}
