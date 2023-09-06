/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./extract.hpp"

#include <cassert>
#include <memory>

#include "duostra/duostra.hpp"
#include "duostra/mapping_eqv_checker.hpp"
#include "qcir/qcir.hpp"
#include "zx/simplifier/simplify.hpp"
#include "zx/simplifier/zx_rules_template.hpp"
#include "zx/zxgraph.hpp"

using namespace std;

extern bool stop_requested();

bool SORT_FRONTIER = 0;
bool SORT_NEIGHBORS = 1;
bool PERMUTE_QUBITS = 1;
bool FILTER_DUPLICATED_CXS = 1;
size_t BLOCK_SIZE = 5;
size_t OPTIMIZE_LEVEL = 2;
extern size_t VERBOSE;

/**
 * @brief Construct a new Extractor:: Extractor object
 *
 * @param g
 * @param c
 * @param d
 */
Extractor::Extractor(ZXGraph* g, QCir* c, std::optional<Device> d) : _graph(g), _device(d), _device_backup(d) {
    _logical_circuit = (c == nullptr) ? new QCir : c;
    _physical_circuit = to_physical() ? new QCir() : nullptr;
    initialize(c == nullptr);
}

/**
 * @brief Initialize the extractor. Set ZXGraph to QCir qubit map.
 *
 */
void Extractor::initialize(bool from_empty_qcir) {
    if (VERBOSE >= 4) cout << "Initialize" << endl;
    size_t cnt = 0;
    for (auto& o : _graph->get_outputs()) {
        if (!o->get_first_neighbor().first->is_boundary()) {
            o->get_first_neighbor().first->set_qubit(o->get_qubit());
            _frontier.emplace(o->get_first_neighbor().first);
        }
        _qubit_map[o->get_qubit()] = cnt;
        if (from_empty_qcir)
            _logical_circuit->add_qubits(1);
        cnt += 1;
    }

    // NOTE - get zx to qc qubit mapping
    _frontier.sort([](ZXVertex const* a, ZXVertex const* b) {
        return a->get_qubit() < b->get_qubit();
    });

    update_neighbors();
    for (auto& v : _graph->get_vertices()) {
        if (_graph->is_gadget_leaf(v)) {
            _axels.emplace(v->get_first_neighbor().first);
        }
    }
    if (VERBOSE >= 8) {
        print_frontier();
        print_neighbors();
        _graph->print_qubits();
        _logical_circuit->print_qubits();
    }
}

/**
 * @brief Extract the graph into circuit
 *
 * @return QCir*
 */
QCir* Extractor::extract() {
    if (!extraction_loop(-1)) {
        return nullptr;
    }
    if (stop_requested()) {
        cerr << "Warning: conversion is interrupted" << endl;
        return nullptr;
    }

    if (VERBOSE >= 3)
        cout << "Finish extracting!" << endl;
    if (VERBOSE >= 8) {
        _logical_circuit->print_qubits();
        _graph->print_qubits();
    }

    if (PERMUTE_QUBITS) {
        permute_qubits();
        if (VERBOSE >= 8) {
            _logical_circuit->print_qubits();
            _graph->print_qubits();
        }
    }

    return _logical_circuit;
}

/**
 * @brief Extraction Loop
 *
 * @param max_iter perform max_iter iterations
 * @return true if successfully extracted
 * @return false if not
 */
bool Extractor::extraction_loop(size_t max_iter) {
    while (max_iter > 0 && !stop_requested()) {
        clean_frontier();
        update_neighbors();

        if (_frontier.empty()) break;

        if (remove_gadget()) {
            if (VERBOSE >= 4) cout << "Gadget(s) are removed." << endl;
            if (VERBOSE >= 8) {
                print_frontier();
                _graph->print_qubits();
                _logical_circuit->print_qubits();
            }
            continue;
        }

        if (contains_single_neighbor()) {
            if (VERBOSE >= 4) cout << "Construct an easy matrix." << endl;
            _biadjacency.from_zxvertices(_frontier, _neighbors);
        } else {
            if (VERBOSE >= 4) cout << "Perform Gaussian Elimination." << endl;
            extract_cxs();
        }
        if (extract_hadamards_from_matrix() == 0) {
            cerr << "Error: no candidate found in extractHsFromM2!!" << endl;
            _biadjacency.print_matrix();
            return false;
        }
        _biadjacency.reset();
        _cnots.clear();

        if (VERBOSE >= 8) {
            print_frontier();
            print_neighbors();
            _graph->print_qubits();
            _logical_circuit->print_qubits();
        }

        if (max_iter != size_t(-1)) max_iter--;
    }
    return true;
}

