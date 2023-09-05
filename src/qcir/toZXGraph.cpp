/****************************************************************************
  FileName     [ qcirGate2ZX.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./toZXGraph.hpp"

#include <cstddef>

#include "./qcirGate.hpp"
#include "util/logger.hpp"
#include "util/phase.hpp"
#include "util/rational.hpp"

using namespace std;

extern dvlab::utils::Logger logger;
extern bool stop_requested();

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
    ZXVertex* target = nullptr;
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

ZXGraph MCR_Gen(vector<BitInfo> qubits, Phase const& rotatePhase, RotationAxis ax) {
    ZXGraph g;
    Phase phase = detail::getGadgetPhase(rotatePhase, qubits.size());

    auto [controls, target] = detail::MC_GenBackbone(g, qubits, ax);

    detail::MCR_GenGadgets(g, controls, target, phase);

    return g;
}

ZXGraph MCP_Gen(vector<BitInfo> qubits, Phase const& rotatePhase, RotationAxis ax) {
    ZXGraph g;
    Phase phase = detail::getGadgetPhase(rotatePhase, qubits.size());

    auto [vertices, target] = detail::MC_GenBackbone(g, qubits, ax);
    vertices.emplace_back(target);

    detail::MCP_GenGadgets(g, vertices, phase);

    return g;
}

/**
 * @brief Map single qubit gate to ZXGraph
 *
 * @param vt
 * @param ph
 * @return ZXGraph
 */
ZXGraph mapSingleQubitGate(QCirGate* gate, VertexType vt, Phase ph) {
    ZXGraph g;
    size_t qubit = gate->getQubits()[0]._qubit;

    ZXVertex* in = g.addInput(qubit);
    ZXVertex* v = g.addVertex(qubit, vt, ph);
    ZXVertex* out = g.addOutput(qubit);
    g.addEdge(in, v, EdgeType::SIMPLE);
    g.addEdge(v, out, EdgeType::SIMPLE);

    return g;
}

// Double or More Qubit Gate

/**
 * @brief get ZXGraph of CX
 *
 * @return ZXGraph
 */
