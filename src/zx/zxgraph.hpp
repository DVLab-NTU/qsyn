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
#include <limits>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

#include "./zx_def.hpp"
#include "qsyn/qsyn_type.hpp"
#include "spdlog/common.h"
#include "util/bit_matrix/bit_matrix.hpp"
#include "util/phase.hpp"

namespace qsyn::zx {

class ZXVertex;
class ZXGraph;

// See `zxVertex.cpp` for details
std::optional<EdgeType> str_to_edge_type(std::string const& str);
std::optional<VertexType> str_to_vertex_type(std::string const& str);
EdgeType toggle_edge(EdgeType const& et);

inline EdgeType concat_edge(EdgeType const& etype) { return etype; }

inline EdgeType concat_edge(EdgeType const& etype, std::convertible_to<EdgeType> auto... others) {
    return (etype == EdgeType::hadamard) ^ (concat_edge(others...) == EdgeType::hadamard) ? EdgeType::hadamard : EdgeType::simple;
}

EdgePair make_edge_pair(ZXVertex* v1, ZXVertex* v2, EdgeType et);
EdgePair make_edge_pair(EdgePair epair);
EdgePair make_edge_pair_dummy();

class ZXVertex {
    friend class ZXGraph;

public:
    using QubitIdType = qsyn::QubitIdType;

    ZXVertex(size_t id, QubitIdType qubit, VertexType vt, Phase phase, float row, float col)
        : _id{id}, _type{vt}, _qubit{qubit}, _phase{phase}, _row{row}, _col{col} {}
    // Getter and Setter

    size_t get_id() const { return _id; }
    QubitIdType get_qubit() const { return _qubit; }
    Phase const& get_phase() const { return _phase; }
    VertexType get_type() const { return _type; }
    float get_row() const { return _row; }
    float get_col() const { return _col; }

    void set_id(size_t id) { _id = id; }
    void set_qubit(QubitIdType q) { _qubit = q; }
    void set_phase(Phase const& p) { _phase = p; }
    void set_row(float r) { _row = r; }
    void set_col(float c) { _col = c; }
    void set_type(VertexType vt) { _type = vt; }

    // Print functions
    void print_vertex(spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;

    // Test
    bool is_z() const { return get_type() == VertexType::z; }
    bool is_x() const { return get_type() == VertexType::x; }
    bool is_hbox() const { return get_type() == VertexType::h_box; }
    bool is_boundary() const { return get_type() == VertexType::boundary; }

    bool has_n_pi_phase() const { return _phase.denominator() == 1; }
    bool is_clifford() const { return _phase.denominator() <= 2; }

private:
    friend class ZXGraph;
    size_t _id;
    VertexType _type;
    QubitIdType _qubit;  // for boundary vertices, this is the qubit id; for non-boundary vertices,
                         // this is a dummy value that may be used to mark temporary information
    Phase _phase;
    float _row;
    float _col;
    Neighbors _neighbors;
};

class ZXGraph {  // NOLINT(cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    using QubitIdType = ZXVertex::QubitIdType;

    ZXGraph() {}

    ~ZXGraph() {
        for (auto& v : _vertices) {
            delete v;
        }
    }

    ZXGraph(ZXGraph const& other);

    ZXGraph(ZXGraph&& other) noexcept = default;

    ZXGraph(ZXVertexList const& vertices,
            ZXVertexList const& inputs,
            ZXVertexList const& outputs);

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
    }

    friend void swap(ZXGraph& a, ZXGraph& b) noexcept {
        a.swap(b);
    }

    // Getter and Setter

    void set_inputs(ZXVertexList const& inputs) { _inputs = inputs; }
    void set_outputs(ZXVertexList const& outputs) { _outputs = outputs; }
    void set_filename(std::string const& f) { _filename = f; }
    void add_procedures(std::vector<std::string> const& ps) { _procedures.insert(std::end(_procedures), std::begin(ps), std::end(ps)); }
    void add_procedure(std::string_view p) { _procedures.emplace_back(p); }

    size_t const& get_next_v_id() const { return _next_v_id; }
    ZXVertexList const& get_inputs() const { return _inputs; }
    ZXVertexList const& get_outputs() const { return _outputs; }
    ZXVertexList const& get_vertices() const { return _vertices; }
    size_t get_num_edges() const;
    size_t get_num_inputs() const { return _inputs.size(); }
    size_t get_num_outputs() const { return _outputs.size(); }
    size_t get_num_vertices() const { return _vertices.size(); }

    Neighbors const& get_neighbors(ZXVertex* v) const { return v->_neighbors; }
    size_t get_num_neighbors(ZXVertex* v) const { return v->_neighbors.size(); }
    NeighborPair const& get_first_neighbor(ZXVertex* v) const { return *(std::begin(v->_neighbors)); }
    NeighborPair const& get_second_neighbor(ZXVertex* v) const { return *(std::next(std::begin(v->_neighbors))); }

    std::string get_filename() const { return _filename; }
    std::vector<std::string> const& get_procedures() const { return _procedures; }

    std::vector<ZXVertex*> get_copied_neighbors(ZXVertex* v) const;

    // attributes
    bool is_neighbor(ZXVertex* v1, ZXVertex* v2) const { return v1->_neighbors.contains({v2, EdgeType::simple}) || v1->_neighbors.contains({v2, EdgeType::hadamard}); }
    bool is_neighbor(ZXVertex* v1, ZXVertex* v2, EdgeType et) const { return v1->_neighbors.contains({v2, et}); }