/**
 * @brief Clean frontier. Contain extract singles and CZs. Used in extract.
 *
 */
void Extractor::clean_frontier() {
    if (VERBOSE >= 4) cout << "Clean Frontier" << endl;
    // NOTE - Edge and Phase
    extract_singles();
    // NOTE - CZs
    extract_czs();
}

/**
 * @brief Extract single qubit gates, i.e. z-rotate family and H. Used in clean frontier.
 *
 */
void Extractor::extract_singles() {
    if (VERBOSE >= 4) cout << "Extract Singles" << endl;
    vector<pair<ZXVertex*, ZXVertex*>> toggle_list;
    for (ZXVertex* o : _graph->get_outputs()) {
        if (o->get_first_neighbor().second == EdgeType::hadamard) {
            prepend_single_qubit_gate("h", _qubit_map[o->get_qubit()], Phase(0));
            toggle_list.emplace_back(o, o->get_first_neighbor().first);
        }
        Phase ph = o->get_first_neighbor().first->get_phase();
        if (ph != Phase(0)) {
            prepend_single_qubit_gate("rotate", _qubit_map[o->get_qubit()], ph);
            o->get_first_neighbor().first->set_phase(Phase(0));
        }
    }
    for (auto& [s, t] : toggle_list) {
        _graph->add_edge(s, t, EdgeType::simple);
        _graph->remove_edge(s, t, EdgeType::hadamard);
    }
    if (VERBOSE >= 8) {
        _logical_circuit->print_qubits();
        _graph->print_qubits();
    }
}

/**
 * @brief Extract CZs from frontier. Used in clean frontier.
 *
 * @param check check no phase in the frontier if true
 * @return true
 * @return false
 */
bool Extractor::extract_czs(bool check) {
    if (VERBOSE >= 4) cout << "Extract CZs" << endl;

    if (check) {
        for (auto& f : _frontier) {
            if (f->get_phase() != Phase(0)) {
                cout << "Note: frontier contains phases, extract them first." << endl;
                return false;
            }
            for (auto& [n, e] : f->get_neighbors()) {
                if (_graph->get_outputs().contains(n) && e == EdgeType::hadamard) {
                    cout << "Note: frontier contains hadamard edge to boundary, extract them first." << endl;
                    return false;
                }
            }
        }
    }

    vector<pair<ZXVertex*, ZXVertex*>> remove_list;

    for (auto itr = _frontier.begin(); itr != _frontier.end(); itr++) {
        for (auto jtr = next(itr); jtr != _frontier.end(); jtr++) {
            if ((*itr)->is_neighbor((*jtr))) {
                remove_list.emplace_back((*itr), (*jtr));
            }
        }
    }
    vector<Operation> ops;
    for (auto const& [s, t] : remove_list) {
        _graph->remove_edge(s, t, EdgeType::hadamard);
        Operation op(GateType::cz, Phase(0), {_qubit_map[s->get_qubit()], _qubit_map[t->get_qubit()]}, {});
        ops.emplace_back(op);
    }
    if (ops.size() > 0) {
        prepend_series_gates(ops);
    }
    if (VERBOSE >= 8) {
        _logical_circuit->print_qubits();
        _graph->print_qubits();
    }

    return true;
}

/**
 * @brief Extract CXs
 *
 * @param strategy (0: directly by result of Gaussian Elimination, 1-default: Filter Duplicated CNOT)
 */
void Extractor::extract_cxs(size_t strategy) {
    _num_cx_iterations++;
    biadjacency_eliminations();
    update_graph_by_matrix();

    if (VERBOSE >= 4) cout << "Extract CXs" << endl;
    unordered_map<size_t, ZXVertex*> front_id2_vertex;
    size_t cnt = 0;
    for (auto& f : _frontier) {
        front_id2_vertex[cnt] = f;
        cnt++;
    }

    for (auto& [t, c] : _cnots) {
        // NOTE - targ and ctrl are opposite here
        size_t ctrl = _qubit_map[front_id2_vertex[c]->get_qubit()];
        size_t targ = _qubit_map[front_id2_vertex[t]->get_qubit()];
        if (VERBOSE >= 4) cout << "Add CX: " << ctrl << " " << targ << endl;
        prepend_double_qubit_gate("cx", {ctrl, targ}, Phase(0));
    }
}

