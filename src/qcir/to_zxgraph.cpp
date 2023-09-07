/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define conversion from QCir to ZXGraph ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./to_zxgraph.hpp"

#include <cstddef>

#include "./qcir_gate.hpp"
#include "util/logger.hpp"
#include "util/phase.hpp"
#include "util/rational.hpp"

using namespace std;

extern dvlab::Logger LOGGER;
extern bool stop_requested();

namespace detail {

Phase get_gadget_phase(Phase const& phase, size_t n_qubits) {
    return phase * Rational(1, pow(2, n_qubits - 1));
}

enum class RotationAxis {
    x,
    y,
    z
};

pair<vector<ZXVertex*>, ZXVertex*>
create_multi_control_backbone(ZXGraph& g, vector<QubitInfo> const& qubits, RotationAxis ax) {
    vector<ZXVertex*> controls;
    ZXVertex* target = nullptr;
    for (auto const& bitinfo : qubits) {
        size_t qubit = bitinfo._qubit;
        ZXVertex* in = g.add_input(qubit);
        ZXVertex* v = g.add_vertex(qubit, VertexType::z);
        ZXVertex* out = g.add_output(qubit);
        if (ax == RotationAxis::z || !bitinfo._isTarget) {
            g.add_edge(in, v, EdgeType::simple);
            g.add_edge(v, out, EdgeType::simple);
        } else {
            g.add_edge(in, v, EdgeType::hadamard);
            g.add_edge(v, out, EdgeType::hadamard);
            if (ax == RotationAxis::y) {
                g.add_buffer(in, v, EdgeType::hadamard)->set_phase(Phase(1, 2));
                g.add_buffer(out, v, EdgeType::hadamard)->set_phase(Phase(-1, 2));
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
void make_combinations_helper(vector<vector<ZXVertex*>>& comb, vector<ZXVertex*>& tmp, vector<ZXVertex*> const& vertices, int left, int k) {
    if (k == 0) {
        comb.emplace_back(tmp);
        return;
    }
    for (int i = left; i < (int)vertices.size(); ++i) {
        tmp.emplace_back(vertices[i]);
        make_combinations_helper(comb, tmp, vertices, i + 1, k - 1);
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
vector<vector<ZXVertex*>> make_combinations(vector<ZXVertex*> const& vertices, int k) {
    vector<vector<ZXVertex*>> comb;
    vector<ZXVertex*> tmp;
    make_combinations_helper(comb, tmp, vertices, 0, k);
    return comb;
}

void create_multi_control_r_gate_gadgets(ZXGraph& g, vector<ZXVertex*> const& controls, ZXVertex* target, Phase const& phase) {
    target->set_phase(phase);
    for (size_t k = 1; k <= controls.size(); k++) {
        vector<vector<ZXVertex*>> combinations = make_combinations(controls, k);
        for (auto& combination : combinations) {
            combination.emplace_back(target);
            g.add_gadget((combination.size() % 2) ? phase : -phase, combination);
        }
    }
}

void create_multi_control_p_gate_gadgets(ZXGraph& g, vector<ZXVertex*> const& vertices, Phase const& phase) {
    for (auto& v : vertices) {
        v->set_phase(phase);
    }
    for (size_t k = 2; k <= vertices.size(); k++) {
        vector<vector<ZXVertex*>> combinations = make_combinations(vertices, k);
        for (auto& combination : combinations) {
            g.add_gadget((combination.size() % 2) ? phase : -phase, combination);
        }
    }
}

ZXGraph create_mcr_zx_form(vector<QubitInfo> qubits, Phase const& phase, RotationAxis ax) {
    ZXGraph g;
    Phase gadget_phase = detail::get_gadget_phase(phase, qubits.size());

    auto [controls, target] = detail::create_multi_control_backbone(g, qubits, ax);

    detail::create_multi_control_r_gate_gadgets(g, controls, target, gadget_phase);

    return g;
}

ZXGraph create_mcp_zx_form(vector<QubitInfo> qubits, Phase const& phase, RotationAxis ax) {
    ZXGraph g;
    Phase gadget_phase = detail::get_gadget_phase(phase, qubits.size());

    auto [vertices, target] = detail::create_multi_control_backbone(g, qubits, ax);
    vertices.emplace_back(target);

    detail::create_multi_control_p_gate_gadgets(g, vertices, gadget_phase);

    return g;
}

/**
 * @brief Map single qubit gate to ZXGraph
 *
 * @param vt
 * @param ph
 * @return ZXGraph
 */
ZXGraph create_single_vertex_zx_form(QCirGate* gate, VertexType vt, Phase ph) {
    ZXGraph g;
    size_t qubit = gate->get_qubits()[0]._qubit;

    ZXVertex* in = g.add_input(qubit);
    ZXVertex* v = g.add_vertex(qubit, vt, ph);
    ZXVertex* out = g.add_output(qubit);
    g.add_edge(in, v, EdgeType::simple);
    g.add_edge(v, out, EdgeType::simple);

    return g;
}

// Double or More Qubit Gate

/**
 * @brief get ZXGraph of CX
 *
 * @return ZXGraph
 */
ZXGraph create_cx_zx_form(QCirGate* gate) {
    ZXGraph g;
    size_t ctrl_qubit = gate->get_qubits()[0]._isTarget ? gate->get_qubits()[1]._qubit : gate->get_qubits()[0]._qubit;
    size_t targ_qubit = gate->get_qubits()[0]._isTarget ? gate->get_qubits()[0]._qubit : gate->get_qubits()[1]._qubit;

    ZXVertex* in_ctrl = g.add_input(ctrl_qubit);
    ZXVertex* in_targ = g.add_input(targ_qubit);
    ZXVertex* ctrl = g.add_vertex(ctrl_qubit, VertexType::z, Phase(0));
    ZXVertex* targ_x = g.add_vertex(targ_qubit, VertexType::x, Phase(0));
    ZXVertex* out_ctrl = g.add_output(ctrl_qubit);
    ZXVertex* out_targ = g.add_output(targ_qubit);
    g.add_edge(in_ctrl, ctrl, EdgeType::simple);
    g.add_edge(ctrl, out_ctrl, EdgeType::simple);
    g.add_edge(in_targ, targ_x, EdgeType::simple);
    g.add_edge(targ_x, out_targ, EdgeType::simple);
    g.add_edge(ctrl, targ_x, EdgeType::simple);

    return g;
}

/**
 * @brief Cet ZXGraph of CCX.
 *        Decomposed into 21 vertices (6X + 6Z + 4T + 3Tdg + 2H)
 *
 * @return ZXGraph
 */
ZXGraph create_ccx_zx_form(QCirGate* gate, size_t decomposition_mode) {
    ZXGraph g;
    size_t ctrl_qubit_2 = gate->get_qubits()[0]._isTarget ? gate->get_qubits()[1]._qubit : gate->get_qubits()[0]._qubit;
    size_t ctrl_qubit_1 = gate->get_qubits()[0]._isTarget ? gate->get_qubits()[2]._qubit : (gate->get_qubits()[1]._isTarget ? gate->get_qubits()[2]._qubit : gate->get_qubits()[1]._qubit);
    size_t targ_qubit = gate->get_qubits()[0]._isTarget ? gate->get_qubits()[0]._qubit : (gate->get_qubits()[1]._isTarget ? gate->get_qubits()[1]._qubit : gate->get_qubits()[2]._qubit);
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
        vertices_info = {{{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(-1, 4)}, targ_qubit}, {{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(1, 4)}, targ_qubit}, {{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(-1, 4)}, targ_qubit}, {{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(1, 4)}, targ_qubit}, {{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::z, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(-1, 4)}, ctrl_qubit_2}, {{VertexType::z, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(0)}, ctrl_qubit_1}, {{VertexType::z, Phase(0)}, ctrl_qubit_1}, {{VertexType::z, Phase(0)}, ctrl_qubit_1}, {{VertexType::z, Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::z, Phase(0)}, ctrl_qubit_1}};
        adj_pair = {{{0, 1}, EdgeType::hadamard}, {{1, 10}, EdgeType::hadamard}, {{1, 2}, EdgeType::hadamard}, {{2, 3}, EdgeType::hadamard}, {{3, 16}, EdgeType::hadamard}, {{3, 4}, EdgeType::hadamard}, {{4, 5}, EdgeType::hadamard}, {{5, 11}, EdgeType::hadamard}, {{5, 6}, EdgeType::hadamard}, {{6, 7}, EdgeType::hadamard}, {{7, 17}, EdgeType::hadamard}, {{7, 8}, EdgeType::hadamard}, {{8, 9}, EdgeType::hadamard}, {{10, 11}, EdgeType::simple}, {{11, 12}, EdgeType::simple}, {{12, 13}, EdgeType::hadamard}, {{13, 18}, EdgeType::hadamard}, {{13, 14}, EdgeType::hadamard}, {{14, 15}, EdgeType::hadamard}, {{15, 20}, EdgeType::hadamard}, {{16, 17}, EdgeType::simple}, {{17, 18}, EdgeType::simple}, {{18, 19}, EdgeType::simple}, {{19, 20}, EdgeType::simple}};

        vertices_col = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 2, 6, 7, 9, 10, 11, 4, 8, 9, 10, 11};

        in_ctrl_1 = g.add_input(ctrl_qubit_1, 0);
        in_ctrl_2 = g.add_input(ctrl_qubit_2, 0);
        in_targ = g.add_input(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.add_vertex(vertices_info[i].second, vertices_info[i].first.first, vertices_info[i].first.second));
        };
        for (size_t i = 0; i < vertices_col.size(); i++) {
            vertices_list[i]->set_col(vertices_col[i]);
        }
        out_ctrl_1 = g.add_output(ctrl_qubit_1, 12);
        out_ctrl_2 = g.add_output(ctrl_qubit_2, 12);
        out_targ = g.add_output(targ_qubit, 12);
        g.add_edge(in_ctrl_1, vertices_list[16], EdgeType::simple);
        g.add_edge(in_ctrl_2, vertices_list[10], EdgeType::simple);
        g.add_edge(in_targ, vertices_list[0], EdgeType::hadamard);
        g.add_edge(out_ctrl_1, vertices_list[20], EdgeType::simple);
        g.add_edge(out_ctrl_2, vertices_list[15], EdgeType::hadamard);
        g.add_edge(out_targ, vertices_list[9], EdgeType::simple);
    } else if (decomposition_mode == 2) {
        vertices_info = {{{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(-1, 4)}, targ_qubit}, {{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(1, 4)}, targ_qubit}, {{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(-1, 4)}, targ_qubit}, {{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(1, 4)}, targ_qubit}, {{VertexType::z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::z, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(-1, 4)}, ctrl_qubit_2}, {{VertexType::z, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(1, 4)}, ctrl_qubit_1}};
        adj_pair = {{{0, 1}, EdgeType::hadamard}, {{0, 8}, EdgeType::hadamard}, {{1, 2}, EdgeType::hadamard}, {{2, 12}, EdgeType::hadamard}, {{2, 3}, EdgeType::hadamard}, {{3, 4}, EdgeType::hadamard}, {{4, 8}, EdgeType::hadamard}, {{4, 5}, EdgeType::hadamard}, {{5, 6}, EdgeType::hadamard}, {{6, 12}, EdgeType::hadamard}, {{6, 7}, EdgeType::hadamard}, {{8, 9}, EdgeType::hadamard}, {{9, 12}, EdgeType::hadamard}, {{9, 10}, EdgeType::hadamard}, {{10, 11}, EdgeType::hadamard}, {{11, 12}, EdgeType::hadamard}};

        vertices_col = {2, 3, 4, 5, 6, 7, 8, 9, 2, 9, 10, 11, 4};

        in_ctrl_1 = g.add_input(ctrl_qubit_1, 0);
        in_ctrl_2 = g.add_input(ctrl_qubit_2, 0);
        in_targ = g.add_input(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.add_vertex(vertices_info[i].second, vertices_info[i].first.first, vertices_info[i].first.second));
        };
        for (size_t i = 0; i < vertices_col.size(); i++) {
            vertices_list[i]->set_col(vertices_col[i]);
        }
        out_ctrl_1 = g.add_output(ctrl_qubit_1, 12);
        out_ctrl_2 = g.add_output(ctrl_qubit_2, 12);
        out_targ = g.add_output(targ_qubit, 12);
        g.add_edge(in_ctrl_1, vertices_list[12], EdgeType::simple);
        g.add_edge(in_ctrl_2, vertices_list[8], EdgeType::simple);
        g.add_edge(in_targ, vertices_list[0], EdgeType::simple);
        g.add_edge(out_ctrl_1, vertices_list[12], EdgeType::simple);
        g.add_edge(out_ctrl_2, vertices_list[11], EdgeType::hadamard);
        g.add_edge(out_targ, vertices_list[7], EdgeType::hadamard);
    } else if (decomposition_mode == 3) {
        vertices_info = {{{VertexType::z, Phase(1, 4)}, targ_qubit}, {{VertexType::z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::z, Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::z, Phase(1, 4)}, -2}, {{VertexType::z, Phase(0)}, -1}, {{VertexType::z, Phase(-1, 4)}, -2}, {{VertexType::z, Phase(0)}, -1}, {{VertexType::z, Phase(-1, 4)}, -2}, {{VertexType::z, Phase(0)}, -1}, {{VertexType::z, Phase(-1, 4)}, -2}, {{VertexType::z, Phase(0)}, -1}};
        adj_pair = {{{0, 4}, EdgeType::hadamard}, {{0, 6}, EdgeType::hadamard}, {{0, 8}, EdgeType::hadamard}, {{1, 4}, EdgeType::hadamard}, {{1, 6}, EdgeType::hadamard}, {{1, 10}, EdgeType::hadamard}, {{2, 4}, EdgeType::hadamard}, {{2, 8}, EdgeType::hadamard}, {{2, 10}, EdgeType::hadamard}, {{3, 4}, EdgeType::hadamard}, {{5, 6}, EdgeType::hadamard}, {{7, 8}, EdgeType::hadamard}, {{9, 10}, EdgeType::hadamard}};
        vertices_col = {5, 5, 5, 1, 1, 2, 2, 3, 3, 4, 4};

        in_ctrl_1 = g.add_input(ctrl_qubit_1, 0);
        in_ctrl_2 = g.add_input(ctrl_qubit_2, 0);
        in_targ = g.add_input(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.add_vertex(vertices_info[i].second, vertices_info[i].first.first, vertices_info[i].first.second));
        };
        for (size_t i = 0; i < vertices_col.size(); i++) {
            vertices_list[i]->set_col(vertices_col[i]);
        }
        out_ctrl_1 = g.add_output(ctrl_qubit_1, 6);
        out_ctrl_2 = g.add_output(ctrl_qubit_2, 6);
        out_targ = g.add_output(targ_qubit, 6);
        g.add_edge(in_ctrl_1, vertices_list[2], EdgeType::simple);
        g.add_edge(in_ctrl_2, vertices_list[1], EdgeType::simple);
        g.add_edge(in_targ, vertices_list[0], EdgeType::hadamard);
        g.add_edge(out_ctrl_1, vertices_list[2], EdgeType::simple);
        g.add_edge(out_ctrl_2, vertices_list[1], EdgeType::simple);
        g.add_edge(out_targ, vertices_list[0], EdgeType::hadamard);
    } else {
        vertices_info = {{{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::x, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(-1, 4)}, targ_qubit}, {{VertexType::x, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(1, 4)}, targ_qubit}, {{VertexType::x, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(-1, 4)}, targ_qubit}, {{VertexType::x, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(1, 4)}, targ_qubit}, {{VertexType::z, Phase(0)}, targ_qubit}, {{VertexType::z, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::x, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(-1, 4)}, ctrl_qubit_2}, {{VertexType::x, Phase(0)}, ctrl_qubit_2}, {{VertexType::z, Phase(0)}, ctrl_qubit_1}, {{VertexType::z, Phase(0)}, ctrl_qubit_1}, {{VertexType::z, Phase(0)}, ctrl_qubit_1}, {{VertexType::z, Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::z, Phase(0)}, ctrl_qubit_1}};
        adj_pair = {{{0, 1}, EdgeType::simple}, {{1, 10}, EdgeType::simple}, {{1, 2}, EdgeType::simple}, {{2, 3}, EdgeType::simple}, {{3, 16}, EdgeType::simple}, {{3, 4}, EdgeType::simple}, {{4, 5}, EdgeType::simple}, {{5, 11}, EdgeType::simple}, {{5, 6}, EdgeType::simple}, {{6, 7}, EdgeType::simple}, {{7, 17}, EdgeType::simple}, {{7, 8}, EdgeType::simple}, {{8, 9}, EdgeType::hadamard}, {{10, 11}, EdgeType::simple}, {{11, 12}, EdgeType::simple}, {{12, 13}, EdgeType::simple}, {{13, 18}, EdgeType::simple}, {{13, 14}, EdgeType::simple}, {{14, 15}, EdgeType::simple}, {{15, 20}, EdgeType::simple}, {{16, 17}, EdgeType::simple}, {{17, 18}, EdgeType::simple}, {{18, 19}, EdgeType::simple}, {{19, 20}, EdgeType::simple}};

        vertices_col = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 2, 6, 7, 9, 10, 11, 4, 8, 9, 10, 11};

        in_ctrl_1 = g.add_input(ctrl_qubit_1, 0);
        in_ctrl_2 = g.add_input(ctrl_qubit_2, 0);
        in_targ = g.add_input(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.add_vertex(vertices_info[i].second, vertices_info[i].first.first, vertices_info[i].first.second));
        };
        for (size_t i = 0; i < vertices_col.size(); i++) {
            vertices_list[i]->set_col(vertices_col[i]);
        }
        out_ctrl_1 = g.add_output(ctrl_qubit_1, 12);
        out_ctrl_2 = g.add_output(ctrl_qubit_2, 12);
        out_targ = g.add_output(targ_qubit, 12);
        g.add_edge(in_ctrl_1, vertices_list[16], EdgeType::simple);
        g.add_edge(in_ctrl_2, vertices_list[10], EdgeType::simple);
        g.add_edge(in_targ, vertices_list[0], EdgeType::hadamard);
        g.add_edge(out_ctrl_1, vertices_list[20], EdgeType::simple);
        g.add_edge(out_ctrl_2, vertices_list[15], EdgeType::simple);
        g.add_edge(out_targ, vertices_list[9], EdgeType::simple);
    }

