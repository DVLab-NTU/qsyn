/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zx/zxgraph_action.hpp"

#include <fmt/core.h>

#include <cstddef>
#include <gsl/narrow>
#include <tl/zip.hpp>
#include <tuple>

#include "./zx_def.hpp"
#include "./zxgraph.hpp"
#include "qsyn/qsyn_type.hpp"

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
 * @brief Lift each vertex's qubit in ZXGraph with `n`.
 *        Ex: origin: 0 -> after lifting: n
 *
 * @param n
 */
void ZXGraph::lift_qubit(ssize_t n) {
    for (auto const& v : get_vertices()) {
        if (v->get_row() >= 0) {
            v->set_row(v->get_row() + static_cast<float>(n));
        }
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
    QubitIdType ori_max_qubit = min_qubit_id, ori_min_qubit = max_qubit_id;
    QubitIdType copied_min_qubit = max_qubit_id;
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
    auto lift_q = static_cast<ssize_t>((ori_max_qubit - ori_min_qubit + 1) - copied_min_qubit);
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
    for (auto const& v : get_vertices()) id2_vertex_map[v->get_id()] = v;
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
        visited_rows.insert(i->get_id());
    }
    while (!vertex_queue.empty()) {
        ZXVertex* v = vertex_queue.front();
        vertex_queue.erase(vertex_queue.begin());
        row_to_vertices_map[v->get_row()].emplace_back(v);
        for (auto const& nb : get_neighbors(v) | std::views::keys) {
            if (!dvlab::contains(visited_rows, nb->get_id())) {
                vertex_queue.emplace_back(nb);
                visited_rows.insert(nb->get_id());
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
                return v.empty() ? 0 : std::ranges::max(v | std::views::transform([](ZXVertex* v) { return v->get_col(); }));
            })));
    for (auto& o : _outputs) o->set_col(max_col);
}

// free functions for editing ZXGraph

/**
 * @brief Toggle a vertex between type Z and X, and toggle the adjacent edges.
 *
 * @param v
 */
void toggle_vertex(ZXGraph& graph, size_t v_id) {
    auto v = graph[v_id];
    if (!v->is_z() && !v->is_x()) return;
    auto const old_neighbors = graph.get_neighbors(v);
    for (auto& [nb, etype] : old_neighbors) {
        graph.remove_edge(v, nb, etype);
    }
    for (auto& [nb, etype] : old_neighbors) {
        graph.add_edge(v, nb, toggle_edge(etype));
    }
    v->set_type(v->get_type() == VertexType::z ? VertexType::x : VertexType::z);
}

/**
 * @brief Add a Z-spider between two vertices. The edge type to the left vertex
 *        is specified and the edge type to the right vertex is automatically
 *        determined to keep the underlying mapping unchanged.
 *
 * @param graph
 * @param left_id
 * @param right_id
 * @param etype_to_left
 * @return std::optional<size_t> if the vertex ids are invalid or no edge exists
 *         between the two vertices, return std::nullopt. Otherwise, return the
 *         id of the added vertex.
 */
std::optional<size_t>
add_identity_vertex(
    ZXGraph& graph, size_t left_id, size_t right_id,
    VertexType vtype, EdgeType etype_to_left, std::optional<size_t> new_v_id) {
    auto const l = graph[left_id];
    auto const r = graph[right_id];

    if (l == nullptr || r == nullptr) return std::nullopt;
    if (new_v_id.has_value() && graph.is_v_id(new_v_id.value())) {
        return std::nullopt;
    }

    auto const etype_orig = graph.get_edge_type(left_id, right_id);
    if (!etype_orig.has_value()) return std::nullopt;

    // REVIEW - the row takes the row of the right vertex due to
    // backward compatibility. Might want to change this in the future
    auto const id_vtx = graph.add_vertex(
        new_v_id, vtype, Phase(0),
        r->get_row(), (l->get_col() + r->get_col()) / 2);
    graph.add_edge(l, id_vtx, etype_to_left);
    graph.add_edge(id_vtx, r, concat_edge(*etype_orig, etype_to_left));
    graph.remove_edge(l, r, *etype_orig);

    return id_vtx->get_id();
}

std::optional<std::tuple<size_t, size_t, VertexType, EdgeType>>
remove_identity_vertex(ZXGraph& graph, size_t v_id) {
    auto const v = graph[v_id];
    if (v == nullptr ||
        graph.get_num_neighbors(v) != 2 ||
        !(v->is_z() || v->is_x()) ||
        v->get_phase() != Phase(0)) {
        return std::nullopt;
    }

    auto const vtype = v->get_type();

    auto const [l, etype_to_l] = graph.get_first_neighbor(v);
    auto const [r, etype_to_r] = graph.get_second_neighbor(v);

    graph.add_edge(l, r, concat_edge(etype_to_l, etype_to_r));
    graph.remove_vertex(v);

    return std::make_tuple(l->get_id(), r->get_id(), vtype, etype_to_l);
}

/**
 * @brief transfer the phase of the specified vertex to a unary gadget.
 *        This function does nothing if the target vertex is not a Z-spider.
 *
 * @param v
 * @param keepPhase if specified, keep this amount of phase on the vertex
 *                  and only transfer the rest.
 */
void gadgetize_phase(ZXGraph& graph, size_t v_id, Phase const& keep_phase) {
    auto v = graph[v_id];
    if (!v->is_z()) return;
    ZXVertex* leaf = graph.add_vertex(
        VertexType::z, v->get_phase() - keep_phase, -2, v->get_col());
    ZXVertex* buffer = graph.add_vertex(
        VertexType::z, Phase(0), -1, v->get_col());
    v->set_phase(keep_phase);

    graph.add_edge(leaf, buffer, EdgeType::hadamard);
    graph.add_edge(buffer, v, EdgeType::hadamard);
}

// ZXGraphAction classes

IdentityRemoval::IdentityRemoval(size_t v_id) : _v_id(v_id) {}

bool IdentityRemoval::apply(ZXGraph& graph) {
    auto res = remove_identity_vertex(graph, _v_id);
    if (!res.has_value()) return false;
    std::tie(_left_id, _right_id, _vtype, _etype_to_left) = *std::move(res);
    return true;
}
bool IdentityRemoval::undo(ZXGraph& graph) {
    auto const res = add_identity_vertex(
        graph, _left_id, _right_id, _vtype, _etype_to_left, _v_id);
    return res.has_value();
}

IdentityAddition::IdentityAddition(
    size_t left_id, size_t right_id,
    VertexType vtype, EdgeType etype_to_left)
    : _left_id(left_id), _right_id(right_id),
      _vtype(vtype), _etype_to_left(etype_to_left) {}

bool IdentityAddition::apply(ZXGraph& graph) {
    auto res = add_identity_vertex(
        graph, _left_id, _right_id, _vtype, _etype_to_left);
    if (!res.has_value()) return false;
    _new_v_id = *res;
    return true;
}
bool IdentityAddition::undo(ZXGraph& graph) {
    return remove_identity_vertex(graph, _new_v_id).has_value();
}

}  // namespace qsyn::zx
