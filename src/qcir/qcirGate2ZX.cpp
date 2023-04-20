/****************************************************************************
  FileName     [ qcirGate2ZX.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t

#include "phase.h"           // for Phase, operator/
#include "qcirGate.h"        // for QCirGate...
#include "rationalNumber.h"  // for Rational
#include "zxDef.h"           // for VertexType, VertexType::Z, VertexType::X
#include "zxGraph.h"         // for ZXGraph, ZXVertex (ptr only)

using namespace std;

extern size_t verbose;

namespace detail {

Phase getGadgetPhase(Phase const &rotatePhase, size_t nQubits) {
    return rotatePhase * Rational(1, pow(2, nQubits - 1));
}

enum class RotationAxis {
    X,
    Y,
    Z
};

pair<vector<ZXVertex *>, ZXVertex *>
MC_GenBackbone(ZXGraph *g, vector<BitInfo> const &qubits, RotationAxis ax) {
    vector<ZXVertex *> controls;
    ZXVertex *target;
    for (auto const &bitinfo : qubits) {
        size_t qubit = bitinfo._qubit;
        ZXVertex *in = g->addInput(qubit);
        ZXVertex *v = g->addVertex(qubit, VertexType::Z);
        ZXVertex *out = g->addOutput(qubit);
        if (ax == RotationAxis::Z || !bitinfo._isTarget) {
            g->addEdge(in, v, EdgeType::SIMPLE);
            g->addEdge(v, out, EdgeType::SIMPLE);
        } else {
            g->addEdge(in, v, EdgeType::HADAMARD);
            g->addEdge(v, out, EdgeType::HADAMARD);
            if (ax == RotationAxis::Y) {
                g->addBuffer(in, v, EdgeType::HADAMARD)->setPhase(Phase(1, 2));
                g->addBuffer(out, v, EdgeType::HADAMARD)->setPhase(Phase(-1, 2));
            }
        }
        if (!bitinfo._isTarget)
            controls.push_back(v);
        else
            target = v;
    }

    assert(target != nullptr);

    return {controls, target};
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
void makeCombiUtil(vector<vector<ZXVertex *>> &comb, vector<ZXVertex *> &tmp, vector<ZXVertex *> const &verVec, int left, int k) {
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

/**
 * @brief Make combination of `k` from `verVec`
 *
 * @param verVec
 * @param k
 * @return vector<vector<ZXVertex* > >
 */
vector<vector<ZXVertex *>> makeCombi(vector<ZXVertex *> const &verVec, int k) {
    vector<vector<ZXVertex *>> comb;
    vector<ZXVertex *> tmp;
    makeCombiUtil(comb, tmp, verVec, 0, k);
    return comb;
}

void MCR_GenGadgets(ZXGraph *g, vector<ZXVertex *> const &controls, ZXVertex *target, Phase const &phase) {
    target->setPhase(phase);
    for (size_t k = 1; k <= controls.size(); k++) {
        vector<vector<ZXVertex *>> combinations = makeCombi(controls, k);
        for (auto &combination : combinations) {
            combination.push_back(target);
            g->addGadget((combination.size() % 2) ? phase : -phase, combination);
        }
    }
}

void MCP_GenGadgets(ZXGraph *g, vector<ZXVertex *> const &vertices, Phase const &phase) {
    for (auto &v : vertices) {
        v->setPhase(phase);
    }
    for (size_t k = 2; k <= vertices.size(); k++) {
        vector<vector<ZXVertex *>> combinations = makeCombi(vertices, k);
        for (auto &combination : combinations) {
            g->addGadget((combination.size() % 2) ? phase : -phase, combination);
        }
    }
}

ZXGraph *MCR_Gen(vector<BitInfo> qubits, size_t id, Phase const &rotatePhase, RotationAxis ax) {
    ZXGraph *g = new ZXGraph(id);
    Phase phase = detail::getGadgetPhase(rotatePhase, qubits.size());

    auto [controls, target] = detail::MC_GenBackbone(g, qubits, ax);

    detail::MCR_GenGadgets(g, controls, target, phase);

    return g;
}

ZXGraph *MCP_Gen(vector<BitInfo> qubits, size_t id, Phase const &rotatePhase, RotationAxis ax) {
    ZXGraph *g = new ZXGraph(id);
    Phase phase = detail::getGadgetPhase(rotatePhase, qubits.size());

    auto [vertices, target] = detail::MC_GenBackbone(g, qubits, ax);
    vertices.push_back(target);

    detail::MCP_GenGadgets(g, vertices, phase);

    return g;
}

}  // namespace detail

/**
 * @brief Map single qubit gate to ZX-graph
 *
 * @param vt
 * @param ph
 * @return ZXGraph*
 */