    for (size_t i = 0; i < adj_pair.size(); i++) {
        g.add_edge(vertices_list[adj_pair[i].first.first], vertices_list[adj_pair[i].first.second], adj_pair[i].second);
    }
    return g;
}

ZXGraph create_swap_zx_form(QCirGate* gate) {
    ZXGraph g;
    size_t qb0 = gate->get_qubits()[0]._qubit;
    size_t qb1 = gate->get_qubits()[1]._qubit;
    auto i0 = g.add_input(qb0, 0);
    auto o0 = g.add_output(qb0, 1);
    auto i1 = g.add_input(qb1, 0);
    auto o1 = g.add_output(qb1, 1);
    g.add_edge(i0, o1, EdgeType::simple);
    g.add_edge(i1, o0, EdgeType::simple);

    return g;
}

/**
 * @brief Get ZXGraph of CZ
 *
 * @return ZXGraph
 */
ZXGraph create_cz_zx_form(QCirGate* gate) {
    ZXGraph g;
    size_t ctrl_qubit = gate->get_qubits()[0]._isTarget ? gate->get_qubits()[1]._qubit : gate->get_qubits()[0]._qubit;
    size_t targ_qubit = gate->get_qubits()[0]._isTarget ? gate->get_qubits()[0]._qubit : gate->get_qubits()[1]._qubit;

    ZXVertex* in_ctrl = g.add_input(ctrl_qubit);
    ZXVertex* in_targ = g.add_input(targ_qubit);
    ZXVertex* ctrl = g.add_vertex(ctrl_qubit, VertexType::z, Phase(0));
    ZXVertex* targ_z = g.add_vertex(targ_qubit, VertexType::z, Phase(0));
    ZXVertex* out_ctrl = g.add_output(ctrl_qubit);
    ZXVertex* out_targ = g.add_output(targ_qubit);
    g.add_edge(in_ctrl, ctrl, EdgeType::simple);
    g.add_edge(ctrl, out_ctrl, EdgeType::simple);
    g.add_edge(in_targ, targ_z, EdgeType::simple);
    g.add_edge(targ_z, out_targ, EdgeType::simple);
    g.add_edge(ctrl, targ_z, EdgeType::hadamard);

    return g;
}

