/****************************************************************************
  FileName     [ qcirGate2ZX.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>

#include "qcir/qcirGate.hpp"
#include "util/phase.hpp"
#include "util/rationalNumber.hpp"
#include "zx/zxDef.hpp"
#include "zx/zxGraph.hpp"

using namespace std;

extern size_t verbose;
extern size_t dmode;

namespace detail {

Phase getGadgetPhase(Phase const& rotatePhase, size_t nQubits) {
    return rotatePhase * Rational(1, pow(2, nQubits - 1));
}

enum class RotationAxis {
    X,
    Y,
    Z
};

pair<vector<ZXVertex*>, ZXVertex*>
MC_GenBackbone(ZXGraph& g, vector<BitInfo> const& qubits, RotationAxis ax) {
    vector<ZXVertex*> controls;
    ZXVertex* target;
    for (auto const& bitinfo : qubits) {
        size_t qubit = bitinfo._qubit;
        ZXVertex* in = g.addInput(qubit);
        ZXVertex* v = g.addVertex(qubit, VertexType::Z);
        ZXVertex* out = g.addOutput(qubit);
        if (ax == RotationAxis::Z || !bitinfo._isTarget) {
            g.addEdge(in, v, EdgeType::SIMPLE);
            g.addEdge(v, out, EdgeType::SIMPLE);
        } else {
            g.addEdge(in, v, EdgeType::HADAMARD);
            g.addEdge(v, out, EdgeType::HADAMARD);
            if (ax == RotationAxis::Y) {
                g.addBuffer(in, v, EdgeType::HADAMARD)->setPhase(Phase(1, 2));
                g.addBuffer(out, v, EdgeType::HADAMARD)->setPhase(Phase(-1, 2));
            }
        }
        if (!bitinfo._isTarget)
            controls.emplace_back(v);
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
void makeCombiUtil(vector<vector<ZXVertex*>>& comb, vector<ZXVertex*>& tmp, vector<ZXVertex*> const& verVec, int left, int k) {
    if (k == 0) {
        comb.emplace_back(tmp);
        return;
    }
    for (int i = left; i < (int)verVec.size(); ++i) {
        tmp.emplace_back(verVec[i]);
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
vector<vector<ZXVertex*>> makeCombi(vector<ZXVertex*> const& verVec, int k) {
    vector<vector<ZXVertex*>> comb;
    vector<ZXVertex*> tmp;
    makeCombiUtil(comb, tmp, verVec, 0, k);
    return comb;
}

void MCR_GenGadgets(ZXGraph& g, vector<ZXVertex*> const& controls, ZXVertex* target, Phase const& phase) {
    target->setPhase(phase);
    for (size_t k = 1; k <= controls.size(); k++) {
        vector<vector<ZXVertex*>> combinations = makeCombi(controls, k);
        for (auto& combination : combinations) {
            combination.emplace_back(target);
            g.addGadget((combination.size() % 2) ? phase : -phase, combination);
        }
    }
}

void MCP_GenGadgets(ZXGraph& g, vector<ZXVertex*> const& vertices, Phase const& phase) {
    for (auto& v : vertices) {
        v->setPhase(phase);
    }
    for (size_t k = 2; k <= vertices.size(); k++) {
        vector<vector<ZXVertex*>> combinations = makeCombi(vertices, k);
        for (auto& combination : combinations) {
            g.addGadget((combination.size() % 2) ? phase : -phase, combination);
        }
    }
}

ZXGraph MCR_Gen(vector<BitInfo> qubits, size_t id, Phase const& rotatePhase, RotationAxis ax) {
    ZXGraph g{id};
    Phase phase = detail::getGadgetPhase(rotatePhase, qubits.size());

    auto [controls, target] = detail::MC_GenBackbone(g, qubits, ax);

    detail::MCR_GenGadgets(g, controls, target, phase);

    return g;
}

ZXGraph MCP_Gen(vector<BitInfo> qubits, size_t id, Phase const& rotatePhase, RotationAxis ax) {
    ZXGraph g{id};
    Phase phase = detail::getGadgetPhase(rotatePhase, qubits.size());

    auto [vertices, target] = detail::MC_GenBackbone(g, qubits, ax);
    vertices.emplace_back(target);

    detail::MCP_GenGadgets(g, vertices, phase);

    return g;
}

}  // namespace detail

/**
 * @brief Map single qubit gate to ZXGraph
 *
 * @param vt
 * @param ph
 * @return ZXGraph
 */
