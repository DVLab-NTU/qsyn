/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph structures ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <filesystem>
#include <iterator>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

#include "./zx_def.hpp"
#include "qsyn/qsyn_type.hpp"
#include "spdlog/common.h"
#include "util/boolean_matrix.hpp"

namespace qsyn::zx {

class ZXVertex;
class ZXGraph;

// See `zxVertex.cpp` for details
std::optional<EdgeType> str_to_edge_type(std::string const& str);
std::optional<VertexType> str_to_vertex_type(std::string const& str);
EdgeType toggle_edge(EdgeType const& et);

inline EdgeType concat_edge(EdgeType const& etype) { return etype; }

inline EdgeType concat_edge(
    EdgeType const& etype, std::convertible_to<EdgeType> auto... others) {
    return (etype == EdgeType::hadamard) ^
                   (concat_edge(others...) == EdgeType::hadamard)
               ? EdgeType::hadamard
               : EdgeType::simple;
}

EdgePair make_edge_pair(ZXVertex* v1, ZXVertex* v2, EdgeType et);
EdgePair make_edge_pair(EdgePair epair);
EdgePair make_edge_pair_dummy();

class ZXVertex {
    friend class ZXGraph;

public:
    ZXVertex(size_t id,
             QubitIdType qubit,
             VertexType vt,
             Phase phase,
             float row,
             float col)
        : _attrs{id, vt, qubit, phase, row, col} {}
    // Getter and Setter

    size_t get_id() const { return _attrs.id; }
    QubitIdType get_qubit() const { return _attrs.qubit; }
    float get_row() const { return _attrs.row; }
    float get_col() const { return _attrs.col; }

    void set_id(size_t id) { _attrs.id = id; }
    void set_qubit(QubitIdType q) { _attrs.qubit = q; }
    void set_row(float r) { _attrs.row = r; }
    void set_col(float c) { _attrs.col = c; }

    auto const& type() const { return _attrs.type; }
    auto& type() { return _attrs.type; }
    auto const& phase() const { return _attrs.phase; }
    auto& phase() { return _attrs.phase; }

    // Print functions
    void print_vertex(
        spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;

    // Test
    bool is_z() const { return type() == VertexType::z; }
    bool is_x() const { return type() == VertexType::x; }
    bool is_zx() const { return is_z() || is_x(); }
    bool is_hbox() const { return type() == VertexType::h_box; }
    bool is_boundary() const { return type() == VertexType::boundary; }

    bool has_n_pi_phase() const { return _attrs.phase.denominator() == 1; }
    bool is_clifford() const { return _attrs.phase.denominator() <= 2; }

    // Comparison functions. Note that the comparison functions only compares
    // vertex type and phases. Graph attributes such as row, col and qubit are
    // not compared.

    bool operator==(ZXVertex const& other) const;
    bool operator!=(ZXVertex const& other) const;

private:
    friend class ZXGraph;
    struct ZXVertexAttrs {
        ZXVertexAttrs(size_t id, VertexType type, QubitIdType qubit,
                      Phase phase, float row, float col)
            : id(id), type(type), qubit(qubit),
              phase(phase), row(row), col(col) {}
        size_t id;
        VertexType type;
        QubitIdType qubit;  // for boundary vertices, this is the qubit id;
                            // for non-boundary vertices, this is a dummy value
                            // that may be used to mark temporary information
        Phase phase;
        float row;
        float col;
    } _attrs;
    Neighbors _neighbors;
};

class ZXGraph {  // NOLINT(cppcoreguidelines-special-member-functions)
                 // : copy-swap idiom
public:
    ZXGraph() = default;

    ZXGraph(ZXVertexList const& vertices,
            ZXVertexList const& inputs,
            ZXVertexList const& outputs);

    ~ZXGraph() {
        for (auto const& v : _vertices) {
            delete v;
        }
    }

    ZXGraph(ZXGraph const& other);

    ZXGraph(ZXGraph&& other) noexcept = default;

