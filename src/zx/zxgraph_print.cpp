/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Print functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <map>
#include <ranges>
#include <string>

#include "fmt/core.h"
#include "util/text_format.hpp"
#include "zx/zxgraph.hpp"

using namespace std;

/**
 * @brief Print information of ZXGraph
 *
 */
void ZXGraph::print_graph() const {
    fmt::println("Graph ({} inputs, {} outputs, {} vertices, {} edges)", get_num_inputs(), get_num_outputs(), get_num_vertices(), get_num_edges());
}

/**
 * @brief Print Inputs of ZXGraph
 *
 */
void ZXGraph::print_inputs() const {
    fmt::println("Input:  ({})", fmt::join(_inputs | views::transform([](ZXVertex* v) { return v->get_id(); }), ", "));
    fmt::println("Total #Inputs: {}", get_num_inputs());
}

/**
 * @brief Print Outputs of ZXGraph
 *
 */
void ZXGraph::print_outputs() const {
    fmt::println("Output: ({})", fmt::join(_outputs | views::transform([](ZXVertex* v) { return v->get_id(); }), ", "));
    fmt::println("Total #Outputs: {}", get_num_outputs());
}

/**
 * @brief Print Inputs and Outputs of ZXGraph
 *
 */
void ZXGraph::print_io() const {
    fmt::println("Input:  ({})", fmt::join(_inputs | views::transform([](ZXVertex* v) { return v->get_id(); }), ", "));
    fmt::println("Output: ({})", fmt::join(_outputs | views::transform([](ZXVertex* v) { return v->get_id(); }), ", "));
    fmt::println("Total #(I,O): ({}, {})", get_num_inputs(), get_num_outputs());
}

/**
 * @brief Print Vertices of ZXGraph
 *
 */
void ZXGraph::print_vertices() const {
    cout << "\n";
    ranges::for_each(_vertices, [](ZXVertex* v) { v->print_vertex(); });
    cout << "Total #Vertices: " << get_num_vertices() << "\n\n";
}

/**
 * @brief Print Vertices of ZXGraph in `cand`.
 *
 * @param cand
 */
void ZXGraph::print_vertices(vector<size_t> cand) const {
    unordered_map<size_t, ZXVertex*> id2_vmap = create_id_to_vertex_map();

    cout << "\n";
    for (size_t i = 0; i < cand.size(); i++) {
        if (is_v_id(cand[i])) id2_vmap[cand[i]]->print_vertex();
    }
    cout << "\n";
}

/**
 * @brief Print Vertices of ZXGraph in `cand` by qubit.
 *
 * @param cand
 */
void ZXGraph::print_qubits(vector<int> cand) const {
    map<int, vector<ZXVertex*>> q2_vmap;
    for (auto const& v : _vertices) {
        if (!q2_vmap.contains(v->get_qubit())) {
            vector<ZXVertex*> tmp(1, v);
            q2_vmap[v->get_qubit()] = tmp;
        } else
            q2_vmap[v->get_qubit()].emplace_back(v);
    }
    if (cand.empty()) {
        for (auto const& [key, vec] : q2_vmap) {
            cout << "\n";
            for (auto const& v : vec) v->print_vertex();
            cout << "\n";
        }
    } else {
        for (size_t i = 0; i < cand.size(); i++) {
            if (q2_vmap.contains(cand[i])) {
                cout << "\n";
                for (auto const& v : q2_vmap[cand[i]]) v->print_vertex();
            }
            cout << "\n";
        }
    }
}

/**
 * @brief Print Edges of ZXGraph
 *
 */
void ZXGraph::print_edges() const {
    for_each_edge([](EdgePair const& epair) {
        fmt::println("{:<12} Type: {}", fmt::format("({}, {})", epair.first.first->get_id(), epair.first.second->get_id()), epair.second);
    });
    fmt::println("Total #Edges: {}", get_num_edges());
}

/**
 * @brief For each vertex ID, print the vertices that only present in one of the graph,
 *        or vertices that differs in the neighbors. This is not a graph isomorphism detector!!!
 *
 * @param other
 */