ZXGraph QCirGate::mapSingleQubitGate(VertexType vt, Phase ph) {
    ZXGraph g{_id};
    size_t qubit = _qubits[0]._qubit;

    ZXVertex* in = g.addInput(qubit);
    ZXVertex* gate = g.addVertex(qubit, vt, ph);
    ZXVertex* out = g.addOutput(qubit);
    g.addEdge(in, gate, EdgeType::SIMPLE);
    g.addEdge(gate, out, EdgeType::SIMPLE);

    return g;
}

// Double or More Qubit Gate

/**
 * @brief get ZXGraph of CX
 *
 * @return ZXGraph
 */
ZXGraph CXGate::getZXform() {
    ZXGraph g{_id};
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;

    ZXVertex* in_ctrl = g.addInput(ctrl_qubit);
    ZXVertex* in_targ = g.addInput(targ_qubit);
    ZXVertex* ctrl = g.addVertex(ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex* targX = g.addVertex(targ_qubit, VertexType::X, Phase(0));
    ZXVertex* out_ctrl = g.addOutput(ctrl_qubit);
    ZXVertex* out_targ = g.addOutput(targ_qubit);
    g.addEdge(in_ctrl, ctrl, EdgeType::SIMPLE);
    g.addEdge(ctrl, out_ctrl, EdgeType::SIMPLE);
    g.addEdge(in_targ, targX, EdgeType::SIMPLE);
    g.addEdge(targX, out_targ, EdgeType::SIMPLE);
    g.addEdge(ctrl, targX, EdgeType::SIMPLE);

    return g;
}

/**
 * @brief Cet ZXGraph of CCX.
 *        Decomposed into 21 vertices (6X + 6Z + 4T + 3Tdg + 2H)
 *
 * @return ZXGraph
 */
ZXGraph CCXGate::getZXform() {
    ZXGraph g{_id};
    size_t ctrl_qubit_2 = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t ctrl_qubit_1 = _qubits[0]._isTarget ? _qubits[2]._qubit : (_qubits[1]._isTarget ? _qubits[2]._qubit : _qubits[1]._qubit);
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : (_qubits[1]._isTarget ? _qubits[1]._qubit : _qubits[2]._qubit);
    vector<pair<pair<VertexType, Phase>, size_t>> vertices_info;
    vector<pair<pair<size_t, size_t>, EdgeType>> adj_pair;
    vector<int> vertices_col;
    vector<ZXVertex*> vertices_list = {};
    ZXVertex* in_ctrl_1;
    ZXVertex* in_ctrl_2;
    ZXVertex* in_targ;
    ZXVertex* out_ctrl_1;
    ZXVertex* out_ctrl_2;
    ZXVertex* out_targ;
    if (dmode == 1) {
        vertices_info = {{{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(-1, 4)}, ctrl_qubit_2}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}};
        adj_pair = {{{0, 1}, EdgeType::HADAMARD}, {{1, 10}, EdgeType::HADAMARD}, {{1, 2}, EdgeType::HADAMARD}, {{2, 3}, EdgeType::HADAMARD}, {{3, 16}, EdgeType::HADAMARD}, {{3, 4}, EdgeType::HADAMARD}, {{4, 5}, EdgeType::HADAMARD}, {{5, 11}, EdgeType::HADAMARD}, {{5, 6}, EdgeType::HADAMARD}, {{6, 7}, EdgeType::HADAMARD}, {{7, 17}, EdgeType::HADAMARD}, {{7, 8}, EdgeType::HADAMARD}, {{8, 9}, EdgeType::HADAMARD}, {{10, 11}, EdgeType::SIMPLE}, {{11, 12}, EdgeType::SIMPLE}, {{12, 13}, EdgeType::HADAMARD}, {{13, 18}, EdgeType::HADAMARD}, {{13, 14}, EdgeType::HADAMARD}, {{14, 15}, EdgeType::HADAMARD}, {{15, 20}, EdgeType::HADAMARD}, {{16, 17}, EdgeType::SIMPLE}, {{17, 18}, EdgeType::SIMPLE}, {{18, 19}, EdgeType::SIMPLE}, {{19, 20}, EdgeType::SIMPLE}};

        vertices_col = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 2, 6, 7, 9, 10, 11, 4, 8, 9, 10, 11};

        in_ctrl_1 = g.addInput(ctrl_qubit_1, 0);
        in_ctrl_2 = g.addInput(ctrl_qubit_2, 0);
        in_targ = g.addInput(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.addVertex(vertices_info[i].second, vertices_info[i].first.first, vertices_info[i].first.second));
        };
        for (size_t i = 0; i < vertices_col.size(); i++) {
            vertices_list[i]->setCol(vertices_col[i]);
        }
        out_ctrl_1 = g.addOutput(ctrl_qubit_1, 12);
        out_ctrl_2 = g.addOutput(ctrl_qubit_2, 12);
        out_targ = g.addOutput(targ_qubit, 12);
        g.addEdge(in_ctrl_1, vertices_list[16], EdgeType::SIMPLE);
        g.addEdge(in_ctrl_2, vertices_list[10], EdgeType::SIMPLE);
        g.addEdge(in_targ, vertices_list[0], EdgeType::HADAMARD);
        g.addEdge(out_ctrl_1, vertices_list[20], EdgeType::SIMPLE);
        g.addEdge(out_ctrl_2, vertices_list[15], EdgeType::HADAMARD);
        g.addEdge(out_targ, vertices_list[9], EdgeType::SIMPLE);
    } else if (dmode == 2) {
        vertices_info = {{{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(-1, 4)}, ctrl_qubit_2}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_1}};
        adj_pair = {{{0, 1}, EdgeType::HADAMARD}, {{0, 8}, EdgeType::HADAMARD}, {{1, 2}, EdgeType::HADAMARD}, {{2, 12}, EdgeType::HADAMARD}, {{2, 3}, EdgeType::HADAMARD}, {{3, 4}, EdgeType::HADAMARD}, {{4, 8}, EdgeType::HADAMARD}, {{4, 5}, EdgeType::HADAMARD}, {{5, 6}, EdgeType::HADAMARD}, {{6, 12}, EdgeType::HADAMARD}, {{6, 7}, EdgeType::HADAMARD}, {{8, 9}, EdgeType::HADAMARD}, {{9, 12}, EdgeType::HADAMARD}, {{9, 10}, EdgeType::HADAMARD}, {{10, 11}, EdgeType::HADAMARD}, {{11, 12}, EdgeType::HADAMARD}};

        vertices_col = {2, 3, 4, 5, 6, 7, 8, 9, 2, 9, 10, 11, 4};

        in_ctrl_1 = g.addInput(ctrl_qubit_1, 0);
        in_ctrl_2 = g.addInput(ctrl_qubit_2, 0);
        in_targ = g.addInput(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.addVertex(vertices_info[i].second, vertices_info[i].first.first, vertices_info[i].first.second));
        };
        for (size_t i = 0; i < vertices_col.size(); i++) {
            vertices_list[i]->setCol(vertices_col[i]);
        }
        out_ctrl_1 = g.addOutput(ctrl_qubit_1, 12);
        out_ctrl_2 = g.addOutput(ctrl_qubit_2, 12);
        out_targ = g.addOutput(targ_qubit, 12);
        g.addEdge(in_ctrl_1, vertices_list[12], EdgeType::SIMPLE);
        g.addEdge(in_ctrl_2, vertices_list[8], EdgeType::SIMPLE);
        g.addEdge(in_targ, vertices_list[0], EdgeType::SIMPLE);
        g.addEdge(out_ctrl_1, vertices_list[12], EdgeType::SIMPLE);
        g.addEdge(out_ctrl_2, vertices_list[11], EdgeType::HADAMARD);
        g.addEdge(out_targ, vertices_list[7], EdgeType::HADAMARD);
    } else if (dmode == 3) {
        vertices_info = {{{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::Z, Phase(1, 4)}, -2}, {{VertexType::Z, Phase(0)}, -1}, {{VertexType::Z, Phase(-1, 4)}, -2}, {{VertexType::Z, Phase(0)}, -1}, {{VertexType::Z, Phase(-1, 4)}, -2}, {{VertexType::Z, Phase(0)}, -1}, {{VertexType::Z, Phase(-1, 4)}, -2}, {{VertexType::Z, Phase(0)}, -1}};
        adj_pair = {{{0, 4}, EdgeType::HADAMARD}, {{0, 6}, EdgeType::HADAMARD}, {{0, 8}, EdgeType::HADAMARD}, {{1, 4}, EdgeType::HADAMARD}, {{1, 6}, EdgeType::HADAMARD}, {{1, 10}, EdgeType::HADAMARD}, {{2, 4}, EdgeType::HADAMARD}, {{2, 8}, EdgeType::HADAMARD}, {{2, 10}, EdgeType::HADAMARD}, {{3, 4}, EdgeType::HADAMARD}, {{5, 6}, EdgeType::HADAMARD}, {{7, 8}, EdgeType::HADAMARD}, {{9, 10}, EdgeType::HADAMARD}};
        vertices_col = {5, 5, 5, 1, 1, 2, 2, 3, 3, 4, 4};

        in_ctrl_1 = g.addInput(ctrl_qubit_1, 0);
        in_ctrl_2 = g.addInput(ctrl_qubit_2, 0);
        in_targ = g.addInput(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.addVertex(vertices_info[i].second, vertices_info[i].first.first, vertices_info[i].first.second));
        };
        for (size_t i = 0; i < vertices_col.size(); i++) {
            vertices_list[i]->setCol(vertices_col[i]);
        }
        out_ctrl_1 = g.addOutput(ctrl_qubit_1, 6);
        out_ctrl_2 = g.addOutput(ctrl_qubit_2, 6);
        out_targ = g.addOutput(targ_qubit, 6);
        g.addEdge(in_ctrl_1, vertices_list[2], EdgeType::SIMPLE);
        g.addEdge(in_ctrl_2, vertices_list[1], EdgeType::SIMPLE);
        g.addEdge(in_targ, vertices_list[0], EdgeType::HADAMARD);
        g.addEdge(out_ctrl_1, vertices_list[2], EdgeType::SIMPLE);
        g.addEdge(out_ctrl_2, vertices_list[1], EdgeType::SIMPLE);
        g.addEdge(out_targ, vertices_list[0], EdgeType::HADAMARD);
    } else {
        vertices_info = {{{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(1, 4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::X, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(-1, 4)}, ctrl_qubit_2}, {{VertexType::X, Phase(0)}, ctrl_qubit_2}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::Z, Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}};
        adj_pair = {{{0, 1}, EdgeType::SIMPLE}, {{1, 10}, EdgeType::SIMPLE}, {{1, 2}, EdgeType::SIMPLE}, {{2, 3}, EdgeType::SIMPLE}, {{3, 16}, EdgeType::SIMPLE}, {{3, 4}, EdgeType::SIMPLE}, {{4, 5}, EdgeType::SIMPLE}, {{5, 11}, EdgeType::SIMPLE}, {{5, 6}, EdgeType::SIMPLE}, {{6, 7}, EdgeType::SIMPLE}, {{7, 17}, EdgeType::SIMPLE}, {{7, 8}, EdgeType::SIMPLE}, {{8, 9}, EdgeType::HADAMARD}, {{10, 11}, EdgeType::SIMPLE}, {{11, 12}, EdgeType::SIMPLE}, {{12, 13}, EdgeType::SIMPLE}, {{13, 18}, EdgeType::SIMPLE}, {{13, 14}, EdgeType::SIMPLE}, {{14, 15}, EdgeType::SIMPLE}, {{15, 20}, EdgeType::SIMPLE}, {{16, 17}, EdgeType::SIMPLE}, {{17, 18}, EdgeType::SIMPLE}, {{18, 19}, EdgeType::SIMPLE}, {{19, 20}, EdgeType::SIMPLE}};

        vertices_col = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 2, 6, 7, 9, 10, 11, 4, 8, 9, 10, 11};

        in_ctrl_1 = g.addInput(ctrl_qubit_1, 0);
        in_ctrl_2 = g.addInput(ctrl_qubit_2, 0);
        in_targ = g.addInput(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.addVertex(vertices_info[i].second, vertices_info[i].first.first, vertices_info[i].first.second));
        };
        for (size_t i = 0; i < vertices_col.size(); i++) {
            vertices_list[i]->setCol(vertices_col[i]);
        }
        out_ctrl_1 = g.addOutput(ctrl_qubit_1, 12);
        out_ctrl_2 = g.addOutput(ctrl_qubit_2, 12);
        out_targ = g.addOutput(targ_qubit, 12);
        g.addEdge(in_ctrl_1, vertices_list[16], EdgeType::SIMPLE);
        g.addEdge(in_ctrl_2, vertices_list[10], EdgeType::SIMPLE);
        g.addEdge(in_targ, vertices_list[0], EdgeType::HADAMARD);
        g.addEdge(out_ctrl_1, vertices_list[20], EdgeType::SIMPLE);
        g.addEdge(out_ctrl_2, vertices_list[15], EdgeType::SIMPLE);
        g.addEdge(out_targ, vertices_list[9], EdgeType::SIMPLE);
    }

    for (size_t i = 0; i < adj_pair.size(); i++) {
        g.addEdge(vertices_list[adj_pair[i].first.first], vertices_list[adj_pair[i].first.second], adj_pair[i].second);
    }
    return g;
}