    ZXGraph& operator=(ZXGraph copy) {
        copy.swap(*this);
        return *this;
    }

    void release() {
        _next_v_id = 0;
        _filename  = "";
        _procedures.clear();
        _inputs.clear();
        _outputs.clear();
        _vertices.clear();
        _input_list.clear();
        _output_list.clear();
        _id_to_vertices.clear();
    }

    void swap(ZXGraph& other) noexcept {
        std::swap(_next_v_id, other._next_v_id);
        std::swap(_filename, other._filename);
        std::swap(_procedures, other._procedures);
        std::swap(_inputs, other._inputs);
        std::swap(_outputs, other._outputs);
        std::swap(_vertices, other._vertices);
        std::swap(_input_list, other._input_list);
        std::swap(_output_list, other._output_list);
        std::swap(_id_to_vertices, other._id_to_vertices);
    }

    friend void swap(ZXGraph& a, ZXGraph& b) noexcept {
        a.swap(b);
    }

    // Comparison functions. two ZXGraphs are deemed equal if their ID-vertex
    // correspondences, qubit-IO correspondences, and connectivity between
    // vertices are the same.

    bool operator==(ZXGraph const& other) const;
    bool operator!=(ZXGraph const& other) const;

    // Getter and Setter
    ZXVertexList const& get_inputs() const { return _inputs; }
    ZXVertexList const& get_outputs() const { return _outputs; }
    ZXVertexList const& get_vertices() const { return _vertices; }

    Neighbors const& get_neighbors(ZXVertex* v) const { return v->_neighbors; }
    NeighborPair const& get_first_neighbor(ZXVertex* v) const {
        return *(std::begin(v->_neighbors));
    }
    NeighborPair const& get_second_neighbor(ZXVertex* v) const {
        return *(std::next(std::begin(v->_neighbors)));
    }

    std::vector<ZXVertex*> get_copied_neighbors(ZXVertex* v) const;
    std::vector<size_t> get_neighbor_ids(ZXVertex* v) const;

    size_t num_edges() const;
    size_t num_inputs() const { return get_inputs().size(); }
    size_t num_outputs() const { return get_outputs().size(); }
    size_t num_vertices() const { return get_vertices().size(); }
    size_t num_neighbors(ZXVertex* v) const { return v->_neighbors.size(); }

    // file and procedure related functions
    // may be moved to manager class in the future
    void set_filename(std::string const& f) { _filename = f; }
    void add_procedures(std::vector<std::string> const& ps) {
        _procedures.insert(std::end(_procedures), std::begin(ps), std::end(ps));
    }
    void add_procedure(std::string_view p) { _procedures.emplace_back(p); }

    std::string get_filename() const { return _filename; }
    std::vector<std::string> const& get_procedures() const {
        return _procedures;
    }

    // attributes
    bool is_neighbor(ZXVertex* v1, ZXVertex* v2) const {
        return v1->_neighbors.contains({v2, EdgeType::simple}) ||
               v1->_neighbors.contains({v2, EdgeType::hadamard});
    }
    bool is_neighbor(ZXVertex* v1, ZXVertex* v2, EdgeType et) const {
        return v1->_neighbors.contains({v2, et});
    }
    bool is_neighbor(size_t v0_id, size_t v1_id) const;
    bool is_neighbor(size_t v0_id, size_t v1_id, EdgeType et) const;
    std::optional<EdgeType> get_edge_type(ZXVertex* v1, ZXVertex* v2) const {
        if (is_neighbor(v1, v2, EdgeType::simple)) return EdgeType::simple;
        if (is_neighbor(v1, v2, EdgeType::hadamard)) return EdgeType::hadamard;
        return std::nullopt;
    }
    std::optional<EdgeType> get_edge_type(size_t v1_id, size_t v2_id) const {
        return get_edge_type(vertex(v1_id), vertex(v2_id));
    }