    bool is_empty() const;
    bool is_valid() const;
    bool is_v_id(size_t id) const;
    bool is_graph_like() const;
    bool is_identity() const;
    size_t get_num_gadgets() const;
    bool is_input_qubit(QubitIdType qubit) const { return (_input_list.contains(qubit)); }
    bool is_output_qubit(QubitIdType qubit) const { return (_output_list.contains(qubit)); }

    bool is_gadget_leaf(ZXVertex*) const;
    bool is_gadget_axel(ZXVertex*) const;
    bool has_dangling_neighbors(ZXVertex*) const;

    double density();
    inline size_t t_count() const {
        return std::ranges::count_if(_vertices, [](ZXVertex* v) { return (v->get_phase().denominator() == 4); });
    }
    inline size_t non_clifford_count() const {
        return std::ranges::count_if(_vertices, [](ZXVertex* v) { return !v->is_clifford(); });
    }
    inline size_t non_clifford_t_count() const { return non_clifford_count() - t_count(); }

    // Add and Remove
    ZXVertex* add_input(QubitIdType qubit, float col = 0.f);
    ZXVertex* add_input(QubitIdType qubit, float row, float col);
    ZXVertex* add_output(QubitIdType qubit, float col = 0.f);
    ZXVertex* add_output(QubitIdType qubit, float row, float col);
    ZXVertex* add_vertex(VertexType vt, Phase phase = Phase(), float row = 0.f, float col = 0.f);
    void add_edge(ZXVertex* vs, ZXVertex* vt, EdgeType et);

    size_t remove_isolated_vertices();
    size_t remove_vertex(ZXVertex* v);

    size_t remove_vertices(std::vector<ZXVertex*> const& vertices);
    size_t remove_edge(EdgePair const& ep);
    size_t remove_edge(ZXVertex* vs, ZXVertex* vt, EdgeType etype);
    size_t remove_edges(std::span<EdgePair const> epairs);
    size_t remove_all_edges_between(ZXVertex* vs, ZXVertex* vt);

    // Operation on graph
    void adjoint();
    void assign_vertex_to_boundary(QubitIdType qubit, bool is_input, VertexType vtype, Phase phase);

    // helper functions for simplifiers
    void gadgetize_phase(ZXVertex* v, Phase const& keep_phase = Phase(0));
    ZXVertex* add_buffer(ZXVertex* vertex_to_protect, ZXVertex* vertex_other, EdgeType etype);

    // Find functions
    ZXVertex* find_vertex_by_id(size_t const& id) const;

    // Action functions (zxGraphAction.cpp)
    void sort_io_by_qubit();
    void toggle_vertex(ZXVertex* v);
    void lift_qubit(int n);
    void relabel_vertex_ids(size_t id_start) {
        std::ranges::for_each(this->_vertices, [&id_start](ZXVertex* v) { v->set_id(id_start++); });
    }
    ZXGraph& compose(ZXGraph const& target);
    ZXGraph& tensor_product(ZXGraph const& target);
    void add_gadget(Phase p, std::vector<ZXVertex*> const& vertices);
    void remove_gadget(ZXVertex* v);
    std::unordered_map<size_t, ZXVertex*> create_id_to_vertex_map() const;
    void adjust_vertex_coordinates();

    // Print functions (zxGraphPrint.cpp)
    void print_graph(spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;
    void print_inputs() const;
    void print_outputs() const;
    void print_io() const;
    void print_vertices(spdlog::level::level_enum lvl = spdlog::level::level_enum::off) const;
    void print_vertices(std::vector<size_t> cand) const;
    void print_vertices_by_rows(spdlog::level::level_enum lvl = spdlog::level::level_enum::off, std::vector<float> cand = {}) const;
    void print_edges() const;

    void print_difference(ZXGraph* other) const;

    // For mapping (in zxMapping.cpp)
    ZXVertexList get_non_boundary_vertices();
    ZXVertex* get_input_by_qubit(size_t const& q);
    ZXVertex* get_output_by_qubit(size_t const& q);
    void concatenate(ZXGraph const& other);
    std::unordered_map<size_t, ZXVertex*> const& get_input_list() const { return _input_list; }
    std::unordered_map<size_t, ZXVertex*> const& get_output_list() const { return _output_list; }

    // I/O (in zxIO.cpp)
    bool write_zx(std::filesystem::path const& filename, bool complete = false) const;
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
        for (auto& v : _vertices) {
            for (auto& [nb, etype] : this->get_neighbors(v)) {
                if (nb->get_id() > v->get_id())
                    lambda(make_edge_pair(v, nb, etype));
            }
        }
    }

    // divide into subgraphs and merge (in zxPartition.cpp)
    std::pair<std::vector<ZXGraph*>, std::vector<ZXCut>> create_subgraphs(std::vector<ZXVertexList> partitions);
    static ZXGraph* from_subgraphs(std::vector<ZXGraph*> const& subgraphs, std::vector<ZXCut> const& cuts);

private:
    size_t _next_v_id = 0;
    std::string _filename;
    std::vector<std::string> _procedures;
    ZXVertexList _inputs;
    ZXVertexList _outputs;
    ZXVertexList _vertices;
    std::unordered_map<size_t, ZXVertex*> _input_list;
    std::unordered_map<size_t, ZXVertex*> _output_list;

    void _dfs(std::unordered_set<ZXVertex*>& visited_vertices, std::vector<ZXVertex*>& topological_order, ZXVertex* v) const;
    void _bfs(std::unordered_set<ZXVertex*>& visited_vertices, std::vector<ZXVertex*>& topological_order, ZXVertex* v) const;

    void _move_vertices_from(ZXGraph& other);
};

dvlab::bit_matrix::BitMatrix get_biadjacency_matrix(ZXGraph const& graph, ZXVertexList const& row_vertices, ZXVertexList const& col_vertices);

}  // namespace qsyn::zx