/**
 * @brief Extract Hadamard if singly connected vertex in frontier is found
 *
 * @param check if true, check frontier is cleaned and axels not connected to frontiers
 * @return size_t
 */
size_t Extractor::extract_hadamards_from_matrix(bool check) {
    if (VERBOSE >= 4) cout << "Extract Hs" << endl;

    if (check) {
        if (!frontier_is_cleaned()) {
            cout << "Note: frontier is dirty, please clean it first." << endl;
            return 0;
        }
        if (axel_in_neighbors()) {
            cout << "Note: axel(s) are in the neighbors, please remove gadget(s) first." << endl;
            return 0;
        }
        _biadjacency.from_zxvertices(_frontier, _neighbors);
    }

    unordered_map<size_t, ZXVertex*> frontier_id_to_vertex;
    unordered_map<size_t, ZXVertex*> neighbor_id_to_vertex;
    size_t cnt = 0;
    for (auto& f : _frontier) {
        frontier_id_to_vertex[cnt] = f;
        cnt++;
    }
    cnt = 0;
    for (auto& n : _neighbors) {
        neighbor_id_to_vertex[cnt] = n;
        cnt++;
    }

    // NOTE - Store pairs to be modified
    vector<pair<ZXVertex*, ZXVertex*>> front_neigh_pairs;

    for (size_t row = 0; row < _biadjacency.num_rows(); ++row) {
        if (!_biadjacency[row].is_one_hot()) continue;

        for (size_t col = 0; col < _biadjacency.num_cols(); col++) {
            if (_biadjacency[row][col] == 1) {
                front_neigh_pairs.emplace_back(frontier_id_to_vertex[row], neighbor_id_to_vertex[col]);
                break;
            }
        }
    }

    for (auto& [f, n] : front_neigh_pairs) {
        // NOTE - Add Hadamard according to the v of frontier (row)
        prepend_single_qubit_gate("h", _qubit_map[f->get_qubit()], Phase(0));
        // NOTE - Set #qubit and #col according to the old frontier
        n->set_qubit(f->get_qubit());
        n->set_col(f->get_col());

        // NOTE - Connect edge between boundary and neighbor
        for (auto& [bound, ep] : f->get_neighbors()) {
            if (bound->is_boundary()) {
                _graph->add_edge(bound, n, ep);
                break;
            }
        }
        // NOTE - Replace frontier by neighbor
        _frontier.erase(f);
        _frontier.emplace(n);
        _graph->remove_vertex(f);
    }

    if (check && front_neigh_pairs.size() == 0) {
        cerr << "Error: no candidate found!!" << endl;
        print_matrix();
    }
    return front_neigh_pairs.size();
}

/**
 * @brief Remove gadget according to Pivot Boundary Rule
 *
 * @param check if true, check the frontier is clean
 * @return true if gadget(s) are removed
 * @return false if not
 */
bool Extractor::remove_gadget(bool check) {
    if (VERBOSE >= 4) cout << "Remove Gadget" << endl;

    if (check) {
        if (_frontier.empty()) {
            cout << "Note: no vertex left." << endl;
            return false;
        }
        if (!frontier_is_cleaned()) {
            cout << "Note: frontier is dirty, please clean it first." << endl;
            return false;
        }
    }

    if (VERBOSE >= 8) _graph->print_vertices();
    if (VERBOSE >= 5) {
        print_frontier();
        print_axels();
    }
    bool gadget_removed = false;
    for (auto& n : _neighbors) {
        if (!_axels.contains(n)) {
            continue;
        }
        for (auto& [candidate, _] : n->get_neighbors()) {
            if (_frontier.contains(candidate)) {
                std::vector<PivotBoundaryRule::MatchType> matches = {
                    {candidate, n},
                };
                _axels.erase(n);
                _frontier.erase(candidate);

                ZXVertex* target_boundary = nullptr;
                for (auto& [boundary, _] : candidate->get_neighbors()) {
                    if (boundary->is_boundary()) {
                        target_boundary = boundary;
                        break;
                    }
                }

                PivotBoundaryRule().apply(*_graph, matches);

                assert(target_boundary != nullptr);
                _frontier.emplace(target_boundary->get_first_neighbor().first);
                // REVIEW - qubit_map
                gadget_removed = true;
                break;
            }
        }
    }
    if (VERBOSE >= 8) _graph->print_vertices();
    if (VERBOSE >= 5) {
        print_frontier();
        print_axels();
    }
    return gadget_removed;
}