void ZXGraph::print_difference(ZXGraph* other) const {
    assert(other != nullptr);

    size_t n_i_ds = max(_next_v_id, other->_next_v_id);
    ZXVertexList v1s, v2s;
    for (size_t i = 0; i < n_i_ds; ++i) {
        auto v1 = find_vertex_by_id(i);
        auto v2 = other->find_vertex_by_id(i);
        if (v1 && v2) {
            if (v1->get_num_neighbors() != v2->get_num_neighbors() ||
                std::invoke([&v1, &v2, &other]() -> bool {
                    for (auto& [nb1, e1] : v1->get_neighbors()) {
                        ZXVertex* nb2 = other->find_vertex_by_id(nb1->get_id());
                        if (!nb2) return true;
                        if (!nb2->is_neighbor(v2, e1)) return true;
                    }
                    return false;
                })) {
                v1s.insert(v1);
                v2s.insert(v2);
            }
        } else if (v1) {
            v1s.insert(v1);
        } else if (v2) {
            v2s.insert(v2);
        }
    }
    cout << ">>>" << endl;
    for (auto& v : v1s) {
        v->print_vertex();
    }
    cout << "===" << endl;
    for (auto& v : v2s) {
        v->print_vertex();
    }
    cout << "<<<" << endl;
}
namespace detail {

/**
 * @brief Print the vertex with color
 *
 * @param v
 * @return string
 */
string get_colored_vertex_string(ZXVertex* v) {
    using namespace dvlab;
    if (v->get_type() == VertexType::boundary)
        return fmt::format("{}", v->get_id());
    else if (v->get_type() == VertexType::z)
        return fmt::format("{}", fmt_ext::styled_if_ansi_supported(v->get_id(), fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
    else if (v->get_type() == VertexType::x)
        return fmt::format("{}", fmt_ext::styled_if_ansi_supported(v->get_id(), fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
    else
        return fmt::format("{}", fmt_ext::styled_if_ansi_supported(v->get_id(), fmt::fg(fmt::terminal_color::yellow) | fmt::emphasis::bold));
}

}  // namespace detail
/**
 * @brief Draw ZXGraphin CLI
 *
 */
void ZXGraph::draw() const {
    cout << endl;
    unsigned int max_col = 0;  // number of columns -1
    unordered_map<int, int> q_pair;
    vector<int> qubit_num;  // number of qubit

    // maxCol
    for (auto& o : get_outputs()) {
        if (o->get_col() > max_col) max_col = o->get_col();
    }

    // qubitNum
    vector<int> qubit_num_temp;  // number of qubit
    for (auto& v : get_vertices()) {
        qubit_num_temp.emplace_back(v->get_qubit());
    }
    sort(qubit_num_temp.begin(), qubit_num_temp.end());
    if (qubit_num_temp.size() == 0) {
        cout << "Empty graph!!" << endl;
        return;
    }
    size_t offset = qubit_num_temp[0];
    qubit_num.emplace_back(0);
    for (size_t i = 1; i < qubit_num_temp.size(); i++) {
        if (qubit_num_temp[i - 1] == qubit_num_temp[i]) {
            continue;
        } else {
            qubit_num.emplace_back(qubit_num_temp[i] - offset);
        }
    }
    qubit_num_temp.clear();

    for (size_t i = 0; i < qubit_num.size(); i++) q_pair[i] = qubit_num[i];
    vector<ZXVertex*> tmp;
    tmp.resize(qubit_num.size());
    vector<vector<ZXVertex*>> col_list(max_col + 1, tmp);

    for (auto& v : get_vertices()) col_list[v->get_col()][q_pair[v->get_qubit() - offset]] = v;

    vector<size_t> max_length(max_col + 1, 0);
    for (size_t i = 0; i < col_list.size(); i++) {
        for (size_t j = 0; j < col_list[i].size(); j++) {
            if (col_list[i][j] != nullptr) {
                if (to_string(col_list[i][j]->get_id()).length() > max_length[i]) max_length[i] = to_string(col_list[i][j]->get_id()).length();
            }
        }
    }
    size_t max_length_q = 0;
    for (size_t i = 0; i < qubit_num.size(); i++) {
        int temp = offset + i;
        if (to_string(temp).length() > max_length_q) max_length_q = to_string(temp).length();
    }

    for (size_t i = 0; i < qubit_num.size(); i++) {
        // print qubit
        int temp = offset + i;
        cout << "[";
        for (size_t i = 0; i < max_length_q - to_string(temp).length(); i++) {
            cout << " ";
        }
        cout << temp << "]";

        // print row
        for (size_t j = 0; j <= max_col; j++) {
            if (i < -offset) {
                if (col_list[j][i] != nullptr) {
                    cout << "(" << detail::get_colored_vertex_string(col_list[j][i]) << ")   ";
                } else {
                    if (j == max_col)
                        cout << endl;
                    else {
                        cout << "   ";
                        for (size_t k = 0; k < max_length[j] + 2; k++) cout << " ";
                    }
                }
            } else if (col_list[j][i] != nullptr) {
                if (j == max_col)
                    cout << "(" << detail::get_colored_vertex_string(col_list[j][i]) << ")" << endl;
                else
                    cout << "(" << detail::get_colored_vertex_string(col_list[j][i]) << ")---";

                for (size_t k = 0; k < max_length[j] - to_string(col_list[j][i]->get_id()).length(); k++) cout << "-";
            } else {
                cout << "---";
                for (size_t k = 0; k < max_length[j] + 2; k++) cout << "-";
            }
        }
        cout << endl;
    }
    for (auto& a : col_list) {
        a.clear();
    }
    col_list.clear();

    max_length.clear();
    qubit_num.clear();
}