ZXGraph *QCirGate::mapSingleQubitGate(VertexType vt, Phase ph) {
    ZXGraph *g = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;

    ZXVertex *in = g->addInput(qubit);
    ZXVertex *gate = g->addVertex(qubit, vt, ph);
    ZXVertex *out = g->addOutput(qubit);
    g->addEdge(in, gate, EdgeType::SIMPLE);
    g->addEdge(gate, out, EdgeType::SIMPLE);

    return g;
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

    return temp;
}

/**
 * @brief Cet ZX-graph of CCX.
 *        Decomposed into 21 vertices (6X + 6Z + 4T + 3Tdg + 2H)
 *
 * @return ZXGraph*
 */
ZXGraph *CCXGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);

    size_t ctrl_qubit_1 = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t ctrl_qubit_2 = _qubits[0]._isTarget ? _qubits[2]._qubit : (_qubits[1]._isTarget ? _qubits[2]._qubit : _qubits[1]._qubit);
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : (_qubits[1]._isTarget ? _qubits[1]._qubit : _qubits[2]._qubit);

    // Build vertices table and adjacency pairs of edges
    vector<pair<pair<VertexType, Phase>, size_t>> Vertices_info = {{{VertexType::H_BOX, Phase(1)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, ctrl_qubit_2}, {{VertexType::H_BOX, Phase(1)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::Z, Phase(-1, 4)}, ctrl_qubit_1}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, ctrl_qubit_2}};

    vector<pair<size_t, size_t>> adj_pair = {{0, 2}, {1, 2}, {2, 3}, {4, 5}, {3, 5}, {5, 6}, {6, 8}, {1, 7}, {7, 8}, {8, 9}, {9, 11}, {10, 11}, {4, 10}, {10, 14}, {11, 13}, {7, 12}, {14, 15}, {12, 15}, {13, 16}, {14, 17}, {17, 19}, {15, 18}, {18, 20}, {19, 20}};

    vector<ZXVertex *> Vertices_list = {};

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

    return temp;
}

// TODO - SWAP ZXForm
ZXGraph *SWAPGate::getZXform() {
    return nullptr;
}

/**
 * @brief Get ZX-graph of CZ
 *
 * @return ZXGraph*
 */
ZXGraph *CZGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;

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

    return temp;
}

// Y Gate
// NOTE - Cannot use mapSingleQubitGate

/**
 * @brief Get ZX-graph of Y = iXZ
 *
 * @return ZXGraph*
 */
ZXGraph *YGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;

    ZXVertex *in = temp->addInput(qubit);
    ZXVertex *X = temp->addVertex(qubit, VertexType::X, Phase(1));
    ZXVertex *Z = temp->addVertex(qubit, VertexType::Z, Phase(1));
    ZXVertex *out = temp->addOutput(qubit);
    temp->addEdge(in, X, EdgeType::SIMPLE);
    temp->addEdge(X, Z, EdgeType::SIMPLE);
    temp->addEdge(Z, out, EdgeType::SIMPLE);

    return temp;
}

/**
 * @brief Get ZX-graph of SY = S。SX。Sdg
 *
 * @return ZXGraph*
 */
ZXGraph *SYGate::getZXform() {
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;

    ZXVertex *in = temp->addInput(qubit);
    ZXVertex *S = temp->addVertex(qubit, VertexType::Z, Phase(1, 2));
    ZXVertex *SX = temp->addVertex(qubit, VertexType::X, Phase(1, 2));
    ZXVertex *Sdg = temp->addVertex(qubit, VertexType::Z, Phase(-1, 2));
    ZXVertex *out = temp->addOutput(qubit);
    temp->addEdge(in, S, EdgeType::SIMPLE);
    temp->addEdge(S, SX, EdgeType::SIMPLE);
    temp->addEdge(SX, Sdg, EdgeType::SIMPLE);
    temp->addEdge(Sdg, out, EdgeType::SIMPLE);

    return temp;
}

/**
 * @brief Get ZX-graph of MCPX
 *
 * @return ZXGraph*
 */
ZXGraph *MCPXGate::getZXform() {
    return detail::MCP_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::X);
}

/**
 * @brief Get ZX-graph of MCPY
 *
 * @return ZXGraph*
 */
ZXGraph *MCPYGate::getZXform() {
    return detail::MCP_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::Y);
}

/**
 * @brief Get ZX-graph of MCP
 *
 * @return ZXGraph*
 */
ZXGraph *MCPGate::getZXform() {
    return detail::MCP_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::Z);
}

/**
 * @brief Get ZX-graph of MCRX
 *
 * @return ZXGraph*
 */
ZXGraph *MCRXGate::getZXform() {
    return detail::MCR_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::X);
}

/**
 * @brief Get ZX-graph of MCRY
 *
 * @return ZXGraph*
 */
ZXGraph *MCRYGate::getZXform() {
    return detail::MCR_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::Y);
}

/**
 * @brief Get ZX-graph of MCRZ
 *
 * @return ZXGraph*
 */
ZXGraph *MCRZGate::getZXform() {
    return detail::MCR_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::Z);
}
