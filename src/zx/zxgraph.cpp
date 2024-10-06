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
                 ZXVertexList const& outputs)
    : _inputs{inputs}, _outputs{outputs}, _vertices{vertices} {
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

ZXGraph::ZXGraph(ZXGraph const& other) : _filename{other._filename}, _procedures{other._procedures}, _next_v_id(other._next_v_id) {
    std::unordered_map<ZXVertex*, ZXVertex*> old_to_new_vertex_map;

    for (auto& v : other.get_vertices()) {
        if (v->is_boundary()) {
            if (other._inputs.contains(v)) {
                old_to_new_vertex_map[v] = this->add_input(v->get_id(), v->get_qubit(), v->get_row(), v->get_col());
            } else {
                old_to_new_vertex_map[v] = this->add_output(v->get_id(), v->get_qubit(), v->get_row(), v->get_col());
            }
        } else if (v->is_z() || v->is_x() || v->is_hbox()) {
            old_to_new_vertex_map[v] = this->add_vertex(v->get_id(), v->type(), v->phase(), v->get_row(), v->get_col());
        }
    }

    other.for_each_edge([&old_to_new_vertex_map, this](EdgePair const& epair) {
        this->add_edge(old_to_new_vertex_map[epair.first.first], old_to_new_vertex_map[epair.first.second], epair.second);
    });
}

/**
 * @brief Returns true if two ZXGraph have the same ID-vertex correspondences,
 *        qubit-IO correspondences, and connectivity between vertices.
 *        This comparison takes $O(|V| + |E|)$ time.
 *
 * @param other
 * @return true
 * @return false
 */
bool ZXGraph::operator==(ZXGraph const& other) const {
    // check if all ID-vertex correspondences are the same
    if (_id_to_vertices.size() != other._id_to_vertices.size()) {
        return false;
    }
    for (auto const& [id, v] : _id_to_vertices) {
        if (!other._id_to_vertices.contains(id) ||
            *v != *other._id_to_vertices.at(id)) {
            return false;
        }
    }

    // check if all qubit-IO correspondences are the same
    if (_input_list.size() != other._input_list.size() ||
        _output_list.size() != other._output_list.size()) {
        return false;
    }
    for (auto const& [q, v] : _input_list) {
        if (!other._input_list.contains(q) ||
            v->get_id() != other._input_list.at(q)->get_id()) {
            return false;
        }
    }
    for (auto const& [q, v] : _output_list) {
        if (!other._output_list.contains(q) ||
            v->get_id() != other._output_list.at(q)->get_id()) {
            return false;
        }
    }

    // check if connectivity between vertices are the same
    for (auto const& v : _vertices) {
        for (auto const& [nb, etype] : this->get_neighbors(v)) {
            if (!other.is_neighbor(v->get_id(), nb->get_id(), etype)) {
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Returns true if two ZXGraph have different ID-vertex correspondences,
 *        qubit-IO correspondences, or connectivity between vertices.
 *        The comparison takes $O(|V| + |E|)$ time.
 *
 * @param other
 * @return true
 * @return false
 */
bool ZXGraph::operator!=(ZXGraph const& other) const {
    return !(*this == other);
}

/**
 * @brief Get the number of edges in ZXGraph
 *
 * @return size_t
 */
size_t ZXGraph::num_edges() const {
    return std::accumulate(_vertices.begin(), _vertices.end(), 0, [this](size_t sum, ZXVertex* v) {
               return sum + this->num_neighbors(v);
           }) /
           2;
}

/**
 * @brief Get an unoccupied vertex ID. Note that this function may not return
 *        the smallest unoccupied ID. This function is not thread-safe.
 *
 * @return size_t
 */
size_t const& ZXGraph::_next_vertex_id() const {
    while (is_v_id(_next_v_id)) {
        _next_v_id++;
    }
    return _next_v_id;
}

/**
 * @brief Get an unoccupied vertex ID. Note that this function may not return
 *        the smallest unoccupied ID. This function is not thread-safe.
 *
 * @return size_t
 */
size_t& ZXGraph::_next_vertex_id() {
    while (is_v_id(_next_v_id)) {
        _next_v_id++;
    }
    return _next_v_id;
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
 * @brief Check if each input and output is connected to exactly one edge
 *
 * @param graph
 * @return true
 * @return false
 */
bool is_io_connection_valid(ZXGraph const& graph) {
    return std::ranges::all_of(graph.get_inputs(), [&](ZXVertex* i) {
               return graph.num_neighbors(i) == 1;
           }) &&
           std::ranges::all_of(graph.get_outputs(), [&](ZXVertex* o) {
               return graph.num_neighbors(o) == 1;
           });
}

/**
 * @brief Check if ZXGraph is graph-like, report first error
 *
 * @return true
 * @return false
 */
bool is_graph_like(ZXGraph const& graph) {
    // all internal edges are hadamard edges
    for (auto const& v : graph.get_vertices()) {
        if (!v->is_z() && !v->is_boundary()) {
            return false;
        }
    }

    for (auto const& v : graph.get_vertices()) {
        if (std::ranges::any_of(
                graph.get_neighbors(v),
                [&](auto const& neighbor) {
                    auto const& [nb, etype] = neighbor;
                    return !v->is_boundary() &&
                           !nb->is_boundary() &&
                           etype != EdgeType::hadamard;
                })) {
            return false;
        }
    }

    return is_io_connection_valid(graph);
}

/**
 * @brief check if the subgraph of `g` at `v_id` and its neighbors is graph-like
 *
 * @param g
 * @param v
 * @return true
 * @return false
 */
bool is_graph_like_at(ZXGraph const& g, size_t v_id) {
    auto v = g[v_id];
    if (v == nullptr) return false;
    // early return
    if (v->is_boundary()) {
        return g.num_neighbors(v) == 1;
    }

    // check v
    if (!v->is_z()) {
        return false;
    }

    // v's neighbors should either be boundary
    // or z-spider connected with hadamard edge
    if (!std::ranges::all_of(
            g.get_neighbors(v),
            [&](auto const& nb_pair) {
                auto const& [nb, etype] = nb_pair;
                return nb->is_boundary() && g.num_neighbors(nb) == 1 ||
                       nb->is_z() && etype == EdgeType::hadamard;
            })) {
        return false;
    }

    // any two non-boundary neighbors should not be connected by a simple edge

    auto const& neighbors = g.get_neighbors(v);

    for (auto it = neighbors.begin(); it != neighbors.end(); ++it) {
        for (auto jt = std::next(it); jt != neighbors.end(); ++jt) {
            if (!it->first->is_boundary() &&
                !jt->first->is_boundary() &&
                g.is_neighbor(it->first, jt->first, EdgeType::simple)) {
                return false;
            }
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
        return (this->num_neighbors(i) == 1) &&
               _outputs.contains(this->get_first_neighbor(i).first) &&
               this->get_first_neighbor(i).first->get_qubit() == i->get_qubit();
    });
}

size_t ZXGraph::num_gadgets() const {
    return std::ranges::count_if(_vertices, [this](ZXVertex* v) {
        return this->is_gadget_leaf(v);
    });
}

bool ZXGraph::is_neighbor(size_t v0_id, size_t v1_id) const {
    if (!is_v_id(v0_id) || !is_v_id(v1_id)) return false;
    return is_neighbor(vertex(v0_id), vertex(v1_id));
}
bool ZXGraph::is_neighbor(size_t v0_id, size_t v1_id, EdgeType et) const {
    if (!is_v_id(v0_id) || !is_v_id(v1_id)) return false;
    return is_neighbor(vertex(v0_id), vertex(v1_id), et);
}

/*****************************************************/
/*   class ZXGraph Add functions                     */
/*****************************************************/

ZXVertex* ZXGraph::add_input(QubitIdType qubit, float col) {
    return add_input(qubit, static_cast<float>(qubit), col);
}

ZXVertex* ZXGraph::add_input(QubitIdType qubit, float row, float col) {
    return add_input(_next_vertex_id()++, qubit, row, col);
}

ZXVertex* ZXGraph::add_input(size_t id, QubitIdType qubit, float row, float col) {
    if (_id_to_vertices.contains(id)) {
        spdlog::warn("Vertex with id {} already exists", id);
        return nullptr;
    }
    if (is_input_qubit(qubit)) {
        spdlog::warn("Input qubit {} already exists", qubit);
        return nullptr;
    }
    auto v = new ZXVertex(id, qubit, VertexType::boundary, Phase(), row, col);
    _inputs.emplace(v);
    _input_list.emplace(qubit, v);
    _vertices.emplace(v);
    _id_to_vertices.emplace(id, v);
    return v;
}

ZXVertex* ZXGraph::add_output(QubitIdType qubit, float col) {
    return add_output(qubit, static_cast<float>(qubit), col);
}

ZXVertex* ZXGraph::add_output(QubitIdType qubit, float row, float col) {
    return add_output(_next_vertex_id()++, qubit, row, col);
}

ZXVertex* ZXGraph::add_output(size_t id, QubitIdType qubit, float row, float col) {
    if (_id_to_vertices.contains(id)) {
        spdlog::warn("Vertex with id {} already exists", id);
        return nullptr;
    }
    if (is_output_qubit(qubit)) {
        spdlog::warn("Output qubit {} already exists", qubit);
        return nullptr;
    }
    auto v = new ZXVertex(id, qubit, VertexType::boundary, Phase(), row, col);
    _outputs.emplace(v);
    _output_list.emplace(qubit, v);
    _vertices.emplace(v);
    _id_to_vertices.emplace(id, v);
    return v;
}

/**
 * @brief Add a vertex to the ZXGraph.
 *
 * @param qubit the qubit to the ZXVertex. In case of adding a boundary vertex,
 *        it is the user's responsibility to maintain non-overlapping input and
 *        output qubit IDs.
 * @param vt vertex type. In case of adding boundary vertex, it is the user's
 *        responsibility to maintain non-overlapping input and output qubit IDs.
 * @param phase the phase
 * @return ZXVertex*
 */
ZXVertex*
ZXGraph::add_vertex(VertexType vt, Phase phase, float row, float col) {
    return add_vertex(_next_vertex_id()++, vt, phase, row, col);
}

ZXVertex*
ZXGraph::add_vertex(
    size_t id, VertexType vt, Phase phase, float row, float col) {
    if (_id_to_vertices.contains(id)) {
        spdlog::warn("Vertex with id {} already exists", id);
        return nullptr;
    }
    auto v = new ZXVertex(id, 0, vt, phase, row, col);
    _vertices.emplace(v);
    _id_to_vertices.emplace(id, v);
    return v;
}

ZXVertex*
ZXGraph::add_vertex(std::optional<size_t> id,
                    VertexType vt,
                    Phase phase,
                    float row,
                    float col) {
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
        if (!vs->is_zx()) {
            throw std::logic_error(
                "Cannot add an edge between a boundary vertex and itself");
        }
        vs->phase() += (et == EdgeType::hadamard ? Phase(1) : Phase(0));
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

    // If one or both vertices are not Z/X-spiders,
    // we can't merge or cancel out the edges in an meaningful way
    if (!vs->is_zx() || !vt->is_zx()) {
        throw std::logic_error(
            fmt::format(
                "Cannot add >1 between {}({}) and {}({})",
                vs->type(), vs->get_id(),
                vt->type(), vt->get_id()));
    }

    auto const existing_etype = this->get_edge_type(vs, vt).value();

    auto const same_type = vs->type() == vt->type();
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

        vs->phase() += Phase(1);
    }
}

void ZXGraph::add_edge(size_t v0_id, size_t v1_id, EdgeType et) {
    if (!is_v_id(v0_id)) {
        spdlog::warn("Vertex with id {} does not exist", v0_id);
        return;
    }
    if (!is_v_id(v1_id)) {
        spdlog::warn("Vertex with id {} does not exist", v1_id);
        return;
    }
    add_edge(vertex(v0_id), vertex(v1_id), et);
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
        if (this->num_neighbors(v) == 0) rm_list.emplace_back(v);
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

size_t ZXGraph::remove_vertex(size_t id) {
    if (!is_v_id(id)) {
        spdlog::warn("Vertex with id {} does not exist", id);
        return 0;
    }
    return remove_vertex(vertex(id));
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

size_t ZXGraph::remove_edge(size_t v0_id, size_t v1_id, EdgeType et) {
    if (!is_v_id(v0_id)) {
        spdlog::warn("Vertex with id {} does not exist", v0_id);
        return 0;
    }
    if (!is_v_id(v1_id)) {
        spdlog::warn("Vertex with id {} does not exist", v1_id);
        return 0;
    }
    return remove_edge(vertex(v0_id), vertex(v1_id), et);
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
size_t ZXGraph::remove_edge(ZXVertex* vs, ZXVertex* vt) {
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
        v->phase() *= -1;
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

// free functions that compute graph properties'

/**
 * @brief Return the density of the ZXGraph
 *
 * @return double
 */
double density(ZXGraph const& graph) {
    return std::transform_reduce(
               graph.get_vertices().begin(), graph.get_vertices().end(),
               0.0,
               std::plus<>(),
               [&](ZXVertex* v) {
                   return std::pow(graph.num_neighbors(v), 2);
               }) /
           gsl::narrow_cast<double>(graph.num_vertices());
}

size_t t_count(ZXGraph const& graph) {
    return std::ranges::count_if(
        graph.get_vertices(),
        [](ZXVertex* v) { return (v->phase().denominator() == 4); });
}
size_t non_clifford_count(ZXGraph const& graph) {
    return std::ranges::count_if(
        graph.get_vertices(),
        [](ZXVertex* v) { return !v->is_clifford(); });
}
size_t non_clifford_t_count(ZXGraph const& graph) {
    return non_clifford_count(graph) - t_count(graph);
}

}  // namespace qsyn::zx