/**
 * @brief Swap columns (order of neighbors) to put the most of them on the diagonal of the bi-adjacency matrix
 *
 */
void Extractor::column_optimal_swap() {
    // NOTE - Swap columns of matrix and order of neighbors

    _row_info.clear();
    _col_info.clear();

    size_t row_cnt = _biadjacency.num_rows();

    size_t col_cnt = _biadjacency.num_cols();
    _row_info.resize(row_cnt);
    _col_info.resize(col_cnt);

    for (size_t i = 0; i < row_cnt; i++) {
        for (size_t j = 0; j < col_cnt; j++) {
            if (_biadjacency[i][j] == 1) {
                _row_info[i].emplace(j);
                _col_info[j].emplace(i);
            }
        }
    }

    Target target;
    target = _find_column_swap(target);

    std::set<size_t> col_set, left, right, targ_key, targ_val;
    for (size_t i = 0; i < col_cnt; i++) col_set.emplace(i);
    for (auto& [k, v] : target) {
        targ_key.emplace(k);
        targ_val.emplace(v);
    }

    std::set_difference(col_set.begin(), col_set.end(), targ_val.begin(), targ_val.end(), inserter(left, left.end()));
    std::set_difference(col_set.begin(), col_set.end(), targ_key.begin(), targ_key.end(), inserter(right, right.end()));
    vector<size_t> lvec(left.begin(), left.end());
    vector<size_t> rvec(right.begin(), right.end());
    for (size_t i = 0; i < lvec.size(); i++) {
        target[rvec[i]] = lvec[i];
    }
    Target perm;
    for (auto& [k, v] : target) {
        perm[v] = k;
    }
    vector<ZXVertex*> neb_vec(_neighbors.begin(), _neighbors.end());
    vector<ZXVertex*> new_neb_vec = neb_vec;
    for (size_t i = 0; i < neb_vec.size(); i++) {
        new_neb_vec[i] = neb_vec[perm[i]];
    }
    _neighbors.clear();
    for (auto& v : new_neb_vec) _neighbors.emplace(v);
}

/**
 * @brief Find the swap target. Used in function columnOptimalSwap
 *
 * @param target
 * @return Target (unordered_map of swaps)
 */
Extractor::Target Extractor::_find_column_swap(Target target) {
    size_t row_cnt = _row_info.size();
    // size_t colCnt = _colInfo.size();

    set<size_t> claimed_cols;
    set<size_t> claimed_rows;
    for (auto& [key, value] : target) {
        claimed_cols.emplace(key);
        claimed_rows.emplace(value);
    }

    while (true) {
        std::optional<size_t> min_index = std::nullopt;
        set<size_t> min_options;
        for (size_t i = 0; i < 1000; i++) min_options.emplace(i);
        bool found_col = false;
        for (size_t i = 0; i < row_cnt; i++) {
            if (claimed_rows.contains(i)) continue;
            set<size_t> free_cols;
            // NOTE - find the free columns
            set_difference(_row_info[i].begin(), _row_info[i].end(), claimed_cols.begin(), claimed_cols.end(), inserter(free_cols, free_cols.end()));
            if (free_cols.size() == 1) {
                // NOTE - pop the only element
                size_t j = *(free_cols.begin());
                free_cols.erase(j);

                target[j] = i;
                claimed_cols.emplace(j);
                claimed_rows.emplace(i);
                found_col = true;
                break;
            }

            if (free_cols.size() == 0) {
                if (VERBOSE >= 5) cout << "Note: no free column for column optimal swap!!" << endl;
                Target t;
                return t;  // NOTE - Contradiction
            }

            for (auto& j : free_cols) {
                set<size_t> free_rows;
                set_difference(_col_info[j].begin(), _col_info[j].end(), claimed_rows.begin(), claimed_rows.end(), inserter(free_rows, free_rows.end()));
                if (free_rows.size() == 1) {
                    target[j] = i;  // NOTE - j can only be connected to i
                    claimed_cols.emplace(j);
                    claimed_rows.emplace(i);
                    found_col = true;
                    break;
                }
            }
            if (found_col) break;
            if (free_cols.size() < min_options.size()) {
                min_index = i;
                min_options = free_cols;
            }
        }

        if (!found_col) {
            bool done = true;
            for (size_t r = 0; r < row_cnt; ++r) {
                if (!claimed_rows.contains(r)) {
                    done = false;
                    break;
                }
            }
            if (done) {
                return target;
            }
            if (min_index == std::nullopt) {
                cerr << "Error: this shouldn't happen ever" << endl;
                assert(false);
            }
            // NOTE -  depth-first search
            Target copied_target = target;
            if (VERBOSE >= 8) cout << "Backtracking on " << min_index.value() << endl;

            for (auto& idx : min_options) {
                if (VERBOSE >= 8) cout << "> trying option" << idx << endl;
                copied_target[idx] = min_index.value();
                Target new_target = _find_column_swap(copied_target);
                if (!new_target.empty())
                    return new_target;
            }
            if (VERBOSE >= 8) cout << "Unsuccessful" << endl;
            return target;
        }
    }
}