// Y Gate
// NOTE - Cannot use mapSingleQubitGate

/**
 * @brief Get ZXGraph of Y = iXZ
 *
 * @return ZXGraph
 */
ZXGraph create_y_zx_form(QCirGate* gate) {
    ZXGraph g;
    size_t qubit = gate->get_qubits()[0]._qubit;

    ZXVertex* in = g.add_input(qubit);
    ZXVertex* x = g.add_vertex(qubit, VertexType::x, Phase(1));
    ZXVertex* z = g.add_vertex(qubit, VertexType::z, Phase(1));
    ZXVertex* out = g.add_output(qubit);
    g.add_edge(in, x, EdgeType::simple);
    g.add_edge(x, z, EdgeType::simple);
    g.add_edge(z, out, EdgeType::simple);

    return g;
}

/**
 * @brief Get ZXGraph of SY = S。SX。Sdg
 *
 * @return ZXGraph
 */
ZXGraph create_ry_zx_form(QCirGate* gate, Phase ph) {
    ZXGraph g;
    size_t qubit = gate->get_qubits()[0]._qubit;

    ZXVertex* in = g.add_input(qubit);
    ZXVertex* s = g.add_vertex(qubit, VertexType::z, Phase(1, 2));
    ZXVertex* sx = g.add_vertex(qubit, VertexType::x, ph);
    ZXVertex* sdg = g.add_vertex(qubit, VertexType::z, Phase(-1, 2));
    ZXVertex* out = g.add_output(qubit);
    g.add_edge(in, s, EdgeType::simple);
    g.add_edge(s, sx, EdgeType::simple);
    g.add_edge(sx, sdg, EdgeType::simple);
    g.add_edge(sdg, out, EdgeType::simple);

    return g;
}

}  // namespace detail

