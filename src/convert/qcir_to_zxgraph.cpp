/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion from QCir to ZXGraph ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_to_zxgraph.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <gsl/narrow>

#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "util/phase.hpp"
#include "util/rational.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"

extern bool stop_requested();

namespace qsyn {

using zx::ZXVertex, zx::ZXGraph, zx::VertexType, zx::EdgeType;

using qcir::QCirGate, qcir::GateRotationCategory, qcir::QCir;

namespace {

dvlab::Phase get_gadget_phase(dvlab::Phase const& phase, size_t n_qubits) {
    return phase * dvlab::Rational(1, static_cast<int>(std::pow(2, gsl::narrow<double>(n_qubits) - 1)));
}

enum class RotationAxis {
    x,
    y,
    z
};

std::pair<std::vector<ZXVertex*>, ZXVertex*>
create_multi_control_backbone(ZXGraph& g, QCirGate const& gate, RotationAxis ax) {
    std::vector<ZXVertex*> controls;
    ZXVertex* target  = nullptr;
    auto target_qubit = gate.get_operand(gate.get_num_qubits() - 1);
    for (auto i : std::views::iota(0ul, gate.get_num_qubits())) {
        auto qubit    = gate.get_operand(i);
        ZXVertex* in  = g.add_input(qubit);
        ZXVertex* v   = g.add_vertex(VertexType::z, dvlab::Phase{}, static_cast<float>(qubit));
        ZXVertex* out = g.add_output(qubit);
        if (ax == RotationAxis::z || qubit != target_qubit) {
            g.add_edge(in, v, EdgeType::simple);
            g.add_edge(v, out, EdgeType::simple);
        } else {
            g.add_edge(in, v, EdgeType::hadamard);
            g.add_edge(v, out, EdgeType::hadamard);
            if (ax == RotationAxis::y) {
                g.add_buffer(in, v, EdgeType::hadamard)->set_phase(dvlab::Phase(-1, 2));
                g.add_buffer(out, v, EdgeType::hadamard)->set_phase(dvlab::Phase(1, 2));
            }
        }
        if (qubit != target_qubit)
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
void make_combinations_helper(std::vector<std::vector<ZXVertex*>>& comb, std::vector<ZXVertex*>& tmp, std::vector<ZXVertex*> const& vertices, size_t left, size_t k) {
    if (k == 0) {
        comb.emplace_back(tmp);
        return;
    }
    for (size_t i = left; i < vertices.size(); ++i) {
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
std::vector<std::vector<ZXVertex*>> make_combinations(std::vector<ZXVertex*> const& vertices, size_t k) {
    std::vector<std::vector<ZXVertex*>> comb;
    std::vector<ZXVertex*> tmp;
    make_combinations_helper(comb, tmp, vertices, 0, k);
    return comb;
}

void create_multi_control_r_gate_gadgets(ZXGraph& g, std::vector<ZXVertex*> const& controls, ZXVertex* target, dvlab::Phase const& phase) {
    target->set_phase(phase);
    for (size_t k = 1; k <= controls.size(); k++) {
        std::vector<std::vector<ZXVertex*>> combinations = make_combinations(controls, k);
        for (auto& combination : combinations) {
            combination.emplace_back(target);
            g.add_gadget((combination.size() % 2) ? phase : -phase, combination);
        }
    }
}

void create_multi_control_p_gate_gadgets(ZXGraph& g, std::vector<ZXVertex*> const& vertices, dvlab::Phase const& phase) {
    for (auto& v : vertices) {
        v->set_phase(phase);
    }
    for (size_t k = 2; k <= vertices.size(); k++) {
        auto const combinations = make_combinations(vertices, k);
        for (auto& combination : combinations) {
            g.add_gadget((combination.size() % 2) ? phase : -phase, combination);
        }
    }
}

ZXGraph create_mcr_zx_form(QCirGate const& gate, RotationAxis ax) {
    ZXGraph g;
    auto const gadget_phase = get_gadget_phase(gate.get_phase(), gate.get_num_qubits());

    auto const [controls, target] = create_multi_control_backbone(g, gate, ax);

    create_multi_control_r_gate_gadgets(g, controls, target, gadget_phase);

    return g;
}

ZXGraph create_mcp_zx_form(QCirGate const& gate, RotationAxis ax) {
    ZXGraph g;
    auto const gadget_phase = get_gadget_phase(gate.get_phase(), gate.get_num_qubits());

    auto [vertices, target] = create_multi_control_backbone(g, gate, ax);
    vertices.emplace_back(target);

    create_multi_control_p_gate_gadgets(g, vertices, gadget_phase);

    return g;
}

/**
 * @brief Map single qubit gate to ZXGraph
 *
 * @param vt
 * @param ph
 * @return ZXGraph
 */
ZXGraph create_single_vertex_zx_form(QCirGate* gate, VertexType vt) {
    ZXGraph g;
    auto qubit = gate->get_operand(0);

    ZXVertex* in  = g.add_input(qubit);
    ZXVertex* v   = g.add_vertex(vt, gate->get_phase(), static_cast<float>(qubit));
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
    auto ctrl_qubit = gate->get_qubits()[0]._isTarget ? gate->get_operand(1) : gate->get_operand(0);
    auto targ_qubit = gate->get_qubits()[0]._isTarget ? gate->get_operand(0) : gate->get_operand(1);

    ZXVertex* in_ctrl  = g.add_input(ctrl_qubit);
    ZXVertex* in_targ  = g.add_input(targ_qubit);
    ZXVertex* ctrl     = g.add_vertex(VertexType::z, dvlab::Phase(0), static_cast<float>(ctrl_qubit));
    ZXVertex* targ_x   = g.add_vertex(VertexType::x, dvlab::Phase(0), static_cast<float>(targ_qubit));
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
    auto ctrl_qubit_2 = gate->get_qubits()[0]._isTarget ? gate->get_operand(1) : gate->get_operand(0);
    auto ctrl_qubit_1 = gate->get_qubits()[0]._isTarget ? gate->get_operand(2) : (gate->get_qubits()[1]._isTarget ? gate->get_operand(2) : gate->get_operand(1));
    auto targ_qubit   = gate->get_qubits()[0]._isTarget ? gate->get_operand(0) : (gate->get_qubits()[1]._isTarget ? gate->get_operand(1) : gate->get_operand(2));
    std::vector<std::pair<std::pair<VertexType, dvlab::Phase>, QubitIdType>> vertices_info;
    std::vector<std::pair<std::pair<size_t, size_t>, EdgeType>> adj_pair;
    std::vector<float> vertices_col;
    std::vector<ZXVertex*> vertices_list = {};
    ZXVertex* in_ctrl_1                  = nullptr;
    ZXVertex* in_ctrl_2                  = nullptr;
    ZXVertex* in_targ                    = nullptr;
    ZXVertex* out_ctrl_1                 = nullptr;
    ZXVertex* out_ctrl_2                 = nullptr;
    ZXVertex* out_targ                   = nullptr;
    if (decomposition_mode == 1) {
        vertices_info = {{{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(-1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(-1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(-1, 4)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_1}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_1}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_1}, {{VertexType::z, dvlab::Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_1}};
        adj_pair      = {{{0, 1}, EdgeType::hadamard}, {{1, 10}, EdgeType::hadamard}, {{1, 2}, EdgeType::hadamard}, {{2, 3}, EdgeType::hadamard}, {{3, 16}, EdgeType::hadamard}, {{3, 4}, EdgeType::hadamard}, {{4, 5}, EdgeType::hadamard}, {{5, 11}, EdgeType::hadamard}, {{5, 6}, EdgeType::hadamard}, {{6, 7}, EdgeType::hadamard}, {{7, 17}, EdgeType::hadamard}, {{7, 8}, EdgeType::hadamard}, {{8, 9}, EdgeType::hadamard}, {{10, 11}, EdgeType::simple}, {{11, 12}, EdgeType::simple}, {{12, 13}, EdgeType::hadamard}, {{13, 18}, EdgeType::hadamard}, {{13, 14}, EdgeType::hadamard}, {{14, 15}, EdgeType::hadamard}, {{15, 20}, EdgeType::hadamard}, {{16, 17}, EdgeType::simple}, {{17, 18}, EdgeType::simple}, {{18, 19}, EdgeType::simple}, {{19, 20}, EdgeType::simple}};

        vertices_col = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 2, 6, 7, 9, 10, 11, 4, 8, 9, 10, 11};

        in_ctrl_1 = g.add_input(ctrl_qubit_1, 0);
        in_ctrl_2 = g.add_input(ctrl_qubit_2, 0);
        in_targ   = g.add_input(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.add_vertex(vertices_info[i].first.first, vertices_info[i].first.second, static_cast<float>(vertices_info[i].second), vertices_col[i]));
        };
        out_ctrl_1 = g.add_output(ctrl_qubit_1, 12);
        out_ctrl_2 = g.add_output(ctrl_qubit_2, 12);
        out_targ   = g.add_output(targ_qubit, 12);
        g.add_edge(in_ctrl_1, vertices_list[16], EdgeType::simple);
        g.add_edge(in_ctrl_2, vertices_list[10], EdgeType::simple);
        g.add_edge(in_targ, vertices_list[0], EdgeType::hadamard);
        g.add_edge(out_ctrl_1, vertices_list[20], EdgeType::simple);
        g.add_edge(out_ctrl_2, vertices_list[15], EdgeType::hadamard);
        g.add_edge(out_targ, vertices_list[9], EdgeType::simple);
    } else if (decomposition_mode == 2) {
        vertices_info = {{{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(-1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(-1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(-1, 4)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(1, 4)}, ctrl_qubit_1}};
        adj_pair      = {{{0, 1}, EdgeType::hadamard}, {{0, 8}, EdgeType::hadamard}, {{1, 2}, EdgeType::hadamard}, {{2, 12}, EdgeType::hadamard}, {{2, 3}, EdgeType::hadamard}, {{3, 4}, EdgeType::hadamard}, {{4, 8}, EdgeType::hadamard}, {{4, 5}, EdgeType::hadamard}, {{5, 6}, EdgeType::hadamard}, {{6, 12}, EdgeType::hadamard}, {{6, 7}, EdgeType::hadamard}, {{8, 9}, EdgeType::hadamard}, {{9, 12}, EdgeType::hadamard}, {{9, 10}, EdgeType::hadamard}, {{10, 11}, EdgeType::hadamard}, {{11, 12}, EdgeType::hadamard}};

        vertices_col = {2, 3, 4, 5, 6, 7, 8, 9, 2, 9, 10, 11, 4};

        in_ctrl_1 = g.add_input(ctrl_qubit_1, 0);
        in_ctrl_2 = g.add_input(ctrl_qubit_2, 0);
        in_targ   = g.add_input(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.add_vertex(vertices_info[i].first.first, vertices_info[i].first.second, static_cast<float>(vertices_info[i].second), vertices_col[i]));
        };
        out_ctrl_1 = g.add_output(ctrl_qubit_1, 12);
        out_ctrl_2 = g.add_output(ctrl_qubit_2, 12);
        out_targ   = g.add_output(targ_qubit, 12);
        g.add_edge(in_ctrl_1, vertices_list[12], EdgeType::simple);
        g.add_edge(in_ctrl_2, vertices_list[8], EdgeType::simple);
        g.add_edge(in_targ, vertices_list[0], EdgeType::simple);
        g.add_edge(out_ctrl_1, vertices_list[12], EdgeType::simple);
        g.add_edge(out_ctrl_2, vertices_list[11], EdgeType::hadamard);
        g.add_edge(out_targ, vertices_list[7], EdgeType::hadamard);
    } else if (decomposition_mode == 3) {
        vertices_info = {{{VertexType::z, dvlab::Phase(1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::z, dvlab::Phase(1, 4)}, -2}, {{VertexType::z, dvlab::Phase(0)}, -1}, {{VertexType::z, dvlab::Phase(-1, 4)}, -2}, {{VertexType::z, dvlab::Phase(0)}, -1}, {{VertexType::z, dvlab::Phase(-1, 4)}, -2}, {{VertexType::z, dvlab::Phase(0)}, -1}, {{VertexType::z, dvlab::Phase(-1, 4)}, -2}, {{VertexType::z, dvlab::Phase(0)}, -1}};
        adj_pair      = {{{0, 4}, EdgeType::hadamard}, {{0, 6}, EdgeType::hadamard}, {{0, 8}, EdgeType::hadamard}, {{1, 4}, EdgeType::hadamard}, {{1, 6}, EdgeType::hadamard}, {{1, 10}, EdgeType::hadamard}, {{2, 4}, EdgeType::hadamard}, {{2, 8}, EdgeType::hadamard}, {{2, 10}, EdgeType::hadamard}, {{3, 4}, EdgeType::hadamard}, {{5, 6}, EdgeType::hadamard}, {{7, 8}, EdgeType::hadamard}, {{9, 10}, EdgeType::hadamard}};
        vertices_col  = {5, 5, 5, 1, 1, 2, 2, 3, 3, 4, 4};

        in_ctrl_1 = g.add_input(ctrl_qubit_1, 0);
        in_ctrl_2 = g.add_input(ctrl_qubit_2, 0);
        in_targ   = g.add_input(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.add_vertex(vertices_info[i].first.first, vertices_info[i].first.second, static_cast<float>(vertices_info[i].second), vertices_col[i]));
        };
        out_ctrl_1 = g.add_output(ctrl_qubit_1, 6);
        out_ctrl_2 = g.add_output(ctrl_qubit_2, 6);
        out_targ   = g.add_output(targ_qubit, 6);
        g.add_edge(in_ctrl_1, vertices_list[2], EdgeType::simple);
        g.add_edge(in_ctrl_2, vertices_list[1], EdgeType::simple);
        g.add_edge(in_targ, vertices_list[0], EdgeType::hadamard);
        g.add_edge(out_ctrl_1, vertices_list[2], EdgeType::simple);
        g.add_edge(out_ctrl_2, vertices_list[1], EdgeType::simple);
        g.add_edge(out_targ, vertices_list[0], EdgeType::hadamard);
    } else {
        vertices_info = {{{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::x, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(-1, 4)}, targ_qubit}, {{VertexType::x, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(1, 4)}, targ_qubit}, {{VertexType::x, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(-1, 4)}, targ_qubit}, {{VertexType::x, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(1, 4)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, targ_qubit}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(1, 4)}, ctrl_qubit_2}, {{VertexType::x, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(-1, 4)}, ctrl_qubit_2}, {{VertexType::x, dvlab::Phase(0)}, ctrl_qubit_2}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_1}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_1}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_1}, {{VertexType::z, dvlab::Phase(1, 4)}, ctrl_qubit_1}, {{VertexType::z, dvlab::Phase(0)}, ctrl_qubit_1}};
        adj_pair      = {{{0, 1}, EdgeType::simple}, {{1, 10}, EdgeType::simple}, {{1, 2}, EdgeType::simple}, {{2, 3}, EdgeType::simple}, {{3, 16}, EdgeType::simple}, {{3, 4}, EdgeType::simple}, {{4, 5}, EdgeType::simple}, {{5, 11}, EdgeType::simple}, {{5, 6}, EdgeType::simple}, {{6, 7}, EdgeType::simple}, {{7, 17}, EdgeType::simple}, {{7, 8}, EdgeType::simple}, {{8, 9}, EdgeType::hadamard}, {{10, 11}, EdgeType::simple}, {{11, 12}, EdgeType::simple}, {{12, 13}, EdgeType::simple}, {{13, 18}, EdgeType::simple}, {{13, 14}, EdgeType::simple}, {{14, 15}, EdgeType::simple}, {{15, 20}, EdgeType::simple}, {{16, 17}, EdgeType::simple}, {{17, 18}, EdgeType::simple}, {{18, 19}, EdgeType::simple}, {{19, 20}, EdgeType::simple}};

        vertices_col = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 2, 6, 7, 9, 10, 11, 4, 8, 9, 10, 11};

        in_ctrl_1 = g.add_input(ctrl_qubit_1, 0);
        in_ctrl_2 = g.add_input(ctrl_qubit_2, 0);
        in_targ   = g.add_input(targ_qubit, 0);
        for (size_t i = 0; i < vertices_info.size(); i++) {
            vertices_list.emplace_back(g.add_vertex(vertices_info[i].first.first, vertices_info[i].first.second, static_cast<float>(vertices_info[i].second), vertices_col[i]));
        };
        out_ctrl_1 = g.add_output(ctrl_qubit_1, 12);
        out_ctrl_2 = g.add_output(ctrl_qubit_2, 12);
        out_targ   = g.add_output(targ_qubit, 12);

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
    auto qb0 = gate->get_operand(0);
    auto qb1 = gate->get_operand(1);
    auto i0  = g.add_input(qb0, 0);
    auto o0  = g.add_output(qb0, 1);
    auto i1  = g.add_input(qb1, 0);
    auto o1  = g.add_output(qb1, 1);
    g.add_edge(i0, o1, EdgeType::simple);
    g.add_edge(i1, o0, EdgeType::simple);

    return g;
}

ZXGraph create_ecr_zx_form(QCirGate* gate) {
    ZXGraph g;
    auto qb0 = gate->get_operand(0);
    auto qb1 = gate->get_operand(1);
    auto i0  = g.add_input(qb0, 0);
    auto o0  = g.add_output(qb0, 3);
    auto i1  = g.add_input(qb1, 0);
    auto o1  = g.add_output(qb1, 3);
    auto s0  = g.add_vertex(VertexType::z, dvlab::Phase(1, 2), 0, 1);
    auto v1  = g.add_vertex(VertexType::x, dvlab::Phase(1, 2), 1, 1);
    auto x0  = g.add_vertex(VertexType::x, dvlab::Phase(1), 0, 2);

    g.add_edge(i0, s0, EdgeType::simple);
    g.add_edge(s0, x0, EdgeType::simple);
    g.add_edge(x0, o0, EdgeType::simple);
    g.add_edge(i1, v1, EdgeType::simple);
    g.add_edge(v1, o1, EdgeType::simple);
    g.add_edge(s0, v1, EdgeType::simple);

    return g;
}

/**
 * @brief Get ZXGraph of CZ
 *
 * @return ZXGraph
 */
ZXGraph create_cz_zx_form(QCirGate* gate) {
    ZXGraph g;
    auto ctrl_qubit = gate->get_qubits()[0]._isTarget ? gate->get_operand(1) : gate->get_operand(0);
    auto targ_qubit = gate->get_qubits()[0]._isTarget ? gate->get_operand(0) : gate->get_operand(1);

    ZXVertex* in_ctrl  = g.add_input(ctrl_qubit);
    ZXVertex* in_targ  = g.add_input(targ_qubit);
    ZXVertex* ctrl     = g.add_vertex(VertexType::z, dvlab::Phase(0), static_cast<float>(ctrl_qubit));
    ZXVertex* targ_z   = g.add_vertex(VertexType::z, dvlab::Phase(0), static_cast<float>(targ_qubit));
    ZXVertex* out_ctrl = g.add_output(ctrl_qubit);
    ZXVertex* out_targ = g.add_output(targ_qubit);
    g.add_edge(in_ctrl, ctrl, EdgeType::simple);
    g.add_edge(ctrl, out_ctrl, EdgeType::simple);
    g.add_edge(in_targ, targ_z, EdgeType::simple);
    g.add_edge(targ_z, out_targ, EdgeType::simple);
    g.add_edge(ctrl, targ_z, EdgeType::hadamard);

    return g;
}

/**
 * @brief Get ZXGraph of SY = S。SX。Sdg
 *
 * @return ZXGraph
 */
ZXGraph create_ry_zx_form(QCirGate* gate) {
    ZXGraph g;
    auto qubit = gate->get_operand(0);

    ZXVertex* in  = g.add_input(qubit);
    ZXVertex* sdg = g.add_vertex(VertexType::z, dvlab::Phase(-1, 2), static_cast<float>(qubit));
    ZXVertex* rx  = g.add_vertex(VertexType::x, gate->get_phase(), static_cast<float>(qubit));
    ZXVertex* s   = g.add_vertex(VertexType::z, dvlab::Phase(1, 2), static_cast<float>(qubit));
    ZXVertex* out = g.add_output(qubit);
    g.add_edge(in, sdg, EdgeType::simple);
    g.add_edge(sdg, rx, EdgeType::simple);
    g.add_edge(rx, s, EdgeType::simple);
    g.add_edge(s, out, EdgeType::simple);

    return g;
}

}  // namespace

std::optional<ZXGraph> to_zxgraph(QCirGate* gate, size_t decomposition_mode) {
    switch (gate->get_rotation_category()) {
        // single-qubit gates
        case GateRotationCategory::h:
            return create_single_vertex_zx_form(gate, VertexType::h_box);
        case GateRotationCategory::rz:
            if (gate->get_num_qubits() == 1) {
                return create_single_vertex_zx_form(gate, VertexType::z);
            } else {
                return create_mcr_zx_form(*gate, RotationAxis::z);
            }
        case GateRotationCategory::rx:
            if (gate->get_num_qubits() == 1) {
                return create_single_vertex_zx_form(gate, VertexType::x);
            } else {
                return create_mcr_zx_form(*gate, RotationAxis::x);
            }
        case GateRotationCategory::ry:
            if (gate->get_num_qubits() == 1) {
                return create_ry_zx_form(gate);
            } else {
                return create_mcr_zx_form(*gate, RotationAxis::y);
            }
        // multi-qubit gates
        case GateRotationCategory::swap:
            return create_swap_zx_form(gate);

        case GateRotationCategory::ecr:
            return create_ecr_zx_form(gate);

        case GateRotationCategory::pz:
            if (gate->get_num_qubits() == 1) {
                return create_single_vertex_zx_form(gate, VertexType::z);
            } else if (gate->get_num_qubits() == 2 && gate->get_phase() == Phase(1)) {
                return create_cz_zx_form(gate);
            } else {
                return create_mcp_zx_form(*gate, RotationAxis::z);
            }
        case GateRotationCategory::px:
            if (gate->get_num_qubits() == 1) {
                return create_single_vertex_zx_form(gate, VertexType::x);
            } else if (gate->get_num_qubits() == 2 && gate->get_phase() == Phase(1)) {
                return create_cx_zx_form(gate);
            } else if (gate->get_num_qubits() == 3 && gate->get_phase() == Phase(1)) {
                return create_ccx_zx_form(gate, decomposition_mode);
            } else {
                return create_mcp_zx_form(*gate, RotationAxis::x);
            }
        case GateRotationCategory::py:
            if (gate->get_num_qubits() == 1) {
                return create_ry_zx_form(gate);
            } else {
                return create_mcp_zx_form(*gate, RotationAxis::y);
            }
        default:
            return std::nullopt;
    }
};

/**
 * @brief Mapping QCir to ZXGraph
 */
std::optional<ZXGraph> to_zxgraph(QCir const& qcir, size_t decomposition_mode) {
    if (qcir.is_empty()) {
        spdlog::error("QCir is empty!!");
        return std::nullopt;
    }
    auto const times = qcir.calculate_gate_times();

    ZXGraph graph;
    spdlog::debug("Add boundaries");
    for (size_t i = 0; i < qcir.get_qubits().size(); i++) {
        ZXVertex* input  = graph.add_input(qcir.get_qubits()[i]->get_id(), 0);
        ZXVertex* output = graph.add_output(qcir.get_qubits()[i]->get_id());
        graph.add_edge(input, output, EdgeType::simple);
    }

    for (auto const& gate : qcir.get_gates()) {
        if (stop_requested()) {
            spdlog::warn("Conversion interrupted.");
            return std::nullopt;
        }
        spdlog::debug("Gate {} ({})", gate->get_id(), gate->get_type_str());

        auto tmp = to_zxgraph(gate, decomposition_mode);
        if (!tmp) {
            spdlog::error("Conversion of Gate {} ({}) to ZXGraph is not supported yet!!", gate->get_id(), gate->get_type_str());
            return std::nullopt;
        }

        for (auto& v : tmp->get_vertices()) {
            v->set_col(v->get_col() + static_cast<float>(times.at(gate->get_id())));
        }

        graph.concatenate(*tmp);
    }

    auto const max_col = std::ranges::max(graph.get_outputs() | std::views::transform([&graph](ZXVertex* v) { return graph.get_first_neighbor(v).first->get_col(); }));
    for (auto& v : graph.get_outputs()) {
        v->set_col(max_col + 1);
    }

    if (stop_requested()) {
        spdlog::warn("Conversion interrupted.");
        return std::nullopt;
    }

    return graph;
}

}  // namespace qsyn