    bool is_empty() const;
    bool is_v_id(size_t id) const;
    bool is_identity() const;
    size_t num_gadgets() const;
    bool is_input_qubit(QubitIdType qubit) const {
        return (_input_list.contains(qubit));
    }
    bool is_output_qubit(QubitIdType qubit) const {
        return (_output_list.contains(qubit));
    }

    bool is_gadget_leaf(ZXVertex* v) const;
    bool is_gadget_axel(ZXVertex* v) const;
    bool has_dangling_neighbors(ZXVertex* v) const;

    // Vertex addition

    ZXVertex* add_input(QubitIdType qubit, float col = 0.f);
    ZXVertex* add_input(QubitIdType qubit, float row, float col);
    ZXVertex* add_output(QubitIdType qubit, float col = 0.f);
    ZXVertex* add_output(QubitIdType qubit, float row, float col);
    ZXVertex* add_vertex(
        VertexType vt, Phase phase = Phase(), float row = 0.f, float col = 0.f);

    // Add vertices with specified IDs. It is generally advised to use
    // the above functions if the ID is not important.

    ZXVertex* add_input(
        size_t id, QubitIdType qubit, float row, float col);
    ZXVertex* add_output(
        size_t id, QubitIdType qubit, float row, float col);
    ZXVertex* add_vertex(
        size_t id, VertexType vt,
        Phase phase = Phase(), float row = 0.f, float col = 0.f);
    ZXVertex* add_vertex(
        std::optional<size_t> id, VertexType vt,
        Phase phase = Phase(), float row = 0.f, float col = 0.f);

    void add_edge(ZXVertex* vs, ZXVertex* vt, EdgeType et);
    void add_edge(size_t v0_id, size_t v1_id, EdgeType et);

    size_t remove_isolated_vertices();
    size_t remove_vertex(ZXVertex* v);
    size_t remove_vertex(size_t id);

    size_t remove_vertices(std::vector<ZXVertex*> const& vertices);
    size_t remove_vertices(std::vector<size_t> const& ids);
    size_t remove_edge(EdgePair const& ep);
    size_t remove_edge(ZXVertex* vs, ZXVertex* vt);
    size_t remove_edge(ZXVertex* vs, ZXVertex* vt, EdgeType etype);
    size_t remove_edge(size_t v0_id, size_t v1_id, EdgeType etype);
    size_t remove_edges(std::span<EdgePair const> epairs);

    // Operation on graph
    void adjoint();
    void assign_vertex_to_boundary(
        QubitIdType qubit, bool is_input, VertexType vtype, Phase phase);

    // Find functions
    ZXVertex* vertex(size_t const& id) const;
    auto operator[](size_t const& id) const { return vertex(id); }

    // Action functions (zxGraphAction.cpp)
    void sort_io_by_qubit();
    void lift_qubit(ssize_t n);

    ZXGraph& compose(ZXGraph const& target);
    ZXGraph& tensor_product(ZXGraph const& target);
    void add_gadget(Phase p, std::vector<ZXVertex*> const& vertices);
    void remove_gadget(ZXVertex* v);
    std::unordered_map<size_t, ZXVertex*> create_id_to_vertex_map() const;
    void adjust_vertex_coordinates();