std::optional<ZXGraph> to_zxgraph(QCirGate* gate, size_t decomposition_mode) {
    switch (gate->get_type()) {
        // single-qubit gates
        case GateType::h:
            return detail::create_single_vertex_zx_form(gate, VertexType::h_box, Phase(1));
        case GateType::z:
            return detail::create_single_vertex_zx_form(gate, VertexType::z, Phase(1));
        case GateType::p:
        case GateType::rz:
            return detail::create_single_vertex_zx_form(gate, VertexType::z, gate->get_phase());
        case GateType::s:
            return detail::create_single_vertex_zx_form(gate, VertexType::z, Phase(1, 2));
        case GateType::t:
            return detail::create_single_vertex_zx_form(gate, VertexType::z, Phase(1, 4));
        case GateType::sdg:
            return detail::create_single_vertex_zx_form(gate, VertexType::z, Phase(-1, 2));
        case GateType::tdg:
            return detail::create_single_vertex_zx_form(gate, VertexType::z, Phase(-1, 4));
        case GateType::x:
            return detail::create_single_vertex_zx_form(gate, VertexType::x, Phase(1));
        case GateType::px:
        case GateType::rx:
            return detail::create_single_vertex_zx_form(gate, VertexType::x, gate->get_phase());
        case GateType::sx:
            return detail::create_single_vertex_zx_form(gate, VertexType::x, Phase(1, 2));
        case GateType::y:
            return detail::create_y_zx_form(gate);
        case GateType::py:
        case GateType::ry:
            return detail::create_ry_zx_form(gate, gate->get_phase());
        case GateType::sy:
            return detail::create_ry_zx_form(gate, Phase(1, 2));
            // double-qubit gates

        case GateType::cx:
            return detail::create_cx_zx_form(gate);
        case GateType::cz:
            return detail::create_cz_zx_form(gate);
        case GateType::swap:
            return detail::create_swap_zx_form(gate);

        // multi-qubit gates
        case GateType::mcrz:
            return detail::create_mcr_zx_form(gate->get_qubits(), gate->get_phase(), detail::RotationAxis::z);
        case GateType::mcp:
        case GateType::ccz:
            return detail::create_mcp_zx_form(gate->get_qubits(), gate->get_phase(), detail::RotationAxis::z);
        case GateType::ccx:
            return detail::create_ccx_zx_form(gate, decomposition_mode);
        case GateType::mcrx:
            return detail::create_mcr_zx_form(gate->get_qubits(), gate->get_phase(), detail::RotationAxis::x);
        case GateType::mcpx:
            return detail::create_mcp_zx_form(gate->get_qubits(), gate->get_phase(), detail::RotationAxis::x);
        case GateType::mcry:
            return detail::create_mcr_zx_form(gate->get_qubits(), gate->get_phase(), detail::RotationAxis::y);
        case GateType::mcpy:
            return detail::create_mcp_zx_form(gate->get_qubits(), gate->get_phase(), detail::RotationAxis::y);

        default:
            return std::nullopt;
    }
};