/**
 * @brief Perform elimination on biadjacency matrix
 *
 * @param check if true, check the frontier is clean and no axels connecting to frontier
 * @return true if check pass
 * @return false if not
 */
bool Extractor::biadjacency_eliminations(bool check) {
    if (check) {
        if (!frontier_is_cleaned()) {
            cout << "Note: frontier is dirty, please clean it first." << endl;
            return false;
        }
        if (axel_in_neighbors()) {
            cout << "Note: axel(s) are in the neighbors, please remove gadget(s) first." << endl;
            return false;
        }
    }

    if (SORT_FRONTIER == true) {
        _frontier.sort([](ZXVertex const* a, ZXVertex const* b) {
            return a->get_qubit() < b->get_qubit();
        });
    }
    if (SORT_NEIGHBORS == true) {
        // REVIEW - Do not know why sort in here would be better
        _neighbors.sort([](ZXVertex const* a, ZXVertex const* b) {
            return a->get_id() < b->get_id();
        });
    }

    vector<BooleanMatrix::RowOperation> greedy_opers;

    _biadjacency.from_zxvertices(_frontier, _neighbors);
    BooleanMatrix greedy_mat = _biadjacency;
    ZXVertexList backup_neighbors = _neighbors;
    if (OPTIMIZE_LEVEL > 1) {
        // NOTE - opt = 2 or 3
        greedy_opers = greedy_reduction(greedy_mat);
        for (auto oper : greedy_opers) {
            greedy_mat.row_operation(oper.first, oper.second, true);
        }
    }

    if (OPTIMIZE_LEVEL != 2) {
        // NOTE - opt = 0, 1 or 3
        column_optimal_swap();
        _biadjacency.from_zxvertices(_frontier, _neighbors);

        if (OPTIMIZE_LEVEL == 0) {
            _biadjacency.gaussian_elimination_skip(BLOCK_SIZE, true, true);
            if (FILTER_DUPLICATED_CXS) {
                size_t old = _num_cx_filtered;
                while (true) {
                    size_t reduce = _biadjacency.filter_duplicate_row_operations();
                    _num_cx_filtered += reduce;
                    if (reduce == 0) break;
                }
                if (VERBOSE >= 4) cout << "Filter " << _num_cx_filtered - old << " CXs. Total: " << _num_cx_filtered << endl;
            }
            _cnots = _biadjacency.get_row_operations();
        } else if (OPTIMIZE_LEVEL == 1 || OPTIMIZE_LEVEL == 3) {
            size_t min_cnots = size_t(-1);
            BooleanMatrix best_matrix;
            for (size_t blk = 1; blk < _biadjacency.num_cols(); blk++) {
                _block_elimination(best_matrix, min_cnots, blk);
            }
            if (OPTIMIZE_LEVEL == 1) {
                _biadjacency = best_matrix;
                _cnots = _biadjacency.get_row_operations();
            } else {
                size_t n_gauss_opers = best_matrix.get_row_operations().size();
                size_t n_single_one_rows = accumulate(best_matrix.get_matrix().begin(), best_matrix.get_matrix().end(), 0,
                                                      [](size_t acc, Row const& r) { return acc + size_t(r.is_one_hot()); });
                // NOTE - opers per extractable rows for Gaussian is bigger than greedy
                bool select_greedy = float(n_gauss_opers) / float(n_single_one_rows) > float(greedy_opers.size()) - 0.1;
                if (!greedy_opers.empty() && select_greedy) {
                    _biadjacency = greedy_mat;
                    _cnots = greedy_mat.get_row_operations();
                    _neighbors = backup_neighbors;
                    if (VERBOSE > 3) cout << "Found greedy reduction with " << _cnots.size() << " CX(s)" << endl;
                } else {
                    _biadjacency = best_matrix;
                    _cnots = _biadjacency.get_row_operations();
                    if (VERBOSE > 3) cout << "Gaussian elimination with " << _cnots.size() << " CX(s)" << endl;
                }
            }
        } else {
            cerr << "Error: wrong optimize level" << endl;
            abort();
        }

    } else {
        // NOTE - OPT level 2
        _biadjacency = greedy_mat;
        _cnots = greedy_mat.get_row_operations();
    }

    // TODO - update matrix to correct one
    return true;
}