ZXGraph getCXZXform(QCirGate* gate) {
    ZXGraph g;
    size_t ctrl_qubit = gate->getQubits()[0]._isTarget ? gate->getQubits()[1]._qubit : gate->getQubits()[0]._qubit;
    size_t targ_qubit = gate->getQubits()[0]._isTarget ? gate->getQubits()[0]._qubit : gate->getQubits()[1]._qubit;

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
ZXGraph getCCXZXform(QCirGate* gate, size_t decomposition_mode) {
    ZXGraph g;
    size_t ctrl_qubit_2 = gate->getQubits()[0]._isTarget ? gate->getQubits()[1]._qubit : gate->getQubits()[0]._qubit;
    size_t ctrl_qubit_1 = gate->getQubits()[0]._isTarget ? gate->getQubits()[2]._qubit : (gate->getQubits()[1]._isTarget ? gate->getQubits()[2]._qubit : gate->getQubits()[1]._qubit);
    size_t targ_qubit = gate->getQubits()[0]._isTarget ? gate->getQubits()[0]._qubit : (gate->getQubits()[1]._isTarget ? gate->getQubits()[1]._qubit : gate->getQubits()[2]._qubit);
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
    if (decomposition_mode == 1) {
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
    } else if (decomposition_mode == 2) {
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
    } else if (decomposition_mode == 3) {
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

ZXGraph getSwapZXform(QCirGate* gate) {
    ZXGraph g;
    size_t qb0 = gate->getQubits()[0]._qubit;
    size_t qb1 = gate->getQubits()[1]._qubit;
    auto i0 = g.addInput(qb0, 0);
    auto o0 = g.addOutput(qb0, 1);
    auto i1 = g.addInput(qb1, 0);
    auto o1 = g.addOutput(qb1, 1);
    g.addEdge(i0, o1, EdgeType::SIMPLE);
    g.addEdge(i1, o0, EdgeType::SIMPLE);

    return g;
}

/**
 * @brief Get ZXGraph of CZ
 *
 * @return ZXGraph
 */
ZXGraph getCZZXform(QCirGate* gate) {
    ZXGraph g;
    size_t ctrl_qubit = gate->getQubits()[0]._isTarget ? gate->getQubits()[1]._qubit : gate->getQubits()[0]._qubit;
    size_t targ_qubit = gate->getQubits()[0]._isTarget ? gate->getQubits()[0]._qubit : gate->getQubits()[1]._qubit;

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
ZXGraph getYZXform(QCirGate* gate) {
    ZXGraph g;
    size_t qubit = gate->getQubits()[0]._qubit;

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
ZXGraph getRYZXform(QCirGate* gate, Phase ph) {
    ZXGraph g;
    size_t qubit = gate->getQubits()[0]._qubit;

    ZXVertex* in = g.addInput(qubit);
    ZXVertex* S = g.addVertex(qubit, VertexType::Z, Phase(1, 2));
    ZXVertex* SX = g.addVertex(qubit, VertexType::X, ph);
    ZXVertex* Sdg = g.addVertex(qubit, VertexType::Z, Phase(-1, 2));
    ZXVertex* out = g.addOutput(qubit);
    g.addEdge(in, S, EdgeType::SIMPLE);
    g.addEdge(S, SX, EdgeType::SIMPLE);
    g.addEdge(SX, Sdg, EdgeType::SIMPLE);
    g.addEdge(Sdg, out, EdgeType::SIMPLE);

    return g;
}

}  // namespace detail

std::optional<ZXGraph> toZXGraph(QCirGate* gate, size_t decomposition_mode) {
    switch (gate->getType()) {
        // single-qubit gates
        case GateType::H:
            return detail::mapSingleQubitGate(gate, VertexType::H_BOX, Phase(1));
        case GateType::Z:
            return detail::mapSingleQubitGate(gate, VertexType::Z, Phase(1));
        case GateType::P:
        case GateType::RZ:
            return detail::mapSingleQubitGate(gate, VertexType::Z, gate->getPhase());
        case GateType::S:
            return detail::mapSingleQubitGate(gate, VertexType::Z, Phase(1, 2));
        case GateType::T:
            return detail::mapSingleQubitGate(gate, VertexType::Z, Phase(1, 4));
        case GateType::SDG:
            return detail::mapSingleQubitGate(gate, VertexType::Z, Phase(-1, 2));
        case GateType::TDG:
            return detail::mapSingleQubitGate(gate, VertexType::Z, Phase(-1, 4));
        case GateType::X:
            return detail::mapSingleQubitGate(gate, VertexType::X, Phase(1));
        case GateType::PX:
        case GateType::RX:
            return detail::mapSingleQubitGate(gate, VertexType::X, gate->getPhase());
        case GateType::SX:
            return detail::mapSingleQubitGate(gate, VertexType::X, Phase(1, 2));
        case GateType::Y:
            return detail::getYZXform(gate);
        case GateType::PY:
        case GateType::RY:
            return detail::getRYZXform(gate, gate->getPhase());
        case GateType::SY:
            return detail::getRYZXform(gate, Phase(1, 2));
            // double-qubit gates

        case GateType::CX:
            return detail::getCXZXform(gate);
        case GateType::CZ:
            return detail::getCZZXform(gate);
        case GateType::SWAP:
            return detail::getSwapZXform(gate);

        // multi-qubit gates
        case GateType::MCRZ:
            return detail::MCR_Gen(gate->getQubits(), gate->getPhase(), detail::RotationAxis::Z);
        case GateType::MCP:
        case GateType::CCZ:
            return detail::MCP_Gen(gate->getQubits(), gate->getPhase(), detail::RotationAxis::Z);
        case GateType::CCX:
            return detail::getCCXZXform(gate, decomposition_mode);
        case GateType::MCRX:
            return detail::MCR_Gen(gate->getQubits(), gate->getPhase(), detail::RotationAxis::X);
        case GateType::MCPX:
            return detail::MCP_Gen(gate->getQubits(), gate->getPhase(), detail::RotationAxis::X);
        case GateType::MCRY:
            return detail::MCR_Gen(gate->getQubits(), gate->getPhase(), detail::RotationAxis::Y);
        case GateType::MCPY:
            return detail::MCP_Gen(gate->getQubits(), gate->getPhase(), detail::RotationAxis::Y);

        default:
            return std::nullopt;
    }
};

/**
 * @brief Mapping QCir to ZXGraph
 */
std::optional<ZXGraph> toZXGraph(QCir const& qcir, size_t decomposition_mode) {
    qcir.updateGateTime();
    ZXGraph g;
    logger.debug("Add boundaries");
    for (size_t i = 0; i < qcir.getQubits().size(); i++) {
        ZXVertex* input = g.addInput(qcir.getQubits()[i]->getId());
        ZXVertex* output = g.addOutput(qcir.getQubits()[i]->getId());
        input->setCol(0);
        g.addEdge(input, output, EdgeType::SIMPLE);
    }

    qcir.topoTraverse([&g, &decomposition_mode](QCirGate* gate) {
        if (stop_requested()) return;
        logger.debug("Gate {} ({})", gate->getId(), gate->getTypeStr());

        auto tmp = toZXGraph(gate, decomposition_mode);
        assert(tmp.has_value());

        for (auto& v : tmp->getVertices()) {
            v->setCol(v->getCol() + gate->getTime() + gate->getDelay());
        }

        g.concatenate(*tmp);
    });

    size_t max = 0;
    for (auto& v : g.getOutputs()) {
        size_t neighborCol = v->getFirstNeighbor().first->getCol();
        if (neighborCol > max) {
            max = neighborCol;
        }
    }
    for (auto& v : g.getOutputs()) {
        v->setCol(max + 1);
    }

    if (stop_requested()) {
        logger.warning("Conversion interrupted.");
        return std::nullopt;
    }

    return g;
}
