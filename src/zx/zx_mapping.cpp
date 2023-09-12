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
ZXVertexList ZXGraph::get_non_boundary_vertices() {
    ZXVertexList tmp;
    tmp.clear();
    for (auto const& v : _vertices) {
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
        std::cerr << "Input qubit id " << q << "not found" << std::endl;
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
        std::cerr << "Output qubit id " << q << "not found" << std::endl;
        return nullptr;
    } else
        return _output_list[q];
}

/**
 * @brief Strips the boundary other ZXGraph `other` and reconnect it to the output of the main graph. The main graph's output IDs are preserved
 *
 * @param other the other graph to concatenate with. This graph should have the same number of inputs and outputs
 */
void ZXGraph::concatenate(ZXGraph const& other) {
    /* Visualiztion of what is done:
       ┌────┐                                ┌────┐
    i0─┤    ├─o0         ┌─────┐          i0─┤    ├─ o0 ┌─────┐
    i1─┤main├─o1  +  i1'─┤     ├─o1' -->  i1─┤main├─────┤     ├─o1
    i2─┤    ├─o2     i2'─┤other├─o2       i2─┤    ├─────┤other├─o2
       └────┘            └─────┘             └────┘     └─────┘
    */

    if (other.get_num_inputs() != other.get_num_outputs()) {
        std::cerr << "Error: the graph being concatenated does not have the same number of inputs and outputs. Concatenation aborted!!\n";
        return;
    }

    ZXGraph copy{other};
    // Reconnect Input
    std::unordered_map<size_t, ZXVertex*> tmp_inputs = copy.get_input_list();
    for (auto& [qubit, i] : tmp_inputs) {
        auto [other_i_vtx, other_i_et] = i->get_first_neighbor();
        auto [this_o_vtx, this_o_et] = this->get_output_by_qubit(qubit)->get_first_neighbor();

        this->remove_edge(this_o_vtx, this->get_output_by_qubit(qubit), this_o_et);
        this->add_edge(this_o_vtx, other_i_vtx, concat_edge(this_o_et, other_i_et));
        copy.remove_vertex(i);
    }

    // Reconnect Output
    std::unordered_map<size_t, ZXVertex*> tmp_outputs = copy.get_output_list();
    for (auto& [qubit, o] : tmp_outputs) {
        auto [other_o_vtx, etype] = o->get_first_neighbor();
        this->add_edge(other_o_vtx, this->get_output_by_qubit(qubit), etype);
        copy.remove_vertex(o);
    }
    this->_move_vertices_from(copy);
}

}  // namespace zx

}  // namespace qsyn