/**
 * @brief Perform Gaussian Elimination with block size `blockSize`
 *
 * @param bestMatrix Currently best matrix
 * @param minCnots Minimum value
 * @param blockSize
 */
void Extractor::_block_elimination(BooleanMatrix& best_matrix, size_t& min_n_cxs, size_t block_size) {
    BooleanMatrix copied_matrix = _biadjacency;
    copied_matrix.gaussian_elimination_skip(block_size, true, true);
    if (FILTER_DUPLICATED_CXS) {
        while (true) {
            size_t reduce = copied_matrix.filter_duplicate_row_operations();
            _num_cx_filtered += reduce;
            if (reduce == 0) break;
        }
    }
    if (copied_matrix.get_row_operations().size() < min_n_cxs) {
        min_n_cxs = copied_matrix.get_row_operations().size();
        best_matrix = copied_matrix;
    }
}

void Extractor::_block_elimination(size_t& best_block, BooleanMatrix& best_matrix, size_t& min_cost, size_t block_size) {
    BooleanMatrix copied_matrix = _biadjacency;
    copied_matrix.gaussian_elimination_skip(block_size, true, true);
    if (FILTER_DUPLICATED_CXS) {
        while (true) {
            size_t reduce = copied_matrix.filter_duplicate_row_operations();
            _num_cx_filtered += reduce;
            if (reduce == 0) break;
        }
    }

    // NOTE - Construct Duostra Input
    unordered_map<size_t, ZXVertex*> front_id2_vertex;
    size_t cnt = 0;
    for (auto& f : _frontier) {
        front_id2_vertex[cnt] = f;
        cnt++;
    }
    vector<Operation> ops;
    for (auto& [t, c] : copied_matrix.get_row_operations()) {
        // NOTE - targ and ctrl are opposite here
        size_t ctrl = _qubit_map[front_id2_vertex[c]->get_qubit()];
        size_t targ = _qubit_map[front_id2_vertex[t]->get_qubit()];
        if (VERBOSE > 4) cout << "Add CX: " << ctrl << " " << targ << endl;
        Operation op(GateType::cx, Phase(0), {ctrl, targ}, {});
        ops.emplace_back(op);
    }

    // NOTE - Get Mapping result, Device is passed by copy
    Duostra duo(ops, _graph->get_num_outputs(), _device.value(), {.verifyResult = false, .silent = true, .useTqdm = false});
    size_t depth = duo.flow(true);
    if (VERBOSE > 4) cout << block_size << ", depth:" << depth << ", #cx: " << ops.size() << endl;
    if (depth < min_cost) {
        min_cost = depth;
        best_matrix = copied_matrix;
        best_block = block_size;
    }
}

/**
 * @brief Permute qubit if input and output are not match
 *
 */
void Extractor::permute_qubits() {
    if (VERBOSE >= 4) cout << "Permute Qubit" << endl;
    // REVIEW - ordered_hashmap?
    unordered_map<size_t, size_t> swap_map;      // o to i
    unordered_map<size_t, size_t> swap_inv_map;  // i to o
    bool matched = true;
    for (auto& o : _graph->get_outputs()) {
        if (o->get_num_neighbors() != 1) {
            cout << "Note: output is not connected to only one vertex" << endl;
            return;
        }
        ZXVertex* i = o->get_first_neighbor().first;
        if (!_graph->get_inputs().contains(i)) {
            cout << "Note: output is not connected to input" << endl;
            return;
        }
        if (i->get_qubit() != o->get_qubit()) {
            matched = false;
        }
        swap_map[o->get_qubit()] = i->get_qubit();
    }

    if (matched) return;

    for (auto& [o, i] : swap_map) {
        swap_inv_map[i] = o;
    }
    for (auto& [o, i] : swap_map) {
        if (o == i) continue;
        size_t t2 = swap_inv_map.at(o);
        prepend_swap_gate(_qubit_map[o], _qubit_map[t2], _logical_circuit);
        swap_map[t2] = i;
        swap_inv_map[i] = t2;
    }
    for (auto& o : _graph->get_outputs()) {
        _graph->remove_all_edges_between(o->get_first_neighbor().first, o);
    }
    for (auto& o : _graph->get_outputs()) {
        for (auto& i : _graph->get_inputs()) {
            if (o->get_qubit() == i->get_qubit()) {
                _graph->add_edge(o, i, EdgeType::simple);
                break;
            }
        }
    }
}

