/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zxgraph.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <numeric>
#include <ranges>

#include "./zx_def.hpp"
#include "qsyn/qsyn_type.hpp"
#include "tl/enumerate.hpp"
#include "util/boolean_matrix.hpp"

namespace qsyn::zx {

/*****************************************************/
/*   class ZXGraph Getter and setter functions       */
/*****************************************************/

/**
 * @brief Construct a new ZXGraph object from a list of vertices.
 *
 * @param vertices the vertices
 * @param inputs the inputs. Note that the inputs must be a subset of the vertices.
 * @param outputs the outputs. Note that the outputs must be a subset of the vertices.
 * @param id
 */
ZXGraph::ZXGraph(ZXVertexList const& vertices,
                 ZXVertexList const& inputs,
                 ZXVertexList const& outputs) : _inputs{inputs}, _outputs{outputs}, _vertices{vertices} {
    for (auto v : _vertices) {
        v->set_id(_next_v_id);
        _id_to_vertices.emplace(_next_v_id, v);
        _next_v_id++;
    }
    for (auto v : _inputs) {
        assert(vertices.contains(v));
        _input_list[v->get_qubit()] = v;
    }
    for (auto v : _outputs) {
        assert(vertices.contains(v));
        _output_list[v->get_qubit()] = v;
    }
}

ZXGraph::ZXGraph(ZXGraph const& other) : _filename{other._filename}, _procedures{other._procedures} {
    std::unordered_map<ZXVertex*, ZXVertex*> old_to_new_vertex_map;

    for (auto& v : other._vertices) {
        if (v->is_boundary()) {
            if (other._inputs.contains(v)) {
                old_to_new_vertex_map[v] = this->add_input(v->get_qubit(), v->get_col());
            } else {
                old_to_new_vertex_map[v] = this->add_output(v->get_qubit(), v->get_col());
            }
        } else if (v->is_z() || v->is_x() || v->is_hbox()) {
            old_to_new_vertex_map[v] = this->add_vertex(v->get_type(), v->get_phase(), v->get_row(), v->get_col());
        }
    }

    other.for_each_edge([&old_to_new_vertex_map, this](EdgePair const& epair) {
        this->add_edge(old_to_new_vertex_map[epair.first.first], old_to_new_vertex_map[epair.first.second], epair.second);
    });
}

/**
 * @brief Get the number of edges in ZXGraph
 *
 * @return size_t
 */
size_t ZXGraph::get_num_edges() const {
    return std::accumulate(_vertices.begin(), _vertices.end(), 0, [this](size_t sum, ZXVertex* v) {
               return sum + this->get_num_neighbors(v);
           }) /
           2;
}

/*****************************************************/
/*   class ZXGraph Testing functions                 */
/*****************************************************/

/**
 * @brief Check if the ZXGraph is an empty one (no vertice)
 *
 * @return true
 * @return false
 */
bool ZXGraph::is_empty() const {
    return _vertices.empty();
}

/**
 * @brief Check if the ZXGraph is valid (i/o connected to 1 vertex, each neighbor matches)
 *
 * @return true
 * @return false
 */
bool ZXGraph::is_valid() const {
    for (auto& v : _inputs) {
        if (this->get_num_neighbors(v) != 1) {
            spdlog::debug("Error: input {} has {} neighbors, expected 1", v->get_id(), this->get_num_neighbors(v));
            return false;
        }
    }
    for (auto& v : _outputs) {
        if (this->get_num_neighbors(v) != 1) {
            spdlog::debug("Error: output {} has {} neighbors, expected 1", v->get_id(), this->get_num_neighbors(v));
            return false;
        }
    }
    for (auto& v : _vertices) {
        for (auto& [nb, etype] : this->get_neighbors(v)) {
            if (!this->is_neighbor(nb, v, etype)) return false;
        }
    }
    return true;
}

/**
 * @brief Check if `id` exists
 *
 * @param id
 * @return true
 * @return false
 */
bool ZXGraph::is_v_id(size_t id) const {
    return _id_to_vertices.contains(id);
}

/**
 * @brief Check if ZXGraph is graph-like, report first error
 *
 * @return true
 * @return false
 */
bool ZXGraph::is_graph_like() const {
    // all internal edges are hadamard edges
    for (auto const& v : _vertices) {
        if (!v->is_z() && !v->is_boundary()) {
            spdlog::debug("Note: vertex {} is of type {}", v->get_id(), v->get_type());
            return false;
        }
        for (auto const& [nb, etype] : this->get_neighbors(v)) {
            if (v->is_boundary() || nb->is_boundary()) continue;
            if (etype != EdgeType::hadamard) {
                spdlog::debug("Note: internal edge ({}, {}) is of type {}", v->get_id(), nb->get_id(), etype);
                return false;
            }
        }
    }

    // 4. Boundary vertices only has an edge

    return std::ranges::all_of(_inputs, [this](ZXVertex* i) {
               if (this->get_num_neighbors(i) != 1) {
                   spdlog::debug("Note: input {} has {} neighbors; expected 1", i->get_id(), this->get_num_neighbors(i));
                   return false;
               }
               return true;
           }) &&
           std::ranges::all_of(_outputs, [this](ZXVertex* o) {
               if (this->get_num_neighbors(o) != 1) {
                   spdlog::debug("Note: output {} has {} neighbors; expected 1", o->get_id(), this->get_num_neighbors(o));
                   return false;
               }
               return true;
           });
}

/**
 * @brief Check if ZXGraph is identity
 *
 * @return true
 * @return false
 */
bool ZXGraph::is_identity() const {
    return all_of(_inputs.begin(), _inputs.end(), [this](ZXVertex* i) {
        return (this->get_num_neighbors(i) == 1) &&
               _outputs.contains(this->get_first_neighbor(i).first) &&
               this->get_first_neighbor(i).first->get_qubit() == i->get_qubit();
    });
}

size_t ZXGraph::get_num_gadgets() const {
    return std::ranges::count_if(_vertices, [this](ZXVertex* v) {
        return this->is_gadget_leaf(v);
    });
}

/**
 * @brief Return the density of the ZXGraph
 *
 * @return double
 */
double ZXGraph::density() {
    return std::accumulate(this->get_vertices().begin(), this->get_vertices().end(), 0,
                           [this](double sum, ZXVertex* v) {
                               return sum + std::pow(this->get_num_neighbors(v), 2);
                           }) /
           gsl::narrow_cast<double>(this->get_num_vertices());
}

/*****************************************************/
/*   class ZXGraph Add functions                     */
/*****************************************************/

ZXVertex* ZXGraph::add_input(QubitIdType qubit, float col) {
    return add_input(qubit, static_cast<float>(qubit), col);
}

ZXVertex* ZXGraph::add_input(QubitIdType qubit, float row, float col) {
    assert(!is_input_qubit(qubit));

    auto v = new ZXVertex(_next_v_id, qubit, VertexType::boundary, Phase(), row, col);
    _inputs.emplace(v);
    _input_list.emplace(qubit, v);
    _vertices.emplace(v);
    _id_to_vertices.emplace(_next_v_id, v);
    _next_v_id++;
    return v;
}

ZXVertex* ZXGraph::add_output(QubitIdType qubit, float col) {
    return add_output(qubit, static_cast<float>(qubit), col);
}

ZXVertex* ZXGraph::add_output(QubitIdType qubit, float row, float col) {
    assert(!is_output_qubit(qubit));

    auto v = new ZXVertex(_next_v_id, qubit, VertexType::boundary, Phase(), row, col);
    _outputs.emplace(v);
    _output_list.emplace(qubit, v);
    _vertices.emplace(v);
    _id_to_vertices.emplace(_next_v_id, v);
    _next_v_id++;
    return v;
}

/**
 * @brief Add a vertex to the ZXGraph.
 *
 * @param qubit the qubit to the ZXVertex. In case of adding a boundary vertex, it is the user's responsibility to maintain non-overlapping input and output qubit IDs.
 * @param vt vertex type. In case of adding boundary vertex, it is the user's responsibility to maintain non-overlapping input and output qubit IDs.
 * @param phase the phase
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::add_vertex(VertexType vt, Phase phase, float row, float col) {
    return add_vertex(_next_v_id++, vt, phase, row, col);
}

ZXVertex* ZXGraph::add_vertex(size_t id, VertexType vt, Phase phase, float row, float col) {
    if (_id_to_vertices.contains(id)) {
        spdlog::warn("Vertex with id {} already exists", id);
        return nullptr;
    }
    auto v = new ZXVertex(id, 0, vt, phase, row, col);
    _vertices.emplace(v);
    _id_to_vertices.emplace(id, v);
    return v;
}

ZXVertex* ZXGraph::add_vertex(std::optional<size_t> id, VertexType vt, Phase phase, float row, float col) {
    if (id.has_value()) {
        return add_vertex(id.value(), vt, phase, row, col);
    }
    return add_vertex(vt, phase, row, col);
}

/**
 * @brief Add edge between `vs` and `vt` with edge `etype`
 *
 * @param vs
 * @param vt
 * @param et
 * @return EdgePair
 */
void ZXGraph::add_edge(ZXVertex* vs, ZXVertex* vt, EdgeType et) {
    if (vs == vt) {
        vs->set_phase(vs->get_phase() + (et == EdgeType::hadamard ? Phase(1) : Phase(0)));
        return;
    }

    if (vs->get_id() > vt->get_id()) std::swap(vs, vt);

    // if vs and vt are not neighbors, simply add the edge
    if (!this->is_neighbor(vs, vt)) {
        vs->_neighbors.emplace(vt, et);
        vt->_neighbors.emplace(vs, et);
        return;
    }

    // when vs and vt are neighbors, try to merge or cancel out the edge

    auto const is_zx = [](ZXVertex* v) { return v->is_z() || v->is_x(); };

    // If one or both vertices are not Z/X-spiders,
    // we can't merge or cancel out the edges in an meaningful way
    if (!is_zx(vs) || !is_zx(vt)) {
        throw std::logic_error(
            fmt::format(
                "Cannot add >1 between {}({}) and {}({})",
                vs->get_type(), vs->get_id(),
                vt->get_type(), vt->get_id()));
    }

    auto const existing_etype = this->get_edge_type(vs, vt).value();

    auto const same_type = vs->get_type() == vt->get_type();
    auto const to_merge  = same_type ? EdgeType::simple : EdgeType::hadamard;
    auto const to_cancel = same_type ? EdgeType::hadamard : EdgeType::simple;
    // Z and X vertices: merge or cancel out

    if (existing_etype == to_merge && et == to_merge) {
        // new edge can be merged with existing edge
        // do nothing
    } else if (existing_etype == to_cancel && et == to_cancel) {
        // new edge can be canceled out with existing edge
        this->remove_edge(vs, vt, to_cancel);
    } else {
        // one edge is to_merge and the other is to_cancel
        // keep the to_merge edge and turn the to_cancel edge into a pi phase
        if (existing_etype == to_cancel) {
            this->remove_edge(vs, vt, to_cancel);
            vs->_neighbors.emplace(vt, to_merge);
            vt->_neighbors.emplace(vs, to_merge);
        }

        vs->set_phase(vs->get_phase() + Phase(1));
    }
}

/**
 * @brief Move vertices from the other graph
 *
 * @param vertices
 */
void ZXGraph::_move_vertices_from(ZXGraph& other) {
    _vertices.insert(other._vertices.begin(), other._vertices.end());
    for (auto v : other._vertices) {
        v->set_id(_next_v_id);
        _id_to_vertices.emplace(_next_v_id, v);
        _next_v_id++;
    }

    other._vertices.clear();
    other._inputs.clear();
    other._outputs.clear();
    other._input_list.clear();
    other._output_list.clear();
    other._id_to_vertices.clear();
}

/*****************************************************/
/*   class ZXGraph Remove functions                  */
/*****************************************************/

/**
 * @brief Remove all vertices in the graph with no neighbor.
 *
 */
size_t ZXGraph::remove_isolated_vertices() {
    std::vector<ZXVertex*> rm_list;
    for (auto const& v : _vertices) {
        if (this->get_num_neighbors(v) == 0) rm_list.emplace_back(v);
    }
    return remove_vertices(rm_list);
}

/**
 * @brief Remove `v` in ZXGraph and itd incident edges
 *
 * @param v
 */
size_t ZXGraph::remove_vertex(ZXVertex* v) {
    if (!_vertices.contains(v)) return 0;

    auto v_neighbors = this->get_neighbors(v);
    for (auto const& n : v_neighbors) {
        v->_neighbors.erase(n);
        ZXVertex* const nv = n.first;
        EdgeType const ne  = n.second;
        nv->_neighbors.erase({v, ne});
    }
    _vertices.erase(v);
    _id_to_vertices.erase(v->get_id());

    // Check if also in _inputs or _outputs
    if (_inputs.contains(v)) {
        _input_list.erase(v->get_qubit());
        _inputs.erase(v);
    }
    if (_outputs.contains(v)) {
        _output_list.erase(v->get_qubit());
        _outputs.erase(v);
    }

    // deallocate ZXVertex
    delete v;
    return 1;
}

/**
 * @brief Remove all vertex in vertices by calling `removeVertex(ZXVertex* v)`
 *
 * @param vertices
 */
size_t ZXGraph::remove_vertices(std::vector<ZXVertex*> const& vertices) {
    return std::transform_reduce(
        vertices.begin(), vertices.end(), 0, std::plus{}, [this](ZXVertex* v) {
            return remove_vertex(v);
        });
}

/**
 * @brief Remove an edge exactly equal to `ep`.
 *
 * @param ep
 */
size_t ZXGraph::remove_edge(EdgePair const& ep) {
    return remove_edge(ep.first.first, ep.first.second, ep.second);
}

/**
 * @brief Remove an edge between `vs` and `vt`, with EdgeType `etype`
 *
 * @param vs
 * @param vt
 * @param etype
 */
size_t ZXGraph::remove_edge(ZXVertex* vs, ZXVertex* vt, EdgeType etype) {
    auto const count = vs->_neighbors.erase({vt, etype}) + vt->_neighbors.erase({vs, etype});
    if (count == 1) {
        throw std::out_of_range("Graph connection error in " + std::to_string(vs->get_id()) + " and " + std::to_string(vt->get_id()));
    }

    return count / 2;
}

/**
 * @brief Remove each ep in `eps` by calling `ZXGraph::remove_edge`
 *
 * @param eps
 * @return size_t
 */
size_t ZXGraph::remove_edges(std::span<EdgePair const> epairs) {
    return std::transform_reduce(
        std::begin(epairs), std::end(epairs), 0, std::plus{}, [this](EdgePair const& epair) {
            return remove_edge(epair);
        });
}

/**
 * @brief Remove all edges between `vs` and `vt` by pointer.
 *
 * @param vs
 * @param vt
 */
size_t ZXGraph::remove_all_edges_between(ZXVertex* vs, ZXVertex* vt) {
    return remove_edge(vs, vt, EdgeType::simple) + remove_edge(vs, vt, EdgeType::hadamard);
}

/*****************************************************/
/*   class ZXGraph Operation on graph functions.     */
/*****************************************************/

/**
 * @brief adjoint the zxgraph
 *
 */
void ZXGraph::adjoint() {
    std::swap(_inputs, _outputs);
    std::swap(_input_list, _output_list);
    auto max_col = std::ranges::max(_vertices | std::views::transform([](ZXVertex* v) { return v->get_col(); }));

    std::ranges::for_each(_vertices, [&max_col](ZXVertex* v) {
        v->set_phase(-v->get_phase());
        v->set_col(max_col - v->get_col());
    });
}

/**
 * @brief Assign rotation/value to the specified boundary
 *
 * @param qubit
 * @param isInput
 * @param ty
 * @param phase
 */
void ZXGraph::assign_vertex_to_boundary(QubitIdType qubit, bool is_input, VertexType vtype, Phase phase) {
    ZXVertex* v        = add_vertex(vtype, phase, gsl::narrow<float>(qubit));
    ZXVertex* boundary = is_input ? _input_list[qubit] : _output_list[qubit];
    for (auto& [nb, etype] : this->get_neighbors(boundary)) {
        add_edge(v, nb, etype);
    }
    remove_vertex(boundary);
}

/*****************************************************/
/*   class ZXGraph Find functions.                   */
/*****************************************************/

/**
 * @brief Find Vertex by vertex's id.
 *
 * @param id
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::vertex(size_t const& id) const {
    return is_v_id(id) ? _id_to_vertices.at(id) : nullptr;
}

dvlab::BooleanMatrix get_biadjacency_matrix(ZXGraph const& graph, ZXVertexList const& row_vertices, ZXVertexList const& col_vertices) {
    dvlab::BooleanMatrix matrix(row_vertices.size(), col_vertices.size());
    for (auto const& [i, v] : row_vertices | tl::views::enumerate) {
        for (auto const& [j, w] : col_vertices | tl::views::enumerate) {
            if (graph.is_neighbor(v, w)) matrix[i][j] = 1;
        }
    }
    return matrix;
}

}  // namespace qsyn::zx
