/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zx/zxgraph_action.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <cstddef>
#include <gsl/narrow>
#include <ranges>
#include <tl/zip.hpp>
#include <tuple>

#include "./zx_def.hpp"
#include "./zxgraph.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/util.hpp"

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
    if (this->num_outputs() != target.num_inputs()) {
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
        (*itr_ori)->type() = VertexType::z;
        (*itr_cop)->type() = VertexType::z;
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
    return v->is_z() &&
           this->num_neighbors(v) == 1 &&
           this->get_first_neighbor(v).first->is_z() &&
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
                                   return this->num_neighbors(nbp.first) == 1 && nbp.first->is_z() && nbp.second == EdgeType::hadamard;
                               });
}

bool ZXGraph::has_dangling_neighbors(ZXVertex* v) const {
    return std::ranges::any_of(this->get_neighbors(v),
                               [this](NeighborPair const& nbp) {
                                   return this->num_neighbors(nbp.first) == 1;
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
        if (vertices[i]->is_boundary() || vertices[i]->is_hbox()) return;
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

// free functions for editing ZXGraph

/**
 * @brief Toggle a vertex between type Z and X, and toggle the adjacent edges.
 *
 * @param v
 */
void toggle_vertex(ZXGraph& graph, size_t v_id) {
    auto v = graph[v_id];
    if (!v->is_zx()) return;
    auto const old_neighbors = graph.get_neighbors(v);
    for (auto& [nb, etype] : old_neighbors) {
        graph.remove_edge(v, nb, etype);
    }
    for (auto& [nb, etype] : old_neighbors) {
        graph.add_edge(v, nb, toggle_edge(etype));
    }
    v->type() = (v->type() == VertexType::z ? VertexType::x : VertexType::z);
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
        graph.num_neighbors(v) != 2 ||
        !v->is_zx() ||
        v->phase() != Phase(0)) {
        return std::nullopt;
    }

    auto const vtype = v->type();

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
        VertexType::z, v->phase() - keep_phase, -2, v->get_col());
    ZXVertex* buffer = graph.add_vertex(
        VertexType::z, Phase(0), -1, v->get_col());
    v->phase() = keep_phase;

    graph.add_edge(leaf, buffer, EdgeType::hadamard);
    graph.add_edge(buffer, v, EdgeType::hadamard);
}

// ZXGraphAction classes

IdentityRemoval::IdentityRemoval(size_t v_id) : _v_id(v_id) {}

/**
 * @brief Identity Removal is applicable if the vertex exists,
 *        is a Z/X-spider with zero phase and exactly two neighbors.
 *
 * @param graph
 * @return true
 * @return false
 */
bool IdentityRemoval::is_applicable(ZXGraph const& graph) const {
    auto const v = graph[_v_id];
    if (v == nullptr) return false;
    if (!v->is_zx()) return false;
    if (v->phase() != Phase(0)) return false;
    if (graph.num_neighbors(v) != 2) return false;
    return true;
}

/**
 * @brief Identity removal is undoable if the vertex id is not in use,
 *        the left and right vertices exist, and they are neighbors.
 *
 * @param graph
 * @return true
 * @return false
 */
bool IdentityRemoval::is_undoable(ZXGraph const& graph) const {
    auto const v = graph[_v_id];
    if (v != nullptr) return false;
    if (graph[_left_id] == nullptr) return false;
    if (graph[_right_id] == nullptr) return false;
    if (!graph.is_neighbor(_left_id, _right_id)) return false;
    return true;
}

void IdentityRemoval::apply_unchecked(ZXGraph& graph) const {
    std::tie(_left_id, _right_id, _vtype, _etype_to_left) =
        std::move(remove_identity_vertex(graph, _v_id).value());
}

void IdentityRemoval::undo_unchecked(ZXGraph& graph) const {
    auto const res = add_identity_vertex(
        graph, _left_id, _right_id, _vtype, _etype_to_left, _v_id);
    assert(res.value() == _v_id);
}

IdentityAddition::IdentityAddition(
    size_t left_id, size_t right_id,
    VertexType vtype, EdgeType etype_to_left)
    : _left_id(left_id), _right_id(right_id),
      _vtype(vtype), _etype_to_left(etype_to_left) {}

bool IdentityAddition::is_applicable(ZXGraph const& graph) const {
    auto const l = graph[_left_id];
    auto const r = graph[_right_id];
    if (l == nullptr || r == nullptr) return false;
    if (graph.is_neighbor(_left_id, _right_id)) return false;
    return true;
}

bool IdentityAddition::is_undoable(ZXGraph const& graph) const {
    auto const v = graph[_new_v_id];
    if (v == nullptr) return false;
    if (graph.num_neighbors(v) != 2) return false;
    return true;
}

void IdentityAddition::apply_unchecked(ZXGraph& graph) const {
    _new_v_id = add_identity_vertex(
                    graph, _left_id, _right_id, _vtype, _etype_to_left)
                    .value();
}

void IdentityAddition::undo_unchecked(ZXGraph& graph) const {
    auto const res = remove_identity_vertex(graph, _new_v_id);
    assert(res.has_value());
}

IdentityFusion::IdentityFusion(size_t v_id) : _v_id(v_id) {}

bool IdentityFusion::is_applicable(ZXGraph const& graph, size_t v_id) {
    auto v = graph[v_id];
    if (v == nullptr) return false;
    if (!v->is_z() || v->phase() != Phase(0)) return false;
    if (graph.num_neighbors(v) != 2) return false;
    auto const [l, etype_to_l] = graph.get_first_neighbor(v);
    auto const [r, etype_to_r] = graph.get_second_neighbor(v);
    return l->is_zx() && r->is_zx() &&
           l->type() == r->type() &&
           etype_to_l == etype_to_r;
}

bool IdentityFusion::is_applicable(ZXGraph const& graph) const {
    return is_applicable(graph, _v_id);
}

bool IdentityFusion::is_undoable(ZXGraph const& graph) const {
    auto l = graph[_left_id];
    if (l == nullptr) return false;
    if (graph[_v_id] != nullptr) return false;
    if (graph[_right_id] != nullptr) return false;

    return std::ranges::all_of(
        _right_neighbors,
        [&graph, this](size_t nb_id) {
            auto nb = graph[nb_id];
            return nb != nullptr || nb_id == _v_id;
        });
}

void IdentityFusion::apply_unchecked(ZXGraph& graph) const {
    auto v = graph[_v_id];

    auto const [l, etype_to_l] = graph.get_first_neighbor(v);
    auto const [r, etype_to_r] = graph.get_second_neighbor(v);

    assert(etype_to_l == EdgeType::hadamard || l->is_boundary());
    assert(etype_to_r == EdgeType::hadamard || r->is_boundary());

    _left_id     = l->get_id();
    _right_id    = r->get_id();
    _right_phase = r->phase();

    _right_neighbors.clear();
    _right_neighbors.reserve(graph.num_neighbors(r) - 1);

    for (auto const& [nb, etype] : graph.get_neighbors(r)) {
        if (nb == v) continue;
        _right_neighbors.emplace_back(nb->get_id());

        if (nb == l) {
            l->phase() += Phase(1);
            continue;
        }

        graph.add_edge(l, nb, etype);
    }

    graph.remove_vertex(v);
    graph.remove_vertex(r);

    l->phase() += r->phase();
}

void IdentityFusion::undo_unchecked(ZXGraph& graph) const {
    auto l = graph[_left_id];

    auto v = graph.add_vertex(
        _v_id, VertexType::z, Phase(0), l->get_row(), l->get_col());

    auto r = graph.add_vertex(
        _right_id, VertexType::z, _right_phase, l->get_row(), l->get_col() + 1);

    graph.add_edge(l, v, EdgeType::hadamard);
    graph.add_edge(v, r, EdgeType::hadamard);

    l->phase() -= r->phase();

    for (auto const& nb_id : _right_neighbors) {
        if (nb_id == _left_id) {
            l->phase() += Phase(1);
            graph.add_edge(l, r, EdgeType::hadamard);
        } else {
            auto nb    = graph[nb_id];
            auto etype = graph.get_edge_type(_left_id, nb_id);
            if (graph.is_neighbor(l, nb, *etype)) {
                graph.remove_edge(l, nb, *etype);
                graph.add_edge(r, nb, *etype);
            } else {
                graph.add_edge(l, nb, EdgeType::hadamard);
                graph.add_edge(r, nb, EdgeType::hadamard);
            }
        }
    }
}

BoundaryDetachment::BoundaryDetachment(size_t v_id) : _v_id(v_id) {}

/**
 * @brief The rule is applicable as long as the vertex exists.
 *
 * @param graph
 * @return true
 * @return false
 */
bool BoundaryDetachment::is_applicable(ZXGraph const& graph) const {
    return graph[_v_id] != nullptr;
}

/**
 * @brief The rule is undoable if identity-removing the buffered vertices gives
 *        the original graph. However, due to the full-check being too
 *        expensive, we only check that the buffered vertices are identities
 *        so that the underlying linear mapping does not change after the undo.
 *
 *
 * @param graph
 * @return true
 * @return false
 */
bool BoundaryDetachment::is_undoable(ZXGraph const& graph) const {
    if (graph[_v_id] == nullptr) {
        return false;
    }

    if (!_buffers.has_value()) {
        return false;
    }

    for (auto const& b_id : *_buffers) {
        auto b = graph[b_id];
        if (b == nullptr ||
            !b->is_zx() ||
            b->phase() != Phase(0) ||
            graph.num_neighbors(b) != 2) {
            return false;
        }
    }

    return true;
}

void BoundaryDetachment::apply_unchecked(ZXGraph& graph) const {
    auto v = graph[_v_id];

    if (!_boundaries.has_value()) {
        _boundaries =
            graph.get_neighbors(v) |
            std::views::keys |
            std::views::filter(&ZXVertex::is_boundary) |
            std::views::transform(&ZXVertex::get_id) |
            tl::to<std::vector>();  // copy to avoid dangling references
    }
    if (_buffers.has_value()) {
        for (auto const& [nb_id, b_id] :
             tl::views::zip(*_boundaries, *_buffers)) {
            add_identity_vertex(
                graph, _v_id, nb_id,
                VertexType::z, EdgeType::hadamard, b_id);
        }
    } else {
        _buffers = std::vector<size_t>();
        _buffers->reserve(_boundaries->size());

        for (auto const& nb_id : *_boundaries) {
            auto buffer = add_identity_vertex(
                graph, _v_id, nb_id,
                VertexType::z, EdgeType::hadamard);

            _buffers->emplace_back(buffer.value());
        }
    }
}

void BoundaryDetachment::undo_unchecked(ZXGraph& graph) const {
    for (auto const& b_id : *_buffers) {
        remove_identity_vertex(graph, b_id);
    }
}

// checks for LComp and Pivot
namespace {

/**
 * @brief Checks if the neighbors of the vertex is are suitable for lcomp/pivot
 *        applications.
 *
 * @param graph
 * @param v_id
 * @return true
 * @return false
 */
bool neighbors_applicable(ZXGraph const& graph,
                          size_t v_id) {
    auto const v = graph[v_id];
    if (v == nullptr) return false;
    return std::ranges::all_of(
        graph.get_neighbors(v),
        [&graph](auto nb_pair) {
            auto const [nb, etype] = nb_pair;
            return (nb->is_z() && etype == EdgeType::hadamard) ||
                   nb->is_boundary();
        });
}

/**
 * @brief Checks if the neighbors are suitable for lcomp/pivot undos.
 *
 * @param graph
 * @param neighbors
 * @return true
 * @return false
 */
bool neighbors_undoable(ZXGraph const& graph,
                        std::vector<size_t> const& neighbors) {
    return std::ranges::all_of(
        neighbors,
        [&graph](size_t nb_id) {
            auto const nb = graph[nb_id];
            return nb != nullptr && nb->is_z();
        });
}

/**
 * @brief Checks if the boundary detachment is applicable.
 *
 * @param graph
 * @param bd
 * @return true
 * @return false
 */
bool boundary_detachment_applicable(
    ZXGraph const& graph,
    std::optional<BoundaryDetachment> const& bd) {
    return !bd.has_value() || bd->is_applicable(graph);
}

/**
 * @brief Checks if the boundary detachment is undoable after undoing
 *        the lcomp/pivot rule.
 *        upon undo, all buffers would receive a phase addition of `_v_phase`
 *        and be disconnected from all neighbors except the boundary vertex.
 *        This means that, originally, all buffers should have been Z-spiders
 *        with Phase `phase` and `target_size` neighbors.
 *
 * @param graph
 * @param bd
 * @param phase
 * @param target_size
 * @return true
 * @return false
 */
bool boundary_detachment_undoable(
    ZXGraph const& graph,
    std::optional<BoundaryDetachment> const& bd,
    Phase const& phase,
    size_t target_size) {
    if (!bd.has_value()) return false;
    for (auto const& b_id : *bd->get_buffers()) {
        auto b = graph[b_id];
        if (b == nullptr ||
            !b->is_z() ||
            b->phase() != -phase ||
            graph.num_neighbors(b) != target_size) {
            return false;
        }
    }
    return true;
}

}  // namespace

LComp::LComp(size_t v_id) : _v_id(v_id) {}

/**
 * @brief Check if the LComp rule is applicable to the vertex, except that the
 *        phase of the vertex is not checked. This function is meant for
 *        composite that uses the LComp rule as a subrule.
 *
 * @param graph
 * @return true
 * @return false
 */
bool LComp::is_applicable_no_phase_check(ZXGraph const& graph) const {
    auto v = graph[_v_id];
    return v != nullptr &&
           v->is_z() &&
           boundary_detachment_applicable(graph, _bd) &&
           neighbors_applicable(graph, _v_id);
}

bool LComp::is_applicable(ZXGraph const& graph) const {
    return is_applicable_no_phase_check(graph) &&
           graph[_v_id]->phase().denominator() == 2;
}

bool LComp::is_undoable(ZXGraph const& graph) const {
    return graph[_v_id] == nullptr &&
           boundary_detachment_undoable(
               graph, _bd, _v_phase, _neighbors.size()) &&
           neighbors_undoable(graph, _neighbors);
}

void LComp::_complement_neighbors(ZXGraph& graph) const {
    for (auto const& [i, j] : dvlab::combinations<2>(_neighbors)) {
        graph.add_edge(i, j, EdgeType::hadamard);
    }
}

void LComp::apply_unchecked(ZXGraph& graph) const {
    auto v = graph[_v_id];

    if (!_bd.has_value()) {
        _bd = BoundaryDetachment(_v_id);
    }

    _bd->apply_unchecked(graph);

    _v_phase = v->phase();

    _neighbors.clear();
    _neighbors.reserve(graph.num_neighbors(v));

    for (auto const& [nb, etype] : graph.get_neighbors(v)) {
        _neighbors.emplace_back(nb->get_id());
        nb->phase() -= _v_phase;
    }

    // toggle all edges between neighbors
    _complement_neighbors(graph);

    graph.remove_vertex(v);
}

void LComp::undo_unchecked(ZXGraph& graph) const {
    auto v = graph.add_vertex(
        _v_id, VertexType::z, _v_phase, 0, 0);

    for (auto const& nb_id : _neighbors) {
        graph[nb_id]->phase() += _v_phase;
        graph.add_edge(v, graph[nb_id], EdgeType::hadamard);
    }

    _complement_neighbors(graph);

    _bd->undo_unchecked(graph);
}

Pivot::Pivot(size_t v1_id, size_t v2_id) : _v1_id(v1_id), _v2_id(v2_id) {}

bool Pivot::is_applicable_no_phase_check(ZXGraph const& graph) const {
    auto const v1 = graph[_v1_id];
    auto const v2 = graph[_v2_id];

    return v1 != nullptr &&
           v2 != nullptr &&
           v1->is_z() &&
           v2->is_z() &&
           graph.is_neighbor(v1, v2, EdgeType::hadamard) &&
           boundary_detachment_applicable(graph, _bd1) &&
           boundary_detachment_applicable(graph, _bd2) &&
           neighbors_applicable(graph, _v1_id) &&
           neighbors_applicable(graph, _v2_id);
}

bool Pivot::is_applicable(ZXGraph const& graph) const {
    return is_applicable_no_phase_check(graph) &&
           graph[_v1_id]->phase().denominator() == 1 &&
           graph[_v2_id]->phase().denominator() == 1;
}

bool Pivot::is_undoable(ZXGraph const& graph) const {
    return graph[_v1_id] == nullptr &&
           graph[_v2_id] == nullptr &&
           boundary_detachment_undoable(
               graph, _bd1, _v2_phase,
               _v2_neighbors.size() + _both_neighbors.size() + 1) &&
           boundary_detachment_undoable(
               graph, _bd2, _v1_phase,
               _v1_neighbors.size() + _both_neighbors.size() + 1) &&
           neighbors_undoable(graph, _v1_neighbors) &&
           neighbors_undoable(graph, _v2_neighbors) &&
           neighbors_undoable(graph, _both_neighbors);
}

void Pivot::_complement_neighbors(ZXGraph& graph) const {
    auto const add_edges_between = [&](std::vector<size_t> const& v1s,
                                       std::vector<size_t> const& v2s) {
        for (auto const& i : v1s) {
            for (auto const& j : v2s) {
                graph.add_edge(i, j, EdgeType::hadamard);
            }
        }
    };

    add_edges_between(_v1_neighbors, _v2_neighbors);
    add_edges_between(_v1_neighbors, _both_neighbors);
    add_edges_between(_v2_neighbors, _both_neighbors);
}

void Pivot::_adjust_phases(ZXGraph& graph) const {
    for (auto const& v_id : _v1_neighbors) {
        graph[v_id]->phase() += _v2_phase;
    }

    for (auto const& v_id : _v2_neighbors) {
        graph[v_id]->phase() += _v1_phase;
    }

    for (auto const& v_id : _both_neighbors) {
        graph[v_id]->phase() += _v1_phase + _v2_phase + Phase(1);
    }
}

void Pivot::apply_unchecked(ZXGraph& graph) const {
    if (!_bd1.has_value()) {
        _bd1 = BoundaryDetachment(_v1_id);
    }
    if (!_bd2.has_value()) {
        _bd2 = BoundaryDetachment(_v2_id);
    }
    _bd1->apply_unchecked(graph);
    _bd2->apply_unchecked(graph);

    _v1_phase = graph[_v1_id]->phase();
    _v2_phase = graph[_v2_id]->phase();

    _v1_neighbors.clear();
    _v2_neighbors.clear();
    _both_neighbors.clear();

    auto const get_neighbor_ids = [&](size_t v_id) {
        return graph.get_neighbors(graph[v_id]) |
               std::views::keys |
               std::views::transform(&ZXVertex::get_id) |
               tl::to<std::vector>();
    };

    auto m1_neighbors = get_neighbor_ids(_v1_id);
    auto m2_neighbors = get_neighbor_ids(_v2_id);

    std::erase(m1_neighbors, _v2_id);
    std::erase(m2_neighbors, _v1_id);

    std::ranges::sort(m1_neighbors);
    std::ranges::sort(m2_neighbors);
    std::set_intersection(
        m1_neighbors.begin(), m1_neighbors.end(),
        m2_neighbors.begin(), m2_neighbors.end(),
        std::back_inserter(_both_neighbors));

    std::ranges::sort(_both_neighbors);

    std::set_difference(
        m1_neighbors.begin(), m1_neighbors.end(),
        _both_neighbors.begin(), _both_neighbors.end(),
        std::back_inserter(_v1_neighbors));

    std::set_difference(
        m2_neighbors.begin(), m2_neighbors.end(),
        _both_neighbors.begin(), _both_neighbors.end(),
        std::back_inserter(_v2_neighbors));

    graph.remove_vertex(_v1_id);
    graph.remove_vertex(_v2_id);

    _adjust_phases(graph);
    _complement_neighbors(graph);
}

void Pivot::undo_unchecked(ZXGraph& graph) const {
    _complement_neighbors(graph);
    _adjust_phases(graph);

    auto const v1 = graph.add_vertex(
        _v1_id, VertexType::z, _v1_phase, 0, 0);

    auto const v2 = graph.add_vertex(
        _v2_id, VertexType::z, _v2_phase, 0, 1);

    graph.add_edge(v1, v2, EdgeType::hadamard);

    for (auto const& v_id : _v1_neighbors) {
        graph.add_edge(_v1_id, v_id, EdgeType::hadamard);
    }

    for (auto const& v_id : _v2_neighbors) {
        graph.add_edge(_v2_id, v_id, EdgeType::hadamard);
    }

    for (auto const& v_id : _both_neighbors) {
        graph.add_edge(_v1_id, v_id, EdgeType::hadamard);
        graph.add_edge(_v2_id, v_id, EdgeType::hadamard);
    }

    _bd1->undo_unchecked(graph);
    _bd2->undo_unchecked(graph);
}

NeighborUnfusion::NeighborUnfusion(size_t v_id,
                                   Phase phase_to_keep,
                                   std::vector<size_t> const& to_unfuse)
    : _v_id(v_id),
      _phase_to_keep(phase_to_keep),
      _neighbors_to_unfuse{to_unfuse} {}

/**
 * @brief all neighbors to be unfused should actually be neighbors of the vertex
 *
 * @param graph
 * @return true
 * @return false
 */
bool NeighborUnfusion::is_applicable(ZXGraph const& graph) const {
    auto v = graph[_v_id];
    if (v == nullptr) return false;
    if (!v->is_zx()) return false;

    return std::ranges::all_of(
        _neighbors_to_unfuse,
        [&graph, v](size_t nb_id) {
            auto nb = graph[nb_id];
            return nb != nullptr &&
                   graph.is_neighbor(v, nb);
        });
}

/**
 * @brief v and unfused vertices should be a candidate for identity fusion
 *
 * @param graph
 * @return true
 * @return false
 */
bool NeighborUnfusion::is_undoable(ZXGraph const& graph) const {
    if (_neighbors_to_unfuse.empty()) {
        return true;
    }

    if (!_buffer_v_id.has_value() || !_unfused_v_id.has_value()) {
        return false;
    }

    auto v       = graph[_v_id];
    auto buffer  = graph[*_buffer_v_id];
    auto unfused = graph[*_unfused_v_id];

    if (v == nullptr || buffer == nullptr || unfused == nullptr) {
        return false;
    }

    // check if the three vertices can be identity-fused
    if (v->type() != unfused->type() || !v->is_zx()) {
        return false;
    }
    return buffer->is_zx() &&
           buffer->phase() == Phase(0) &&
           graph.num_neighbors(buffer) == 2;
}

void NeighborUnfusion::apply_unchecked(ZXGraph& graph) const {
    auto v = graph[_v_id];

    auto unfused_v = graph.add_vertex(_unfused_v_id,
                                      v->type(),
                                      v->phase() - _phase_to_keep,
                                      -2);
    _unfused_v_id  = unfused_v->get_id();
    auto buffer    = graph.add_vertex(_buffer_v_id,
                                      VertexType::z,
                                      Phase(0),
                                      -1);
    _buffer_v_id   = buffer->get_id();
    v->phase()     = _phase_to_keep;

    graph.add_edge(*_buffer_v_id, *_unfused_v_id, EdgeType::hadamard);
    graph.add_edge(_v_id, *_buffer_v_id, EdgeType::hadamard);

    // move the neighbors to unfused to the _unfused_v_id

    for (auto const& nb_id : _neighbors_to_unfuse) {
        auto nb    = graph[nb_id];
        auto etype = graph.get_edge_type(_v_id, nb_id).value();
        graph.remove_edge(_v_id, nb_id, etype);
        graph.add_edge(*_unfused_v_id, nb_id, etype);
    }
}

void NeighborUnfusion::undo_unchecked(ZXGraph& graph) const {
    auto unfused = graph[*_unfused_v_id];
    for (auto const& [nb, etype] : graph.get_neighbors(unfused)) {
        auto const nb_id = nb->get_id();
        if (nb_id == _buffer_v_id) continue;
        graph.add_edge(_v_id, nb_id, etype);
    }

    graph[_v_id]->phase() += unfused->phase();

    graph.remove_vertex(*_buffer_v_id);
    graph.remove_vertex(*_unfused_v_id);
}

LCompUnfusion::LCompUnfusion(size_t v_id, std::vector<size_t> const& to_unfuse)
    : _nu{v_id, Phase(1, 2), to_unfuse}, _lcomp{v_id} {}

bool LCompUnfusion::_no_need_to_unfuse(ZXGraph const& graph) const {
    return _nu.get_neighbors_to_unfuse().empty() &&
           graph[_nu.get_v_id()]->phase().denominator() == 2;
}

bool LCompUnfusion::_no_need_to_undo_unfuse() const {
    return _nu.get_unfused_id() == std::nullopt ||
           _nu.get_buffer_id() == std::nullopt;
}

bool LCompUnfusion::is_applicable(ZXGraph const& graph) const {
    return _nu.is_applicable(graph) &&
           _lcomp.is_applicable_no_phase_check(graph);
}

bool LCompUnfusion::is_undoable(ZXGraph const& graph) const {
    if (!_lcomp.is_undoable(graph)) {
        return false;
    }
    // no unfusion actually takes place. This can happen if the target vertex
    // has a phase of pi/2 or -pi/2 and no neighbors to unfuse.
    if (_no_need_to_undo_unfuse()) {
        return true;
    }
    auto const unfused = graph[*_nu.get_unfused_id()];
    auto const buffer  = graph[*_nu.get_buffer_id()];

    if (unfused == nullptr || buffer == nullptr) {
        return false;
    }

    return buffer->phase() == Phase(-1, 2) &&
           buffer->is_z() &&
           graph.num_neighbors(buffer) == _lcomp.get_num_neighbors();
}

void LCompUnfusion::apply_unchecked(ZXGraph& graph) const {
    if (!_no_need_to_unfuse(graph)) {
        _nu.apply_unchecked(graph);
    }
    _lcomp.apply_unchecked(graph);
}

void LCompUnfusion::undo_unchecked(ZXGraph& graph) const {
    _lcomp.undo_unchecked(graph);
    if (!_no_need_to_undo_unfuse()) {
        _nu.undo_unchecked(graph);
    }
}

PivotUnfusion::PivotUnfusion(size_t v1_id, size_t v2_id,
                             std::vector<size_t> const& neighbors_to_unfuse_v1,
                             std::vector<size_t> const& neighbors_to_unfuse_v2)
    : _nu1{v1_id, Phase(0), neighbors_to_unfuse_v1},
      _nu2{v2_id, Phase(0), neighbors_to_unfuse_v2},
      _pivot{v1_id, v2_id} {}

bool PivotUnfusion::_no_need_to_unfuse(ZXGraph const& graph,
                                       NeighborUnfusion const& nu) const {
    return nu.get_neighbors_to_unfuse().empty() &&
           graph[nu.get_v_id()]->phase().denominator() == 1;
}

bool PivotUnfusion::_no_need_to_undo_unfuse(NeighborUnfusion const& nu) const {
    return nu.get_unfused_id() == std::nullopt ||
           nu.get_buffer_id() == std::nullopt;
}

bool PivotUnfusion::is_applicable(ZXGraph const& graph) const {
    return _nu1.is_applicable(graph) &&
           _nu2.is_applicable(graph) &&
           _pivot.is_applicable_no_phase_check(graph);
}

bool PivotUnfusion::is_undoable(ZXGraph const& graph) const {
    if (!_pivot.is_undoable(graph)) {
        return false;
    }

    auto const check_unfuse_conditions =
        [&](auto const& nu, auto const& num_neighbors) {
            if (_no_need_to_undo_unfuse(nu)) {
                return true;
            }

            auto const unfused = graph[*nu.get_unfused_id()];
            auto const buffer  = graph[*nu.get_buffer_id()];

            return unfused != nullptr && buffer != nullptr &&
                   buffer->phase() == Phase(0) && buffer->is_z() &&
                   graph.num_neighbors(buffer) == num_neighbors;
        };

    auto const num_neighbors_v1 =
        _pivot.get_num_v2_neighbors() + _pivot.get_num_both_neighbors() + 1;
    auto const num_neighbors_v2 =
        _pivot.get_num_v1_neighbors() + _pivot.get_num_both_neighbors() + 1;

    return check_unfuse_conditions(_nu1, num_neighbors_v1) &&
           check_unfuse_conditions(_nu2, num_neighbors_v2);
}

void PivotUnfusion::apply_unchecked(ZXGraph& graph) const {
    if (!_no_need_to_unfuse(graph, _nu1)) {
        _nu1.apply_unchecked(graph);
    }
    if (!_no_need_to_unfuse(graph, _nu2)) {
        _nu2.apply_unchecked(graph);
    }
    _pivot.apply_unchecked(graph);
}

void PivotUnfusion::undo_unchecked(ZXGraph& graph) const {
    _pivot.undo_unchecked(graph);
    if (!_no_need_to_undo_unfuse(_nu1)) {
        _nu1.undo_unchecked(graph);
    }
    if (!_no_need_to_undo_unfuse(_nu2)) {
        _nu2.undo_unchecked(graph);
    }
}

}  // namespace qsyn::zx