/**
 * @brief Mapping QCir to ZXGraph
 */
std::optional<ZXGraph> to_zxgraph(QCir const& qcir, size_t decomposition_mode) {
    qcir.update_gate_time();
    ZXGraph g;
    LOGGER.debug("Add boundaries");
    for (size_t i = 0; i < qcir.get_qubits().size(); i++) {
        ZXVertex* input = g.add_input(qcir.get_qubits()[i]->get_id());
        ZXVertex* output = g.add_output(qcir.get_qubits()[i]->get_id());
        input->set_col(0);
        g.add_edge(input, output, EdgeType::simple);
    }

    qcir.topological_traverse([&g, &decomposition_mode](QCirGate* gate) {
        if (stop_requested()) return;
        LOGGER.debug("Gate {} ({})", gate->get_id(), gate->get_type_str());

        auto tmp = to_zxgraph(gate, decomposition_mode);
        assert(tmp.has_value());

        for (auto& v : tmp->get_vertices()) {
            v->set_col(v->get_col() + gate->get_time() + gate->get_delay());
        }

        g.concatenate(*tmp);
    });

    size_t max = 0;
    for (auto& v : g.get_outputs()) {
        size_t neighbor_col = v->get_first_neighbor().first->get_col();
        if (neighbor_col > max) {
            max = neighbor_col;
        }
    }
    for (auto& v : g.get_outputs()) {
        v->set_col(max + 1);
    }

    if (stop_requested()) {
        LOGGER.warning("Conversion interrupted.");
        return std::nullopt;
    }

    return g;
}