/**
 * @brief Update neighbors according to frontier
 *
 */
void Extractor::update_neighbors() {
    _neighbors.clear();
    vector<ZXVertex*> rm_vs;

    for (auto& f : _frontier) {
        size_t num_boundaries = count_if(
            f->get_neighbors().begin(),
            f->get_neighbors().end(),
            [](NeighborPair const& nbp) { return nbp.first->is_boundary(); });

        if (num_boundaries != 2) continue;

        if (f->get_num_neighbors() == 2) {
            // NOTE - Remove
            for (auto& [b, ep] : f->get_neighbors()) {
                if (_graph->get_inputs().contains(b)) {
                    if (ep == EdgeType::hadamard) {
                        prepend_single_qubit_gate("h", _qubit_map[f->get_qubit()], Phase(0));
                    }
                    break;
                }
            }
            rm_vs.emplace_back(f);
        } else {
            for (auto [b, ep] : f->get_neighbors()) {  // The pass-by-copy is deliberate. Pass by ref will cause segfault
                if (_graph->get_inputs().contains(b)) {
                    _graph->add_buffer(b, f, ep);
                    break;
                }
            }
        }
    }

    for (auto& v : rm_vs) {
        if (VERBOSE >= 8) cout << "Remove " << v->get_id() << " (q" << v->get_qubit() << ") from frontiers." << endl;
        _frontier.erase(v);
        _graph->add_edge(v->get_first_neighbor().first, v->get_second_neighbor().first, EdgeType::simple);
        _graph->remove_vertex(v);
    }

    for (auto& f : _frontier) {
        for (auto& [n, _] : f->get_neighbors()) {
            if (!n->is_boundary() && !_frontier.contains(n))
                _neighbors.emplace(n);
        }
    }
}

/**
 * @brief Update graph according to bi-adjacency matrix
 *
 * @param et EdgeType, default: EdgeType::HADAMARD
 */
void Extractor::update_graph_by_matrix(EdgeType et) {
    size_t r = 0;
    if (VERBOSE >= 4) cout << "Update Graph by Matrix" << endl;
    for (auto& f : _frontier) {
        size_t c = 0;
        for (auto& nb : _neighbors) {
            if (_biadjacency[r][c] == 1 && !f->is_neighbor(nb)) {  // NOTE - Should connect but not connected
                _graph->add_edge(f, nb, et);
            } else if (_biadjacency[r][c] == 0 && f->is_neighbor(nb)) {  // NOTE - Should not connect but connected
                _graph->remove_edge(f, nb, et);
            }
            c++;
        }
        r++;
    }
}

/**
 * @brief Create bi-adjacency matrix fron frontier and neighbors
 *
 */
void Extractor::create_matrix() {
    _biadjacency.from_zxvertices(_frontier, _neighbors);
}

/**
 * @brief Prepend single-qubit gate to circuit. If _device is given, directly map to physical device.
 *
 * @param type
 * @param qubit logical
 * @param phase
 */
void Extractor::prepend_single_qubit_gate(string type, size_t qubit, Phase phase) {
    if (type == "rotate") {
        _logical_circuit->add_single_rz(qubit, phase, false);
        // if (toPhysical()) {
        //     size_t physicalQId = _device.value().getPhysicalbyLogical(qubit);
        //     assert(physicalQId != SIZE_MAX);
        //     _device.value().applySingleQubitGate(physicalQId);
        //     _physicalCircuit->addSingleRZ(physicalQId, phase, false);
        // }
    } else {
        _logical_circuit->add_gate(type, {qubit}, phase, false);
        // if (toPhysical()) {
        //     size_t physicalQId = _device.value().getPhysicalbyLogical(qubit);
        //     assert(physicalQId != SIZE_MAX);
        //     _device.value().applySingleQubitGate(physicalQId);
        //     _physicalCircuit->addGate(type, {physicalQId}, phase, false);
        // }
    }
    // if (toPhysical()) _device.value().printStatus();
}

