/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>

#include "./zxgraph.hpp"

namespace qsyn {

namespace zx {

/**
 * @brief Get non-boundary vertices
 *
 * @return ZXVertexList
 */
ZXVertexList ZXGraph::get_non_boundary_vertices() const {
    ZXVertexList tmp;
    tmp.clear();
    for (auto const& v : get_vertices()) {
        if (!v->is_boundary())
            tmp.emplace(v);
    }
    return tmp;
}

/**
 * @brief Get input vertex of qubit q
 *
 * @param q qubit
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::get_input_by_qubit(size_t const& q) {
    if (!_input_list.contains(q)) {
        spdlog::error("Input qubit id {} not found", q);
        return nullptr;
    } else
        return _input_list[q];
}

/**
 * @brief Get output vertex of qubit q
 *
 * @param q qubit
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::get_output_by_qubit(size_t const& q) {
    if (!_output_list.contains(q)) {
        spdlog::error("Output qubit id {} not found", q);
        return nullptr;
    } else
        return _output_list[q];
}

/**
 * @brief Strips the boundary other ZXGraph `other` and reconnect it to the output of the main graph. The main graph's output IDs are preserved
 *
 * @param other the other graph to concatenate with. This graph should have the same number of inputs and outputs
 */
void ZXGraph::concatenate(ZXGraph other, std::vector<size_t> const& qubits) {
    /* Visualization of what is done:
       ┌────┐                                ┌────┐
    i0─┤    ├─o0         ┌─────┐          i0─┤    ├─ o0 ┌─────┐
    i1─┤main├─o1  +  i1'─┤     ├─o1' -->  i1─┤main├─────┤     ├─o1
    i2─┤    ├─o2     i2'─┤other├─o2       i2─┤    ├─────┤other├─o2
       └────┘            └─────┘             └────┘     └─────┘
    */

    // auto tmp = other;
    // other    = tmp;

    if (other.num_inputs() != other.num_outputs()) {
        spdlog::error("Error: the graph being concatenated does not have the same number of inputs and outputs. Concatenation aborted!!");
        return;
    }
    // relabel qubit and rows
    for (auto* v : other.get_vertices()) {
        v->set_qubit(qubits[v->get_qubit()]);
        // if row is non-negative, it is a non-gadget qubit; and we would want to draw it on the correct row
        if (v->get_row() >= 0) {
            v->set_row(static_cast<float>(qubits[static_cast<size_t>(v->get_row())]));
        }
    }
    // Reconnect Input
    std::unordered_map<size_t, ZXVertex*> const copy_inputs = other.get_input_list();
    for (auto& [_, i] : copy_inputs) {
        auto [other_i_vtx, other_i_et] = other.get_first_neighbor(i);
        auto [this_o_vtx, this_o_et]   = other.get_first_neighbor(this->get_output_by_qubit(i->get_qubit()));

        this->remove_edge(this_o_vtx, this->get_output_by_qubit(i->get_qubit()), this_o_et);
        this->add_edge(this_o_vtx, other_i_vtx, concat_edge(this_o_et, other_i_et));
        other.remove_vertex(i);
    }

    // Reconnect Output
    std::unordered_map<size_t, ZXVertex*> const copy_outputs = other.get_output_list();
    for (auto& [_, o] : copy_outputs) {
        auto [other_o_vtx, etype] = other.get_first_neighbor(o);
        this->add_edge(other_o_vtx, this->get_output_by_qubit(o->get_qubit()), etype);
        other.remove_vertex(o);
    }
    this->_move_vertices_from(other);
}

}  // namespace zx

}  // namespace qsyn
