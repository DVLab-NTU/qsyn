/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph structures ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

#include "./zx_def.hpp"
#include "util/phase.hpp"

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
    // See `zxVertex.cpp` for details
public:
    ZXVertex(size_t id, int qubit, VertexType vt, Phase phase = Phase(), size_t col = 0)
        : _id{id}, _type{vt}, _qubit{qubit}, _phase{phase}, _col{(float)col} {}

    // Getter and Setter

    size_t const& get_id() const { return _id; }
    int const& get_qubit() const { return _qubit; }
    size_t const& get_pin() const { return _pin; }
    Phase const& get_phase() const { return _phase; }
    VertexType const& get_type() const { return _type; }
    float const& get_col() const { return _col; }
    Neighbors const& get_neighbors() const { return _neighbors; }
    NeighborPair const& get_first_neighbor() const { return *(_neighbors.begin()); }
    NeighborPair const& get_second_neighbor() const { return *next((_neighbors.begin())); }

    std::vector<ZXVertex*> get_copied_neighbors();
    size_t get_num_neighbors() const { return _neighbors.size(); }

    void set_id(size_t const& id) { _id = id; }
    void set_qubit(int const& q) { _qubit = q; }
    void set_pin(size_t const& p) { _pin = p; }
    void set_phase(Phase const& p) { _phase = p; }
    void set_col(float const& c) { _col = c; }
    void set_type(VertexType const& vt) { _type = vt; }
    // REVIEW - seems quite unsafe to me...
    void set_neighbors(Neighbors const& n) { _neighbors = n; }

    // Add and Remove
    void add_neighbor(NeighborPair const& n) { _neighbors.insert(n); }
    void add_neighbor(ZXVertex* v, EdgeType et) { _neighbors.emplace(v, et); }
    size_t remove_neighbor(NeighborPair const& n) { return _neighbors.erase(n); }
    size_t remove_neighbor(ZXVertex* v, EdgeType et) { return remove_neighbor(std::make_pair(v, et)); }
    size_t remove_neighbor(ZXVertex* v) { return remove_neighbor(v, EdgeType::simple) + remove_neighbor(v, EdgeType::hadamard); }

    // Print functions
    void print_vertex() const;
    void print_neighbors() const;

    // Test
    bool is_z() const { return get_type() == VertexType::z; }
    bool is_x() const { return get_type() == VertexType::x; }
    bool is_hbox() const { return get_type() == VertexType::h_box; }
    bool is_boundary() const { return get_type() == VertexType::boundary; }
    bool is_neighbor(ZXVertex* v) const { return _neighbors.contains(std::make_pair(v, EdgeType::simple)) || _neighbors.contains(std::make_pair(v, EdgeType::hadamard)); }
    bool is_neighbor(NeighborPair const& n) const { return _neighbors.contains(n); }
    bool is_neighbor(ZXVertex* v, EdgeType et) const { return is_neighbor(std::make_pair(v, et)); }

    std::optional<EdgeType> get_edge_type_between(ZXVertex* v) const {
        if (is_neighbor(v, EdgeType::simple)) return EdgeType::simple;
        if (is_neighbor(v, EdgeType::hadamard)) return EdgeType::hadamard;
        return std::nullopt;
    }
    bool has_n_pi_phase() const { return _phase.denominator() == 1; }
    bool is_clifford() const { return _phase.denominator() <= 2; }

    // DFS
    bool is_visited(unsigned global) { return global == _dfs_counter; }
    void mark_as_visited(unsigned global) { _dfs_counter = global; }

private:
    size_t _id;
    VertexType _type;
    int _qubit;
    Phase _phase;
    float _col;
    Neighbors _neighbors;
    unsigned _dfs_counter = 0;
    size_t _pin = UINT_MAX;
};

class ZXGraph {  // NOLINT(cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    ZXGraph() {}

    ~ZXGraph() {
        for (auto& v : _vertices) {
            delete v;
        }
    }