/**
 * @brief Prepend double-qubit gate to circuit. If _device is given, directly map to physical device.
 *
 * @param type
 * @param qubits
 * @param phase
 */
void Extractor::prepend_double_qubit_gate(string type, vector<size_t> const& qubits, Phase phase) {
    assert(qubits.size() == 2);
    _logical_circuit->add_gate(type, qubits, phase, false);
}

/**
 * @brief Prepend series of gates.
 *
 * @param logical
 * @param physical
 */
void Extractor::prepend_series_gates(std::vector<Operation> const& logical, std::vector<Operation> const& physical) {
    for (auto const& gates : logical) {
        tuple<size_t, size_t> qubits = gates.get_qubits();
        _logical_circuit->add_gate(GATE_TYPE_TO_STR[gates.get_type()], {get<0>(qubits), get<1>(qubits)}, gates.get_phase(), false);
    }

    for (auto const& gates : physical) {
        tuple<size_t, size_t> qubits = gates.get_qubits();
        if (gates.get_type() == GateType::swap) {
            prepend_swap_gate(get<0>(qubits), get<1>(qubits), _physical_circuit);
            _num_swaps++;
        } else
            _physical_circuit->add_gate(GATE_TYPE_TO_STR[gates.get_type()], {get<0>(qubits), get<1>(qubits)}, gates.get_phase(), false);
    }
}

/**
 * @brief Prepend swap gate. Decompose into three CXs
 *
 * @param q0 logical
 * @param q1 logical
 * @param circuit
 */
void Extractor::prepend_swap_gate(size_t q0, size_t q1, QCir* circuit) {
    // NOTE - No qubit permutation in Physical Circuit
    circuit->add_gate("cx", {q0, q1}, Phase(0), false);
    circuit->add_gate("cx", {q1, q0}, Phase(0), false);
    circuit->add_gate("cx", {q0, q1}, Phase(0), false);
}

/**
 * @brief Check whether the frontier is clean
 *
 * @return true if clean
 * @return false if dirty
 */
bool Extractor::frontier_is_cleaned() {
    for (auto& f : _frontier) {
        if (f->get_phase() != Phase(0)) return false;
        for (auto& [n, e] : f->get_neighbors()) {
            if (_frontier.contains(n)) return false;
            if (_graph->get_outputs().contains(n) && e == EdgeType::hadamard) return false;
        }
    }
    return true;
}

/**
 * @brief Check any axel in neighbors
 *
 * @return true
 * @return false
 */
bool Extractor::axel_in_neighbors() {
    for (auto& n : _neighbors) {
        if (_axels.contains(n)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check whether the frontier contains a vertex has only a single neighbor (boundary excluded).
 *
 * @return true
 * @return false
 */
bool Extractor::contains_single_neighbor() {
    for (auto& f : _frontier) {
        if (f->get_num_neighbors() == 2)
            return true;
    }
    return false;
}

/**
 * @brief Print frontier
 *
 */
void Extractor::print_frontier() {
    cout << "Frontier:" << endl;
    for (auto& f : _frontier)
        cout << "Qubit:" << f->get_qubit() << ": " << f->get_id() << endl;
    cout << endl;
}

/**
 * @brief Print neighbors
 *
 */
void Extractor::print_neighbors() {
    cout << "Neighbors:" << endl;
    for (auto& n : _neighbors)
        cout << n->get_id() << endl;
    cout << endl;
}

/**
 * @brief Print axels
 *
 */
void Extractor::print_axels() {
    cout << "Axels:" << endl;
    for (auto& n : _axels) {
        cout << n->get_id() << " (phase gadget: ";
        for (auto& [pg, _] : n->get_neighbors()) {
            if (_graph->is_gadget_leaf(pg))
                cout << pg->get_id() << ")" << endl;
        }
    }
    cout << endl;
}

void Extractor::print_cxs() {
    cout << "CXs: " << endl;
    for (auto& [c, t] : _cnots) {
        cout << "(" << c << ", " << t << ")  ";
    }
    cout << endl;
}