// TODO - SWAP ZXForm
ZXGraph SWAPGate::getZXform() {
    ZXGraph g;
    auto i0 = g.addInput(0, 0);
    auto o0 = g.addOutput(0, 1);
    auto i1 = g.addInput(1, 0);
    auto o1 = g.addOutput(1, 1);
    g.addEdge(i0, o1, EdgeType::SIMPLE);
    g.addEdge(i1, o0, EdgeType::SIMPLE);

    return g;
}

/**
 * @brief Get ZXGraph of CZ
 *
 * @return ZXGraph
 */
ZXGraph CZGate::getZXform() {
    ZXGraph g{_id};
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;

    ZXVertex* in_ctrl = g.addInput(ctrl_qubit);
    ZXVertex* in_targ = g.addInput(targ_qubit);
    ZXVertex* ctrl = g.addVertex(ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex* targZ = g.addVertex(targ_qubit, VertexType::Z, Phase(0));
    ZXVertex* out_ctrl = g.addOutput(ctrl_qubit);
    ZXVertex* out_targ = g.addOutput(targ_qubit);
    g.addEdge(in_ctrl, ctrl, EdgeType::SIMPLE);
    g.addEdge(ctrl, out_ctrl, EdgeType::SIMPLE);
    g.addEdge(in_targ, targZ, EdgeType::SIMPLE);
    g.addEdge(targZ, out_targ, EdgeType::SIMPLE);
    g.addEdge(ctrl, targZ, EdgeType::HADAMARD);

    return g;
}

// Y Gate
// NOTE - Cannot use mapSingleQubitGate

/**
 * @brief Get ZXGraph of Y = iXZ
 *
 * @return ZXGraph
 */
ZXGraph YGate::getZXform() {
    ZXGraph g{_id};
    size_t qubit = _qubits[0]._qubit;

    ZXVertex* in = g.addInput(qubit);
    ZXVertex* X = g.addVertex(qubit, VertexType::X, Phase(1));
    ZXVertex* Z = g.addVertex(qubit, VertexType::Z, Phase(1));
    ZXVertex* out = g.addOutput(qubit);
    g.addEdge(in, X, EdgeType::SIMPLE);
    g.addEdge(X, Z, EdgeType::SIMPLE);
    g.addEdge(Z, out, EdgeType::SIMPLE);

    return g;
}

/**
 * @brief Get ZXGraph of SY = S。SX。Sdg
 *
 * @return ZXGraph
 */
ZXGraph SYGate::getZXform() {
    ZXGraph g{_id};
    size_t qubit = _qubits[0]._qubit;

    ZXVertex* in = g.addInput(qubit);
    ZXVertex* S = g.addVertex(qubit, VertexType::Z, Phase(1, 2));
    ZXVertex* SX = g.addVertex(qubit, VertexType::X, Phase(1, 2));
    ZXVertex* Sdg = g.addVertex(qubit, VertexType::Z, Phase(-1, 2));
    ZXVertex* out = g.addOutput(qubit);
    g.addEdge(in, S, EdgeType::SIMPLE);
    g.addEdge(S, SX, EdgeType::SIMPLE);
    g.addEdge(SX, Sdg, EdgeType::SIMPLE);
    g.addEdge(Sdg, out, EdgeType::SIMPLE);

    return g;
}

/**
 * @brief Get ZXGraph of MCPX
 *
 * @return ZXGraph
 */
ZXGraph MCPXGate::getZXform() {
    return detail::MCP_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::X);
}

/**
 * @brief Get ZXGraph of MCPY
 *
 * @return ZXGraph
 */
ZXGraph MCPYGate::getZXform() {
    return detail::MCP_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::Y);
}

/**
 * @brief Get ZXGraph of MCP
 *
 * @return ZXGraph
 */
ZXGraph MCPGate::getZXform() {
    return detail::MCP_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::Z);
}

/**
 * @brief Get ZXGraph of MCRX
 *
 * @return ZXGraph
 */
ZXGraph MCRXGate::getZXform() {
    return detail::MCR_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::X);
}

/**
 * @brief Get ZXGraphof MCRY
 *
 * @return ZXGraph
 */
ZXGraph MCRYGate::getZXform() {
    return detail::MCR_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::Y);
}

/**
 * @brief Get ZXGraphof MCRZ
 *
 * @return ZXGraph
 */
ZXGraph MCRZGate::getZXform() {
    return detail::MCR_Gen(_qubits, _id, _rotatePhase, detail::RotationAxis::Z);
}