    ZXGraph(ZXGraph const& other) : _filename{other._filename}, _procedures{other._procedures} {
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

        other.for_each_edge([&old_v2new_v_map, this](EdgePair&& epair) {
            this->add_edge(old_v2new_v_map[epair.first.first], old_v2new_v_map[epair.first.second], epair.second);
        });
    }

    ZXGraph(ZXGraph&& other) noexcept = default;

    /**
     * @brief Construct a new ZXGraph object from a list of vertices.
     *
     * @param vertices the vertices
     * @param inputs the inputs. Note that the inputs must be a subset of the vertices.
     * @param outputs the outputs. Note that the outputs must be a subset of the vertices.
     * @param id
     */
    ZXGraph(ZXVertexList const& vertices,
            ZXVertexList const& inputs,
            ZXVertexList const& outputs) {
        _vertices = vertices;
        _inputs = inputs;
        _outputs = outputs;
        _next_v_id = 0;
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

    ZXGraph& operator=(ZXGraph copy) {
        copy.swap(*this);
        return *this;
    }

    void release() {
        _next_v_id = 0;
        _filename = "";
        _procedures.clear();
        _inputs.clear();
        _outputs.clear();
        _vertices.clear();
        _topological_order.clear();
        _input_list.clear();
        _output_list.clear();
        _global_traversal_counter = 1;
    }

    void swap(ZXGraph& other) noexcept {
        std::swap(_next_v_id, other._next_v_id);
        std::swap(_filename, other._filename);
        std::swap(_procedures, other._procedures);
        std::swap(_inputs, other._inputs);
        std::swap(_outputs, other._outputs);
        std::swap(_vertices, other._vertices);
        std::swap(_topological_order, other._topological_order);
        std::swap(_input_list, other._input_list);
        std::swap(_output_list, other._output_list);
        std::swap(_global_traversal_counter, other._global_traversal_counter);
    }

    friend void swap(ZXGraph& a, ZXGraph& b) noexcept {
        a.swap(b);
    }

    // Getter and Setter

    void set_inputs(ZXVertexList const& inputs) { _inputs = inputs; }
    void set_outputs(ZXVertexList const& outputs) { _outputs = outputs; }
    void set_filename(std::string const& f) { _filename = f; }
    void add_procedures(std::vector<std::string> const& ps) { _procedures.insert(_procedures.end(), ps.begin(), ps.end()); }
    void add_procedure(std::string_view p) { _procedures.emplace_back(p); }

    size_t const& get_next_v_id() const { return _next_v_id; }
    ZXVertexList const& get_inputs() const { return _inputs; }
    ZXVertexList const& get_outputs() const { return _outputs; }
    ZXVertexList const& get_vertices() const { return _vertices; }
    size_t get_num_edges() const;
    size_t get_num_inputs() const { return _inputs.size(); }
    size_t get_num_outputs() const { return _outputs.size(); }
    size_t get_num_vertices() const { return _vertices.size(); }
    std::string get_filename() const { return _filename; }
    std::vector<std::string> const& get_procedures() const { return _procedures; }

    // For testings
    bool is_empty() const;
    bool is_valid() const;
    bool is_v_id(size_t id) const;
    bool is_graph_like() const;
    bool is_identity() const;
    size_t get_num_gadgets() const;
    bool is_input_qubit(int qubit) const { return (_input_list.contains(qubit)); }
    bool is_output_qubit(int qubit) const { return (_output_list.contains(qubit)); }

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
    ZXVertex* add_input(int qubit, unsigned int col = 0);
    ZXVertex* add_output(int qubit, unsigned int col = 0);
    ZXVertex* add_vertex(int qubit, VertexType vt, Phase phase = Phase(), unsigned int col = 0);
    void add_edge(ZXVertex* vs, ZXVertex* vt, EdgeType et);

    size_t remove_isolated_vertices();
    size_t remove_vertex(ZXVertex* v);

    size_t remove_vertices(std::vector<ZXVertex*> const& vertices);
    size_t remove_edge(EdgePair const& ep);
    size_t remove_edge(ZXVertex* vs, ZXVertex* vt, EdgeType etype);
    size_t remove_edges(std::vector<EdgePair> const& eps);
    size_t remove_all_edges_between(ZXVertex* vs, ZXVertex* vt);

    // Operation on graph
    void adjoint();
    void assign_vertex_to_boundary(int qubit, bool is_input, VertexType vtype, Phase phase);

    // helper functions for simplifiers
    void gadgetize_phase(ZXVertex* v, Phase const& keep_phase = Phase(0));
    ZXVertex* add_buffer(ZXVertex* vertex_to_protect, ZXVertex* vertex_other, EdgeType etype);

    // Find functions
    ZXVertex* find_vertex_by_id(size_t const& id) const;

    // Action functions (zxGraphAction.cpp)
    void sort_io_by_qubit();
    void toggle_vertex(ZXVertex* v);
    void lift_qubit(size_t const& n);
    void relabel_vertex_ids(size_t id_start) {
        std::ranges::for_each(this->_vertices, [&id_start](ZXVertex* v) { v->set_id(id_start++); });
    }
    ZXGraph& compose(ZXGraph const& target);
    ZXGraph& tensor_product(ZXGraph const& target);
    void add_gadget(Phase p, std::vector<ZXVertex*> const& vertices);
    void remove_gadget(ZXVertex* v);
    std::unordered_map<size_t, ZXVertex*> create_id_to_vertex_map() const;
    void normalize();

    // Print functions (zxGraphPrint.cpp)
    void print_graph() const;
    void print_inputs() const;
    void print_outputs() const;
    void print_io() const;
    void print_vertices() const;
    void print_vertices(std::vector<size_t> cand) const;
    void print_qubits(std::vector<int> cand = {}) const;
    void print_edges() const;

    void print_difference(ZXGraph* other) const;
    void draw() const;

    // For mapping (in zxMapping.cpp)
    ZXVertexList get_non_boundary_vertices();
    ZXVertex* get_input_by_qubit(size_t const& q);
    ZXVertex* get_output_by_qubit(size_t const& q);
    void concatenate(ZXGraph const& other);
    std::unordered_map<size_t, ZXVertex*> const& get_input_list() const { return _input_list; }
    std::unordered_map<size_t, ZXVertex*> const& get_output_list() const { return _output_list; }

    // I/O (in zxIO.cpp)
    bool read_zx(std::filesystem::path const& filepath, bool keep_id = false);
    bool write_zx(std::string const& filename, bool complete = false) const;
    bool write_tikz(std::string const& filename) const;
    bool write_tikz(std::ostream& filename) const;
    bool write_pdf(std::string const& filename) const;
    bool write_tex(std::string const& filename) const;
    bool write_tex(std::ostream& filename) const;

    // Traverse (in zxTraverse.cpp)
    void update_topological_order() const;
    void update_breadth_level() const;
    template <typename F>
    void topological_traverse(F lambda) {
        update_topological_order();
        for_each(_topological_order.begin(), _topological_order.end(), lambda);
    }
    template <typename F>
    void topological_traverse(F lambda) const {
        update_topological_order();
        for_each(_topological_order.begin(), _topological_order.end(), lambda);
    }
    template <typename F>
    void for_each_edge(F lambda) const {
        for (auto& v : _vertices) {
            for (auto& [nb, etype] : v->get_neighbors()) {
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
    std::vector<ZXVertex*> mutable _topological_order;
    unsigned mutable _global_traversal_counter = 1;

    void _dfs(ZXVertex*) const;
    void _bfs(ZXVertex*) const;

    bool _build_graph_from_parser_storage(detail::StorageType const& storage, bool keep_id = false);

    void _move_vertices_from(ZXGraph& other);
};
