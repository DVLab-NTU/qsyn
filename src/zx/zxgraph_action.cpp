/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <queue>

#include "./zx_def.hpp"
#include "./zxgraph.hpp"
#include "util/logger.hpp"
#include "util/phase.hpp"

using namespace std;
extern dvlab::Logger LOGGER;

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
void ZXGraph::toggle_vertex(ZXVertex* v) {
    if (!v->is_z() && !v->is_x()) return;
    Neighbors toggled_neighbors;
    for (auto& itr : v->get_neighbors()) {
        toggled_neighbors.insert(make_pair(itr.first, toggle_edge(itr.second)));
        itr.first->remove_neighbor(make_pair(v, itr.second));
        itr.first->add_neighbor(make_pair(v, toggle_edge(itr.second)));
    }
    v->set_neighbors(toggled_neighbors);
    v->set_type(v->get_type() == VertexType::z ? VertexType::x : VertexType::z);
}

/**
 * @brief Lift each vertex's qubit in ZXGraph with `n`.
 *        Ex: origin: 0 -> after lifting: n
 *
 * @param n
 */
void ZXGraph::lift_qubit(size_t const& n) {
    for (auto const& v : _vertices) {
        v->set_qubit(v->get_qubit() + n);
    }

    unordered_map<size_t, ZXVertex*> new_input_list, new_output_list;

    for_each(_input_list.begin(), _input_list.end(),
             [&n, &new_input_list](pair<size_t, ZXVertex*> itr) {
                 new_input_list[itr.first + n] = itr.second;
             });
    for_each(_output_list.begin(), _output_list.end(),
             [&n, &new_output_list](pair<size_t, ZXVertex*> itr) {
                 new_output_list[itr.first + n] = itr.second;
             });

    _input_list = new_input_list;
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
        cerr << "Error: The composing ZXGraph's #input is not equivalent to the original ZXGraph's #output." << endl;
        return *this;
    }

    ZXGraph copied_graph{target};

    // Get maximum column in `this`
    unsigned max_col = 0;
    for (auto const& o : this->get_outputs()) {
        if (o->get_col() > max_col) max_col = o->get_col();
    }

    // Update `_col` of copiedGraph to make them unique to the original graph
    for (auto const& v : copied_graph.get_vertices()) {
        v->set_col(v->get_col() + max_col + 1);
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

    _outputs = copied_graph._outputs;
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
    int ori_max_qubit = INT_MIN, ori_min_qubit = INT_MAX;
    int copied_min_qubit = INT_MAX;
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
    size_t lift_q = (ori_max_qubit - ori_min_qubit + 1) - copied_min_qubit;
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
           v->get_num_neighbors() == 1 &&
           v->get_first_neighbor().first->get_type() == VertexType::z &&
           v->get_first_neighbor().second == EdgeType::hadamard &&
           v->get_first_neighbor().first->has_n_pi_phase();
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
           any_of(v->get_neighbors().begin(), v->get_neighbors().end(),
                  [](NeighborPair const& nbp) {
                      return nbp.first->get_num_neighbors() == 1 && nbp.first->is_z() && nbp.second == EdgeType::hadamard;
                  });
}

bool ZXGraph::has_dangling_neighbors(ZXVertex* v) const {
    return any_of(v->get_neighbors().begin(), v->get_neighbors().end(),
                  [](NeighborPair const& nbp) {
                      return nbp.first->get_num_neighbors() == 1;
                  });
}

/**
 * @brief Add phase gadget of phase `p` for each vertex in `verVec`.
 *
 * @param p
 * @param verVec
 */
void ZXGraph::add_gadget(Phase p, vector<ZXVertex*> const& vertices) {
    for (size_t i = 0; i < vertices.size(); i++) {
        if (vertices[i]->get_type() == VertexType::boundary || vertices[i]->get_type() == VertexType::h_box) return;
    }

    ZXVertex* axel = add_vertex(-1, VertexType::z, Phase(0));
    ZXVertex* leaf = add_vertex(-2, VertexType::z, p);

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
    ZXVertex* axel = v->get_first_neighbor().first;
    remove_vertex(axel);
    remove_vertex(v);
}

/**
 * @brief Generate a id-2-ZXVertex* map
 *
 * @return unordered_map<size_t, ZXVertex*>
 */
unordered_map<size_t, ZXVertex*> ZXGraph::create_id_to_vertex_map() const {
    unordered_map<size_t, ZXVertex*> id2_vertex_map;
    for (auto const& v : _vertices) id2_vertex_map[v->get_id()] = v;
    return id2_vertex_map;
}

/**
 * @brief Rearrange nodes on each qubit so that each node can be seperated in the printed graph.
 *
 */
void ZXGraph::normalize() {
    unordered_map<int, vector<ZXVertex*> > mp;
    unordered_set<int> vis;
    queue<ZXVertex*> cand;
    for (auto const& i : _inputs) {
        cand.push(i);
        vis.insert(i->get_id());
    }
    while (!cand.empty()) {
        ZXVertex* node = cand.front();
        cand.pop();
        mp[node->get_qubit()].emplace_back(node);
        for (auto const& n : node->get_neighbors()) {
            if (vis.find(n.first->get_id()) == vis.end()) {
                cand.push(n.first);
                vis.insert(n.first->get_id());
            }
        }
    }
    int max_col = 0;
    for (auto& i : mp) {
        int col = 0;
        for (auto& v : i.second) {
            v->set_col(col);
            col++;
        }
        col--;
        max_col = max(max_col, col);
    }
    for (auto& o : _outputs) o->set_col(max_col);
}
