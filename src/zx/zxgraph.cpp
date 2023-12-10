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
    std::unordered_map<ZXVertex*, ZXVertex*> old_v2new_v_map;

    for (auto& v : other._vertices) {
        if (v->is_boundary()) {
            if (other._inputs.contains(v))
                old_v2new_v_map[v] = this->add_input(v->get_qubit(), v->get_col());
            else
                old_v2new_v_map[v] = this->add_output(v->get_qubit(), v->get_col());
        } else if (v->is_z() || v->is_x() || v->is_hbox()) {
            old_v2new_v_map[v] = this->add_vertex(v->get_qubit(), v->get_type(), v->get_phase(), v->get_col());
        }
    }

    other.for_each_edge([&old_v2new_v_map, this](EdgePair const& epair) {
        this->add_edge(old_v2new_v_map[epair.first.first], old_v2new_v_map[epair.first.second], epair.second);
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
    return std::ranges::any_of(_vertices, [id](ZXVertex* v) { return v->get_id() == id; });
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
    for (auto const& v : _inputs) {
        if (this->get_num_neighbors(v) != 1) {
            spdlog::debug("Note: boundary {} has {} neighbors; expected 1", v->get_id(), this->get_num_neighbors(v));
            return false;
        }
    }
    for (auto const& v : _outputs) {
        if (this->get_num_neighbors(v) != 1) {
            spdlog::debug("Note: boundary {} has {} neighbors; expected 1", v->get_id(), this->get_num_neighbors(v));
            return false;
        }
    }
    return true;
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
        return !v->is_boundary() && this->get_num_neighbors(v) == 1;
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

/**
 * @brief Add input to the ZXGraph
 *
 * @param qubit
 * @param col
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::add_input(QubitIdType qubit, ColumnIdType col) {
    assert(!is_input_qubit(qubit));

    ZXVertex* v = add_vertex(qubit, VertexType::boundary, Phase(), col);
    _inputs.emplace(v);
    _input_list.emplace(qubit, v);
    return v;
}

/**
 * @brief Add output to the ZXGraph
 *
 * @param qubit
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::add_output(QubitIdType qubit, ColumnIdType col) {
    assert(!is_output_qubit(qubit));

    ZXVertex* v = add_vertex(qubit, VertexType::boundary, Phase(), col);
    _outputs.emplace(v);
    _output_list.emplace(qubit, v);
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
ZXVertex* ZXGraph::add_vertex(QubitIdType qubit, VertexType vt, Phase phase, ColumnIdType col) {
    auto v = new ZXVertex(_next_v_id, qubit, vt, phase, col);
    _vertices.emplace(v);
    _next_v_id++;
    return v;
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

    // in case an edge already exists
    if (this->is_neighbor(vs, vt, et)) {
        // if vs or vt (or both) is H-box,
        // there isn't a way to merge or cancel out with the current edge
        // To circumvent this, we add a new vertex in the middle of the edge
        // and connect the new vertex to both vs and vt
        if (vs->is_hbox() || vt->is_hbox()) {
            ZXVertex* v = add_vertex(
                (vs->get_qubit() + vt->get_qubit()) / 2,
                et == EdgeType::hadamard ? VertexType::h_box : VertexType::z,
                et == EdgeType::hadamard ? Phase(1) : Phase(0),
                (vs->get_col() + vt->get_col()) / 2);
            vs->_neighbors.emplace(v, EdgeType::simple);
            v->_neighbors.emplace(vs, EdgeType::simple);
            vt->_neighbors.emplace(v, EdgeType::simple);
            v->_neighbors.emplace(vt, EdgeType::simple);

            return;
        }

        // Z and X vertices: merge or cancel out
        if (
            (vs->is_z() && vt->is_x() && et == EdgeType::simple) ||
            (vs->is_x() && vt->is_z() && et == EdgeType::simple) ||
            (vs->is_z() && vt->is_z() && et == EdgeType::hadamard) ||
            (vs->is_x() && vt->is_x() && et == EdgeType::hadamard)) {
            vs->_neighbors.erase({vt, et});
            vt->_neighbors.erase({vs, et});
        }  // else do nothing

        return;
    }

    vs->_neighbors.emplace(vt, et);
    vt->_neighbors.emplace(vs, et);

    return;
}

/**
 * @brief Move vertices from the other graph
 *
 * @param vertices
 */
void ZXGraph::_move_vertices_from(ZXGraph& other) {
    _vertices.insert(other._vertices.begin(), other._vertices.end());
    other.relabel_vertex_ids(_next_v_id);
    _next_v_id += other.get_num_vertices();

    other._vertices.clear();
    other._inputs.clear();
    other._outputs.clear();
    other._input_list.clear();
    other._output_list.clear();
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
    ZXVertex* v        = add_vertex(qubit, vtype, phase);
    ZXVertex* boundary = is_input ? _input_list[qubit] : _output_list[qubit];
    for (auto& [nb, etype] : this->get_neighbors(boundary)) {
        add_edge(v, nb, etype);
    }
    remove_vertex(boundary);
}

/**
 * @brief transfer the phase of the specified vertex to a unary gadget. This function does nothing
 *        if the target vertex is not a Z-spider.
 *
 * @param v
 * @param keepPhase if specified, keep this amount of phase on the vertex and only transfer the rest.
 */
void ZXGraph::gadgetize_phase(ZXVertex* v, Phase const& keep_phase) {
    if (!v->is_z()) return;
    ZXVertex* leaf   = this->add_vertex(-2, VertexType::z, v->get_phase() - keep_phase);
    ZXVertex* buffer = this->add_vertex(-1, VertexType::z, Phase(0));
    // REVIEW - No floating, directly take v
    leaf->set_col(v->get_col());
    buffer->set_col(v->get_col());
    v->set_phase(keep_phase);

    this->add_edge(leaf, buffer, EdgeType::hadamard);
    this->add_edge(buffer, v, EdgeType::hadamard);
}
/**
 * @brief Add a Z-spider to buffer a vertex from another vertex, so that they don't come in
 *        contact with each other on the edge with specified edge type. If such edge does not
 *        exists, this function does nothing.
 *
 * @param vertex_to_protect the vertex to protect
 * @param vertex_other the vertex to buffer from
 * @param etype the edgetype the buffer should be added on
 */
ZXVertex* ZXGraph::add_buffer(ZXVertex* vertex_to_protect, ZXVertex* vertex_other, EdgeType etype) {
    if (!this->is_neighbor(vertex_to_protect, vertex_other, etype)) return nullptr;

    ZXVertex* buffer_vertex = this->add_vertex(vertex_to_protect->get_qubit(), VertexType::z, Phase(0));

    this->add_edge(vertex_to_protect, buffer_vertex, toggle_edge(etype));
    this->add_edge(buffer_vertex, vertex_other, EdgeType::hadamard);
    this->remove_edge(vertex_to_protect, vertex_other, etype);
    // REVIEW - Float version
    buffer_vertex->set_col((vertex_to_protect->get_col() + vertex_other->get_col()) / 2);
    return buffer_vertex;
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
ZXVertex* ZXGraph::find_vertex_by_id(size_t const& id) const {
    auto it = std::ranges::find_if(_vertices, [id](ZXVertex* v) { return v->get_id() == id; });
    return it == _vertices.end() ? nullptr : *it;
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