    // Print functions (zxGraphPrint.cpp)
    void print_graph(
        spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;
    void print_inputs() const;
    void print_outputs() const;
    void print_gadgets() const;
    void print_io() const;
    void print_vertices(std::vector<size_t> cand) const;
    void print_vertices(
        spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;
    void print_vertices_by_rows(
        spdlog::level::level_enum lvl  = spdlog::level::level_enum::off,
        std::vector<float> const& cand = {}) const;
    void print_edges() const;

    // For mapping (in zxMapping.cpp)
    ZXVertexList get_non_boundary_vertices() const;
    ZXVertex* get_input_by_qubit(size_t const& q);
    ZXVertex* get_output_by_qubit(size_t const& q);
    void concatenate(ZXGraph other, std::vector<size_t> const& qubits);
    std::unordered_map<size_t, ZXVertex*> const&
    get_input_list() const { return _input_list; }
    std::unordered_map<size_t, ZXVertex*> const&
    get_output_list() const { return _output_list; }

    // I/O (in zxIO.cpp)
    bool write_zx(
        std::filesystem::path const& filename, bool complete = false) const;
    bool write_json(std::filesystem::path const& filename) const;
    bool write_tikz(std::string const& filename) const;
    bool write_tikz(std::ostream& os) const;
    bool write_pdf(std::string const& filename) const;
    bool write_tex(std::string const& filename) const;
    bool write_tex(std::ostream& os) const;

    // Traverse (in zxTraverse.cpp)
    std::vector<ZXVertex*> create_topological_order() const;
    std::vector<ZXVertex*> create_breadth_level() const;
    template <typename F>
    void topological_traverse(F lambda) {
        std::ranges::for_each(create_topological_order(), lambda);
    }
    template <typename F>
    void topological_traverse(F lambda) const {
        std::ranges::for_each(create_topological_order(), lambda);
    }
    template <typename F>
    void for_each_edge(F lambda) const {
        for (auto const& v : get_vertices()) {
            for (auto const& [nb, etype] : this->get_neighbors(v)) {
                if (nb->get_id() > v->get_id())
                    lambda(make_edge_pair(v, nb, etype));
            }
        }
    }

    template <typename F>
    void for_each_edge(ZXVertexList const& vertices, F lambda) const {
        for (auto const& v : vertices) {
            for (auto const& [nb, etype] : this->get_neighbors(v)) {
                if (nb->get_id() > v->get_id() && vertices.contains(nb))
                    lambda(make_edge_pair(v, nb, etype));
            }
        }
    }

    // divide into subgraphs and merge (in zxPartition.cpp)
    static std::pair<std::vector<ZXGraph*>, std::vector<ZXCut>>
    create_subgraphs(
        ZXGraph g,
        std::vector<ZXVertexList> partitions);
    static ZXGraph from_subgraphs(
        std::vector<ZXGraph*> const& subgraphs,
        std::vector<ZXCut> const& cuts);

private:
    mutable size_t _next_v_id = 0;
    std::string _filename;
    std::vector<std::string> _procedures;
    ZXVertexList _inputs;
    ZXVertexList _outputs;
    ZXVertexList _vertices;
    std::unordered_map<size_t, ZXVertex*> _input_list;
    std::unordered_map<size_t, ZXVertex*> _output_list;
    std::unordered_map<size_t, ZXVertex*> _id_to_vertices;

    void _dfs(
        std::unordered_set<ZXVertex*>& visited_vertices,
        std::vector<ZXVertex*>& topological_order,
        ZXVertex* v) const;
    void _bfs(
        std::unordered_set<ZXVertex*>& visited_vertices,
        std::vector<ZXVertex*>& topological_order,
        ZXVertex* v) const;

    size_t const& _next_vertex_id() const;
    size_t& _next_vertex_id();

    void _move_vertices_from(ZXGraph& other);
};

bool is_io_connection_valid(ZXGraph const& graph);
bool is_graph_like(ZXGraph const& graph);
bool is_graph_like_at(ZXGraph const& graph, size_t v_id);
bool is_interiorly_graph_like_at(ZXGraph const& graph, size_t v_id);

double density(ZXGraph const& graph);
size_t t_count(ZXGraph const& graph);
size_t non_clifford_count(ZXGraph const& graph);
size_t non_clifford_t_count(ZXGraph const& graph);

std::vector<size_t> closed_neighborhood(ZXGraph const& graph,
                                        std::vector<size_t> const& vertices,
                                        size_t level = 1);

std::vector<size_t> get_isolated_vertices(ZXGraph const& graph);

dvlab::BooleanMatrix get_biadjacency_matrix(
    ZXGraph const& graph,
    ZXVertexList const& row_vertices,
    ZXVertexList const& col_vertices);

}  // namespace qsyn::zx
