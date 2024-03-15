/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>

#include <cstddef>
#include <gsl/narrow>
#include <queue>
#include <tl/zip.hpp>

#include "./zx_def.hpp"
#include "./zxgraph.hpp"
#include "util/phase.hpp"

namespace qsyn::zx {

/**
 * @brief Sort _inputs and _outputs of graph by qubit (ascending)
 *
 */
void ZXGraph::sort_io_by_qubit() {
    _inputs.sort([](ZXVertex* a, ZXVertex* b) { return a->get_qubit() < b->get_qubit(); });
    _outputs.sort([](ZXVertex* a, ZXVertex* b) { return a->get_qubit() < b->get_qubit(); });
}

/**
 * @brief Toggle a vertex between type Z and X, and toggle the edges adjacent to `v`. ( H -> S / S -> H)
 *        Ex: [(3, S), (4, H), (5, S)] -> [(3, H), (4, S), (5, H)]
 *
 * @param v
 */
void ZXGraph::toggle_vertex(ZXVertex* v) const {
    if (!v->is_z() && !v->is_x()) return;
    Neighbors toggled_neighbors;
    for (auto& [nb, etype] : this->get_neighbors(v)) {
        toggled_neighbors.emplace(nb, toggle_edge(etype));
        nb->_neighbors.erase({v, etype});
        nb->_neighbors.emplace(v, toggle_edge(etype));
    }
    v->_neighbors = toggled_neighbors;
    v->set_type(v->get_type() == VertexType::z ? VertexType::x : VertexType::z);
}

/**
 * @brief Lift each vertex's qubit in ZXGraph with `n`.
 *        Ex: origin: 0 -> after lifting: n
 *
 * @param n
 */
void ZXGraph::lift_qubit(int n) {
    for (auto const& v : _vertices) {
        v->set_row(v->get_row() + static_cast<float>(n));
    }

    for (auto const& i : _inputs) {
        i->set_qubit(i->get_qubit() + n);
    }
    for (auto const& o : _outputs) {
        o->set_qubit(o->get_qubit() + n);
    }

    std::unordered_map<size_t, ZXVertex*> new_input_list, new_output_list;

    std::for_each(_input_list.begin(), _input_list.end(),
                  [&n, &new_input_list](std::pair<size_t, ZXVertex*> itr) {
                      new_input_list[itr.first + n] = itr.second;
                  });
    std::for_each(_output_list.begin(), _output_list.end(),
                  [&n, &new_output_list](std::pair<size_t, ZXVertex*> itr) {
                      new_output_list[itr.first + n] = itr.second;
                  });

    _input_list  = new_input_list;
    _output_list = new_output_list;
}

/**
 * @brief Compose `target` to the original ZXGraph (horizontal concat)
 *
 * @param target
 * @return ZXGraph*
 */
ZXGraph& ZXGraph::compose(ZXGraph const& target) {
    // Check ori-outputNum == target-inputNum
    if (this->get_num_outputs() != target.get_num_inputs()) {
        spdlog::error("Error: The composing ZXGraph's #input is not equivalent to the original ZXGraph's #output.");
        return *this;
    }

    ZXGraph copied_graph{target};

    // Get maximum column in `this`
    auto max_col = gsl::narrow_cast<unsigned>(std::ranges::max(get_vertices() | std::views::transform([](ZXVertex* v) { return v->get_col(); })));

    // Update `_col` of copiedGraph to make them unique to the original graph
    for (auto const& v : copied_graph.get_vertices()) {
        v->set_col(v->get_col() + static_cast<float>(max_col) + 1);
    }

    // Sort ori-output and copy-input
    this->sort_io_by_qubit();
    copied_graph.sort_io_by_qubit();

    // Change ori-output and copy-inputs' vt to Z and link them respectively

    auto itr_ori = _outputs.begin();
    auto itr_cop = copied_graph.get_inputs().begin();
    for (; itr_ori != _outputs.end(); ++itr_ori, ++itr_cop) {
        (*itr_ori)->set_type(VertexType::z);
        (*itr_cop)->set_type(VertexType::z);
        this->add_edge((*itr_ori), (*itr_cop), EdgeType::simple);
    }

    _outputs     = copied_graph._outputs;
    _output_list = copied_graph._output_list;

    this->_move_vertices_from(copied_graph);

    return *this;
}

/**
 * @brief Tensor `target` to the original ZXGraph (vertical concat)
 *
 * @param target
 * @return ZXGraph*
 */
ZXGraph& ZXGraph::tensor_product(ZXGraph const& target) {
    ZXGraph copied_graph{target};

    // Lift Qubit
    QubitIdType ori_max_qubit = INT_MIN, ori_min_qubit = INT_MAX;
    QubitIdType copied_min_qubit = INT_MAX;
    for (auto const& i : get_inputs()) {
        if (i->get_qubit() > ori_max_qubit) ori_max_qubit = i->get_qubit();
        if (i->get_qubit() < ori_min_qubit) ori_min_qubit = i->get_qubit();
    }
    for (auto const& i : get_outputs()) {
        if (i->get_qubit() > ori_max_qubit) ori_max_qubit = i->get_qubit();
        if (i->get_qubit() < ori_min_qubit) ori_min_qubit = i->get_qubit();
    }

    for (auto const& i : copied_graph.get_inputs()) {
        if (i->get_qubit() < copied_min_qubit) copied_min_qubit = i->get_qubit();
    }
    for (auto const& i : copied_graph.get_outputs()) {
        if (i->get_qubit() < copied_min_qubit) copied_min_qubit = i->get_qubit();
    }
    auto lift_q = (ori_max_qubit - ori_min_qubit + 1) - copied_min_qubit;
    copied_graph.lift_qubit(lift_q);

    // Merge copiedGraph to original graph
    _inputs.insert(copied_graph._inputs.begin(), copied_graph._inputs.end());
    _input_list.merge(copied_graph._input_list);
    _outputs.insert(copied_graph._outputs.begin(), copied_graph._outputs.end());
    _output_list.merge(copied_graph._output_list);

    this->_move_vertices_from(copied_graph);

    return *this;
}

/**
 * @brief Check if v is a gadget leaf
 *
 * @param v
 * @return true
 * @return false
 */
bool ZXGraph::is_gadget_leaf(ZXVertex* v) const {
    return v->get_type() == VertexType::z &&
           this->get_num_neighbors(v) == 1 &&
           this->get_first_neighbor(v).first->get_type() == VertexType::z &&
           this->get_first_neighbor(v).second == EdgeType::hadamard &&
           this->get_first_neighbor(v).first->has_n_pi_phase();
}

/**
 * @brief Check if v is a gadget axel
 *
 * @param v
 * @return true
 * @return false
 */
bool ZXGraph::is_gadget_axel(ZXVertex* v) const {
    return v->is_z() &&
           v->has_n_pi_phase() &&
           std::ranges::any_of(this->get_neighbors(v),
                               [this](NeighborPair const& nbp) {
                                   return this->get_num_neighbors(nbp.first) == 1 && nbp.first->is_z() && nbp.second == EdgeType::hadamard;
                               });
}

bool ZXGraph::has_dangling_neighbors(ZXVertex* v) const {
    return std::ranges::any_of(this->get_neighbors(v),
                               [this](NeighborPair const& nbp) {
                                   return this->get_num_neighbors(nbp.first) == 1;
                               });
}

/**
 * @brief Add phase gadget of phase `p` for each vertex in `verVec`.
 *
 * @param p
 * @param verVec
 */
void ZXGraph::add_gadget(Phase p, std::vector<ZXVertex*> const& vertices) {
    for (size_t i = 0; i < vertices.size(); i++) {
        if (vertices[i]->get_type() == VertexType::boundary || vertices[i]->get_type() == VertexType::h_box) return;
    }

    ZXVertex* axel = add_vertex(VertexType::z, Phase(0), -1);
    ZXVertex* leaf = add_vertex(VertexType::z, p, -2);

    add_edge(axel, leaf, EdgeType::hadamard);
    for (auto const& v : vertices) add_edge(v, axel, EdgeType::hadamard);
}

/**
 * @brief Remove phase gadget `v`. (Auto-check if `v` is a gadget first!)
 *
 * @param v
 */
void ZXGraph::remove_gadget(ZXVertex* v) {
    if (!is_gadget_leaf(v)) return;
    ZXVertex* axel = this->get_first_neighbor(v).first;
    remove_vertex(axel);
    remove_vertex(v);
}

/**
 * @brief Generate a id-2-ZXVertex* map
 *
 * @return unordered_map<size_t, ZXVertex*>
 */
std::unordered_map<size_t, ZXVertex*> ZXGraph::create_id_to_vertex_map() const {
    std::unordered_map<size_t, ZXVertex*> id2_vertex_map;
    for (auto const& v : _vertices) id2_vertex_map[v->get_id()] = v;
    return id2_vertex_map;
}

/**
 * @brief Rearrange vertices on each qubit so that each vertex can be separated in the printed graph.
 *
 */
void ZXGraph::adjust_vertex_coordinates() {
    // FIXME - QubitId -> RowId
    std::unordered_map<float, std::vector<ZXVertex*>> row_to_vertices_map;
    std::unordered_set<QubitIdType> visited_rows;
    std::vector<ZXVertex*> vertex_queue;
    // NOTE - Check Gadgets
    // FIXME - When replacing QubitId with RowId, add 0.5 on it

    // REVIEW - Whether to move the vertex from row -2 when it is no longer a gadget
    // for (auto const& i : _vertices) {
    //     if (i->get_qubit() == -2 && get_num_neighbors(i) > 1) {
    //         std::unordered_map<QubitIdType, size_t> num_neighbor_qubits;
    //         for (auto const& [nb, _] : get_neighbors(i)) {
    //             if (num_neighbor_qubits.contains(nb->get_qubit())) {
    //                 num_neighbor_qubits[nb->get_qubit()]++;
    //             } else
    //                 num_neighbor_qubits[nb->get_qubit()] = 1;
    //         }
    //         // fmt::println("move to {}", (*max_element(num_neighbor_qubits.begin(), num_neighbor_qubits.end(), [](const std::pair<QubitIdType, size_t>& p1, const std::pair<QubitIdType, size_t>& p2) { return p1.second < p2.second; })).first);
    //         i->set_qubit((*max_element(num_neighbor_qubits.begin(), num_neighbor_qubits.end(), [](const std::pair<QubitIdType, size_t>& p1, const std::pair<QubitIdType, size_t>& p2) { return p1.second < p2.second; })).first);
    //     }
    // }

    for (auto const& i : _inputs) {
        vertex_queue.emplace_back(i);
        visited_rows.insert(gsl::narrow<QubitIdType>(i->get_id()));
    }
    while (!vertex_queue.empty()) {
        ZXVertex* v = vertex_queue.front();
        vertex_queue.erase(vertex_queue.begin());
        row_to_vertices_map[v->get_row()].emplace_back(v);
        for (auto const& nb : get_neighbors(v) | std::views::keys) {
            if (visited_rows.find(gsl::narrow<QubitIdType>(nb->get_id())) == visited_rows.end()) {
                vertex_queue.emplace_back(nb);
                visited_rows.insert(gsl::narrow<QubitIdType>(nb->get_id()));
            }
        }
    }
    std::vector<ZXVertex*> gadgets;
    float non_gadget = 0;
    for (size_t i = 0; i < row_to_vertices_map[-2].size(); i++) {
        if (get_num_neighbors(row_to_vertices_map[-2][i]) == 1) {  // Not Gadgets
            gadgets.emplace_back(row_to_vertices_map[-2][i]);
        } else
            non_gadget++;
    }
    std::erase_if(row_to_vertices_map[-2], [this](ZXVertex* v) { return this->get_num_neighbors(v) == 1; });

    row_to_vertices_map[-2].insert(row_to_vertices_map[-2].end(), gadgets.begin(), gadgets.end());

    for (auto const& [qid, vertices] : row_to_vertices_map) {
        auto col = std::invoke([qid = qid, non_gadget]() -> float {
            if (qid == -2) return 0.5;
            if (qid == -1) return 0.5f + non_gadget;
            return 0.0f;
        });
        for (auto const& v : vertices) {
            v->set_col(col);
            col++;
        }
    }

    auto const max_col = std::ceil(
        std::ranges::max(
            row_to_vertices_map |
            std::views::values |
            std::views::transform([](std::vector<ZXVertex*> const& v) {
                return std::ranges::max(v | std::views::transform([](ZXVertex* v) { return v->get_col(); }));
            })));

    for (auto& o : _outputs) o->set_col(max_col);
}

}  // namespace qsyn::zx
