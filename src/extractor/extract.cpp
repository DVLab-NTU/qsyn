/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./extract.hpp"

#include <cassert>
#include <memory>
#include <ranges>
#include <tuple>

#include "duostra/duostra.hpp"
#include "duostra/mapping_eqv_checker.hpp"
#include "qcir/qcir.hpp"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include "util/boolean_matrix.hpp"
#include "util/util.hpp"
#include "zx/simplifier/simplify.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;
using namespace qsyn::qcir;

extern bool stop_requested();

namespace qsyn::extractor {

bool SORT_FRONTIER        = false;
bool SORT_NEIGHBORS       = true;
bool PERMUTE_QUBITS       = true;
bool FILTER_DUPLICATE_CXS = true;
size_t BLOCK_SIZE         = 5;
size_t OPTIMIZE_LEVEL     = 2;

/**
 * @brief Construct a new Extractor:: Extractor object
 *
 * @param g
 * @param c
 * @param d
 */
Extractor::Extractor(ZXGraph* g, QCir* c, std::optional<Device> const& d) : _graph(g), _logical_circuit{c ? c : new QCir()}, _physical_circuit{to_physical() ? new QCir() : nullptr}, _device(d), _device_backup(d) {
    initialize(c == nullptr);
}

/**
 * @brief Initialize the extractor. Set ZXGraph to QCir qubit map.
 *
 */
void Extractor::initialize(bool from_empty_qcir) {
    spdlog::debug("Initializing extractor");

    QubitIdType cnt = 0;
    for (auto& o : _graph->get_outputs()) {
        ZXVertex* neighbor_to_output = _graph->get_first_neighbor(o).first;
        if (!neighbor_to_output->is_boundary()) {
            neighbor_to_output->set_qubit(o->get_qubit());
            _frontier.emplace(neighbor_to_output);
        }
        _qubit_map[o->get_qubit()] = cnt;
        if (from_empty_qcir)
            _logical_circuit->add_qubits(1);
        cnt++;
    }

    // NOTE - get zx to qc qubit mapping
    _frontier.sort([](ZXVertex const* a, ZXVertex const* b) {
        return a->get_qubit() < b->get_qubit();
    });

    update_neighbors();
    for (auto& v : _graph->get_vertices()) {
        if (_graph->is_gadget_leaf(v)) {
            _axels.emplace(_graph->get_first_neighbor(v).first);
        }
    }
    print_frontier(spdlog::level::level_enum::trace);
    print_neighbors(spdlog::level::level_enum::trace);
    _graph->print_vertices_by_qubits(spdlog::level::level_enum::trace);
    _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
}

/**
 * @brief Extract the graph into circuit
 *
 * @return QCir*
 */
QCir* Extractor::extract() {
    if (_graph->is_empty()) {
        spdlog::error("The ZXGraph is empty!!");
        return nullptr;
    }
    if (!extraction_loop(-1)) {
        return nullptr;
    }
    if (stop_requested()) {
        spdlog::warn("Conversion is interrupted");
        return nullptr;
    }

    spdlog::info("Finished Extracting!");
    _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
    _graph->print_vertices_by_qubits(spdlog::level::level_enum::trace);

    if (PERMUTE_QUBITS) {
        permute_qubits();
        _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
        _graph->print_vertices_by_qubits(spdlog::level::level_enum::trace);
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
bool Extractor::extraction_loop(std::optional<size_t> max_iter) {
    while ((!max_iter.has_value() || *max_iter > 0) && !stop_requested()) {
        clean_frontier();
        update_neighbors();

        if (_frontier.empty()) break;

        if (remove_gadget()) {
            spdlog::debug("Gadget(s) are removed.");
            print_frontier(spdlog::level::level_enum::trace);
            _graph->print_vertices_by_qubits(spdlog::level::level_enum::trace);
            _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
            continue;
        }

        if (contains_single_neighbor()) {
            spdlog::debug("Single neighbor found. Construct an easy matrix.");
            update_matrix();
        } else {
            spdlog::debug("Perform Gaussian elimination.");
            extract_cxs();
        }
        if (extract_hadamards_from_matrix() == 0) {
            spdlog::error("No hadamard gates to extract from the matrix!!");

            _biadjacency.print_matrix(spdlog::level::level_enum::err);
            return false;
        }
        _biadjacency.reset();
        _cnots.clear();

        print_frontier(spdlog::level::level_enum::trace);
        print_neighbors(spdlog::level::level_enum::trace);
        _graph->print_vertices_by_qubits(spdlog::level::level_enum::trace);
        _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);

        if (max_iter.has_value()) (*max_iter)--;
    }
    return true;
}

/**
 * @brief Clean frontier. Contain extract singles and CZs. Used in extract.
 *
 */
void Extractor::clean_frontier() {
    spdlog::debug("Cleaning frontier");
    // NOTE - Edge and dvlab::Phase
    extract_singles();
    // NOTE - CZs
    extract_czs();
}

/**
 * @brief Extract single qubit gates, i.e. z-rotate family and H. Used in clean frontier.
 *
 */
void Extractor::extract_singles() {
    spdlog::debug("Extracting single qubit gates");
    std::vector<std::pair<ZXVertex*, ZXVertex*>> toggle_list;
    for (ZXVertex* o : _graph->get_outputs()) {
        if (_graph->get_first_neighbor(o).second == EdgeType::hadamard) {
            prepend_single_qubit_gate("h", _qubit_map[o->get_qubit()], dvlab::Phase(0));
            toggle_list.emplace_back(o, _graph->get_first_neighbor(o).first);
        }
        auto const ph = _graph->get_first_neighbor(o).first->get_phase();
        if (ph != dvlab::Phase(0)) {
            prepend_single_qubit_gate("rotate", _qubit_map[o->get_qubit()], ph);
            _graph->get_first_neighbor(o).first->set_phase(dvlab::Phase(0));
        }
    }
    for (auto& [s, t] : toggle_list) {
        _graph->add_edge(s, t, EdgeType::simple);
        _graph->remove_edge(s, t, EdgeType::hadamard);
    }
    _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
    _graph->print_vertices_by_qubits(spdlog::level::level_enum::trace);
}

/**
 * @brief Extract CZs from frontier. Used in clean frontier.
 *
 * @param check check no phase in the frontier if true
 * @return true
 * @return false
 */
bool Extractor::extract_czs(bool check) {
    spdlog::debug("Extracting CZs");

    if (check) {
        for (auto& f : _frontier) {
            if (f->get_phase() != dvlab::Phase(0)) {
                spdlog::error("Phase found in frontier!! Please extract them first");
                return false;
            }
            for (auto& [n, e] : _graph->get_neighbors(f)) {
                if (_graph->get_outputs().contains(n) && e == EdgeType::hadamard) {
                    spdlog::error("Hadamard edge found in frontier!! Please extract them first");
                    return false;
                }
            }
        }
    }

    std::vector<std::pair<ZXVertex*, ZXVertex*>> remove_list;

    for (auto itr = _frontier.begin(); itr != _frontier.end(); itr++) {
        for (auto jtr = next(itr); jtr != _frontier.end(); jtr++) {
            if (_graph->is_neighbor(*itr, *jtr, EdgeType::hadamard)) {
                remove_list.emplace_back((*itr), (*jtr));
            }
        }
    }
    std::vector<Operation> ops;
    for (auto const& [s, t] : remove_list) {
        _graph->remove_edge(s, t, EdgeType::hadamard);
        ops.emplace_back(GateRotationCategory::pz, dvlab::Phase(1), std::make_tuple(_qubit_map[s->get_qubit()], _qubit_map[t->get_qubit()]), std::make_tuple(0, 0));
    }
    if (ops.size() > 0) {
        prepend_series_gates(ops);
    }
    _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
    _graph->print_vertices_by_qubits(spdlog::level::level_enum::trace);

    return true;
}

/**
 * @brief Extract CXs
 *
 */
void Extractor::extract_cxs() {
    _num_cx_iterations++;
    biadjacency_eliminations();
    update_graph_by_matrix();
    spdlog::debug("Extracting CXs");
    std::unordered_map<size_t, ZXVertex*> front_id2_vertex;
    size_t cnt = 0;
    for (auto& f : _frontier) {
        front_id2_vertex[cnt] = f;
        cnt++;
    }

    for (auto& [t, c] : _cnots) {
        // NOTE - targ and ctrl are opposite here
        auto ctrl = _qubit_map[front_id2_vertex[c]->get_qubit()];
        auto targ = _qubit_map[front_id2_vertex[t]->get_qubit()];
        spdlog::debug("Adding CX: {} {}", ctrl, targ);
        prepend_double_qubit_gate("cx", {ctrl, targ}, dvlab::Phase(0));
    }
}

/**
 * @brief Extract Hadamard if singly connected vertex in frontier is found
 *
 * @param check if true, check frontier is cleaned and axels not connected to frontiers
 * @return size_t
 */
size_t Extractor::extract_hadamards_from_matrix(bool check) {
    spdlog::debug("Extracting Hadamards from matrix");

    if (check) {
        if (!frontier_is_cleaned()) {
            spdlog::error("Frontier is dirty!! Please clean it first.");
            return 0;
        }
        if (axel_in_neighbors()) {
            spdlog::error("Axel(s) are in the neighbors!! Please remove gadget(s) first.");
            return 0;
        }
        update_matrix();
    }

    std::unordered_map<size_t, ZXVertex*> frontier_id_to_vertex;
    std::unordered_map<size_t, ZXVertex*> neighbor_id_to_vertex;
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
    std::vector<std::pair<ZXVertex*, ZXVertex*>> front_neigh_pairs;

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
        prepend_single_qubit_gate("h", _qubit_map[f->get_qubit()], dvlab::Phase(0));
        // NOTE - Set #qubit and #col according to the old frontier
        n->set_qubit(f->get_qubit());
        n->set_col(f->get_col());

        // NOTE - Connect edge between boundary and neighbor
        for (auto& [bound, ep] : _graph->get_neighbors(f)) {
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
        spdlog::error("No candidate found!!");
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
    spdlog::debug("Removing gadget(s)");

    if (check) {
        if (_frontier.empty()) {
            spdlog::error("no vertex left in the frontier!!");
            return false;
        }
        if (!frontier_is_cleaned()) {
            spdlog::error("frontier is dirty!! Please clean it first.");
            return false;
        }
    }

    _graph->print_graph(spdlog::level::level_enum::trace);
    print_frontier(spdlog::level::level_enum::debug);
    print_axels(spdlog::level::level_enum::debug);

    bool removed_some_gadgets = false;
    for (auto& n : _neighbors) {
        if (!_axels.contains(n)) {
            continue;
        }
        for (auto& [candidate, _] : _graph->get_neighbors(n)) {
            if (_frontier.contains(candidate)) {
                _axels.erase(n);
                _frontier.erase(candidate);

                ZXVertex* target_boundary = nullptr;
                for (auto& [boundary, _] : _graph->get_neighbors(candidate)) {
                    if (boundary->is_boundary()) {
                        target_boundary = boundary;
                        break;
                    }
                }

                PivotBoundaryRule().apply(*_graph, {{candidate, n}});

                assert(target_boundary != nullptr);
                _frontier.emplace(_graph->get_first_neighbor(target_boundary).first);
                // REVIEW - qubit_map
                removed_some_gadgets = true;
                break;
            }
        }
    }
    _graph->print_vertices(spdlog::level::level_enum::trace);
    print_frontier(spdlog::level::level_enum::debug);
    print_axels(spdlog::level::level_enum::debug);

    return removed_some_gadgets;
}

/**
 * @brief Swap columns (order of neighbors) to put the most of them on the diagonal of the bi-adjacency matrix
 *
 */
void Extractor::column_optimal_swap() {
    // NOTE - Swap columns of matrix and order of neighbors

    _row_info.clear();
    _col_info.clear();

    auto const row_cnt = _biadjacency.num_rows();
    auto const col_cnt = _biadjacency.num_cols();

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
    std::vector<size_t> lvec(left.begin(), left.end());
    std::vector<size_t> rvec(right.begin(), right.end());
    for (size_t i = 0; i < lvec.size(); i++) {
        target[rvec[i]] = lvec[i];
    }
    Target perm;
    for (auto& [k, v] : target) {
        perm[v] = k;
    }
    std::vector<ZXVertex*> neb_vec(_neighbors.begin(), _neighbors.end());
    std::vector<ZXVertex*> new_neb_vec = neb_vec;
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
    auto const row_cnt = _row_info.size();

    std::set<size_t> claimed_cols;
    std::set<size_t> claimed_rows;
    for (auto& [key, value] : target) {
        claimed_cols.emplace(key);
        claimed_rows.emplace(value);
    }

    while (true) {
        std::optional<size_t> min_index = std::nullopt;
        std::set<size_t> min_options;
        for (size_t i = 0; i < 1000; i++) min_options.emplace(i);
        bool found_col = false;
        for (size_t i = 0; i < row_cnt; i++) {
            if (claimed_rows.contains(i)) continue;
            std::set<size_t> free_cols;
            // NOTE - find the free columns
            set_difference(_row_info[i].begin(), _row_info[i].end(), claimed_cols.begin(), claimed_cols.end(), inserter(free_cols, free_cols.end()));
            if (free_cols.size() == 1) {
                // NOTE - pop the only element
                auto const j = *(free_cols.begin());
                free_cols.erase(j);

                target[j] = i;
                claimed_cols.emplace(j);
                claimed_rows.emplace(i);
                found_col = true;
                break;
            }

            if (free_cols.size() == 0) {
                spdlog::debug("No free column for column optimal swap!!");
                return {};  // NOTE - Contradiction
            }

            for (auto& j : free_cols) {
                std::set<size_t> free_rows;
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
                min_index   = i;
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
            DVLAB_ASSERT(min_index != std::nullopt, "Min index should always exist");
            // NOTE -  depth-first search
            Target copied_target = target;
            spdlog::trace("Backtracking on {}", min_index.value());

            for (auto& idx : min_options) {
                spdlog::trace("Trying option {}", idx);
                copied_target[idx] = min_index.value();
                Target new_target  = _find_column_swap(copied_target);
                if (!new_target.empty())
                    return new_target;
            }
            spdlog::trace("Backtracking failed");
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
void Extractor::_filter_duplicate_cxs() {
    auto const old = _num_cx_filtered;
    while (true) {
        auto const reduce = _biadjacency.filter_duplicate_row_operations();
        if (reduce == 0) break;
        _num_cx_filtered += reduce;
    }
    spdlog::debug("Filter {} CXs. Total: {}", _num_cx_filtered - old, _num_cx_filtered);
}

bool Extractor::biadjacency_eliminations(bool check) {
    if (check) {
        if (!frontier_is_cleaned()) {
            spdlog::error("Frontier is dirty!! Please clean it first.");
            return false;
        }
        if (axel_in_neighbors()) {
            spdlog::error("Axel(s) are in the neighbors!! Please remove gadget(s) first.");
            return false;
        }
    }

    if (SORT_FRONTIER == true) {
        _frontier.sort([](ZXVertex const* a, ZXVertex const* b) {
            return a->get_qubit() < b->get_qubit();
        });
    }
    if (SORT_NEIGHBORS == true) {
        // REVIEW - Do not know why sort here would be better
        _neighbors.sort([](ZXVertex const* a, ZXVertex const* b) {
            return a->get_id() < b->get_id();
        });
    }

    std::vector<dvlab::BooleanMatrix::RowOperation> greedy_opers;

    update_matrix();
    dvlab::BooleanMatrix greedy_matrix = _biadjacency;
    auto const backup_neighbors        = _neighbors;

    DVLAB_ASSERT(OPTIMIZE_LEVEL <= 3, "Error: wrong optimize level");

    if (OPTIMIZE_LEVEL > 1) {
        // NOTE - opt = 2 or 3
        greedy_opers = greedy_reduction(greedy_matrix);
        for (auto oper : greedy_opers) {
            greedy_matrix.row_operation(oper.first, oper.second, true);
        }
    }

    if (OPTIMIZE_LEVEL != 2) {
        // NOTE - opt = 0, 1 or 3
        column_optimal_swap();
        update_matrix();

        if (OPTIMIZE_LEVEL == 0) {
            _biadjacency.gaussian_elimination_skip(BLOCK_SIZE, true, true);
            if (FILTER_DUPLICATE_CXS) _filter_duplicate_cxs();
            _cnots = _biadjacency.get_row_operations();
        } else if (OPTIMIZE_LEVEL == 1 || OPTIMIZE_LEVEL == 3) {
            auto min_cnots = SIZE_MAX;
            dvlab::BooleanMatrix best_matrix;
            for (size_t blk = 1; blk < _biadjacency.num_cols(); blk++) {
                _block_elimination(best_matrix, min_cnots, blk);
            }
            if (OPTIMIZE_LEVEL == 1) {
                _biadjacency = best_matrix;
                _cnots       = _biadjacency.get_row_operations();
            } else {
                auto const n_gauss_opers     = best_matrix.get_row_operations().size();
                auto const n_single_one_rows = accumulate(best_matrix.get_matrix().begin(), best_matrix.get_matrix().end(), 0,
                                                          [](size_t acc, dvlab::BooleanMatrix::Row const& r) { return acc + size_t(r.is_one_hot()); });
                // NOTE - opers per extractable rows for Gaussian is bigger than greedy
                auto const found_greedy = (float(n_gauss_opers) / float(n_single_one_rows)) > (float(greedy_opers.size()) - 0.1);
                if (!greedy_opers.empty() && found_greedy) {
                    _biadjacency = greedy_matrix;
                    _cnots       = greedy_matrix.get_row_operations();
                    _neighbors   = backup_neighbors;
                    spdlog::debug("Found greedy reduction with {} CXs", _cnots.size());
                } else {
                    _biadjacency = best_matrix;
                    _cnots       = _biadjacency.get_row_operations();
                    spdlog::debug("Found Gaussian elimination with {} CXs", _cnots.size());
                }
            }
        }
    } else {
        // NOTE - OPT level 2
        _biadjacency = greedy_matrix;
        _cnots       = greedy_matrix.get_row_operations();
    }

    return true;
}

/**
 * @brief Perform Gaussian Elimination with block size `blockSize`
 *
 * @param bestMatrix Currently best matrix
 * @param minCnots Minimum value
 * @param blockSize
 */
void Extractor::_block_elimination(dvlab::BooleanMatrix& best_matrix, size_t& min_n_cxs, size_t block_size) {
    dvlab::BooleanMatrix copied_matrix = _biadjacency;
    copied_matrix.gaussian_elimination_skip(block_size, true, true);
    if (FILTER_DUPLICATE_CXS) _filter_duplicate_cxs();
    if (copied_matrix.get_row_operations().size() < min_n_cxs) {
        min_n_cxs   = copied_matrix.get_row_operations().size();
        best_matrix = copied_matrix;
    }
}

void Extractor::_block_elimination(size_t& best_block, dvlab::BooleanMatrix& best_matrix, size_t& min_cost, size_t block_size) {
    dvlab::BooleanMatrix copied_matrix = _biadjacency;
    copied_matrix.gaussian_elimination_skip(block_size, true, true);
    if (FILTER_DUPLICATE_CXS) _filter_duplicate_cxs();

    // NOTE - Construct Duostra Input
    std::unordered_map<size_t, ZXVertex*> front_id2_vertex;
    size_t cnt = 0;
    for (auto& f : _frontier) {
        front_id2_vertex[cnt] = f;
        cnt++;
    }
    std::vector<Operation> ops;
    for (auto& [t, c] : copied_matrix.get_row_operations()) {
        // NOTE - targ and ctrl are opposite here
        size_t ctrl = _qubit_map[front_id2_vertex[c]->get_qubit()];
        size_t targ = _qubit_map[front_id2_vertex[t]->get_qubit()];
        spdlog::debug("Adding CX: {} {}", ctrl, targ);
        ops.emplace_back(GateRotationCategory::px, dvlab::Phase(0), std::make_tuple(ctrl, targ), std::make_tuple(0, 0));
    }

    // NOTE - Get Mapping result, Device is passed by copy
    qsyn::duostra::Duostra duo(ops, _graph->get_num_outputs(), _device.value(), {.verify_result = false, .silent = true, .use_tqdm = false});
    size_t depth = duo.map(true);
    spdlog::debug("Block size: {}, depth: {}, #cx: {}", block_size, depth, ops.size());
    if (depth < min_cost) {
        min_cost    = depth;
        best_matrix = copied_matrix;
        best_block  = block_size;
    }
}

/**
 * @brief Permute qubit if input and output are not match
 *
 */
void Extractor::permute_qubits() {
    spdlog::debug("Permuting qubits");
    std::unordered_map<QubitIdType, QubitIdType> swap_map;      // o to i
    std::unordered_map<QubitIdType, QubitIdType> swap_inv_map;  // i to o
    bool matched = true;
    for (auto& o : _graph->get_outputs()) {
        if (_graph->get_num_neighbors(o) != 1) {
            spdlog::error("Output is not connected to only one vertex!!");
            return;
        }
        ZXVertex* i = _graph->get_first_neighbor(o).first;
        if (!_graph->get_inputs().contains(i)) {
            spdlog::error("Output is not connected to input!!");
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
        auto t2 = swap_inv_map.at(o);
        prepend_swap_gate(_qubit_map[o], _qubit_map[t2], _logical_circuit);
        swap_map[t2]    = i;
        swap_inv_map[i] = t2;
    }
    for (auto& o : _graph->get_outputs()) {
        _graph->remove_all_edges_between(_graph->get_first_neighbor(o).first, o);
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
    std::vector<ZXVertex*> rm_vs;

    for (auto& f : _frontier) {
        auto const num_boundaries = std::ranges::count_if(
            _graph->get_neighbors(f),
            [](NeighborPair const& nbp) { return nbp.first->is_boundary(); });

        if (num_boundaries != 2) continue;

        if (_graph->get_num_neighbors(f) == 2 && f->get_phase() == dvlab::Phase(0)) {
            // NOTE - Remove
            for (auto& [b, ep] : _graph->get_neighbors(f)) {
                if (_graph->get_inputs().contains(b)) {
                    if (ep == EdgeType::hadamard) {
                        prepend_single_qubit_gate("h", _qubit_map[f->get_qubit()], dvlab::Phase(0));
                    }
                    break;
                }
            }
            rm_vs.emplace_back(f);
        } else {
            for (auto [b, ep] : _graph->get_neighbors(f)) {  // The pass-by-copy is deliberate. Pass by ref will cause segfault
                if (_graph->get_inputs().contains(b)) {
                    _graph->add_buffer(b, f, ep);
                    break;
                }
            }
        }
    }

    for (auto& v : rm_vs) {
        spdlog::trace("Remove {} from frontier", v->get_id());
        _frontier.erase(v);
        _graph->add_edge(_graph->get_first_neighbor(v).first, _graph->get_second_neighbor(v).first, EdgeType::simple);
        _graph->remove_vertex(v);
    }

    for (auto& f : _frontier) {
        for (auto& [n, _] : _graph->get_neighbors(f)) {
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
    spdlog::debug("Updating graph by matrix");
    for (auto& f : _frontier) {
        size_t c = 0;
        for (auto& nb : _neighbors) {
            if (_biadjacency[r][c] == 1 && !_graph->is_neighbor(nb, f, et)) {  // NOTE - Should connect but not connected
                _graph->add_edge(f, nb, et);
            } else if (_biadjacency[r][c] == 0 && _graph->is_neighbor(nb, f, et)) {  // NOTE - Should not connect but connected
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
void Extractor::update_matrix() {
    _biadjacency = get_biadjacency_matrix(*_graph, _frontier, _neighbors);
}

/**
 * @brief Prepend single-qubit gate to circuit. If _device is given, directly map to physical device.
 *
 * @param type
 * @param qubit logical
 * @param phase
 */
void Extractor::prepend_single_qubit_gate(std::string const& type, QubitIdType qubit, dvlab::Phase phase) {
    if (type == "rotate") {
        _logical_circuit->add_single_rz(qubit, phase, false);
    } else {
        _logical_circuit->add_gate(type, {qubit}, phase, false);
    }
}

/**
 * @brief Prepend double-qubit gate to circuit. If _device is given, directly map to physical device.
 *
 * @param type
 * @param qubits
 * @param phase
 */
void Extractor::prepend_double_qubit_gate(std::string const& type, QubitIdList const& qubits, dvlab::Phase phase) {
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
        auto qubits = gates.get_qubits();
        if (gates.get_phase() != dvlab::Phase(0)) {
            _logical_circuit->add_gate(gates.get_type_str(), {get<0>(qubits), get<1>(qubits)}, gates.get_phase(), false);
        }
    }

    for (auto const& gates : physical) {
        auto qubits = gates.get_qubits();
        if (gates.is_swap()) {
            prepend_swap_gate(get<0>(qubits), get<1>(qubits), _physical_circuit);
            _num_swaps++;
        } else if (gates.get_phase() != dvlab::Phase(0)) {
            _physical_circuit->add_gate(gates.get_type_str(), {get<0>(qubits), get<1>(qubits)}, gates.get_phase(), false);
        }
    }
}

/**
 * @brief Prepend swap gate. Decompose into three CXs
 *
 * @param q0 logical
 * @param q1 logical
 * @param circuit
 */
void Extractor::prepend_swap_gate(QubitIdType q0, QubitIdType q1, QCir* circuit) {
    // NOTE - No qubit permutation in Physical Circuit
    circuit->add_gate("cx", {q0, q1}, dvlab::Phase(1), false);
    circuit->add_gate("cx", {q1, q0}, dvlab::Phase(1), false);
    circuit->add_gate("cx", {q0, q1}, dvlab::Phase(1), false);
}

/**
 * @brief Check whether the frontier is clean
 *
 * @return true if clean
 * @return false if dirty
 */
bool Extractor::frontier_is_cleaned() {
    for (auto& f : _frontier) {
        if (f->get_phase() != dvlab::Phase(0)) return false;
        for (auto& [n, e] : _graph->get_neighbors(f)) {
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
        if (_graph->get_num_neighbors(f) == 2)
            return true;
    }
    return false;
}

/**
 * @brief Print frontier
 *
 */
void Extractor::print_frontier(spdlog::level::level_enum lvl) const {
    spdlog::log(lvl, "Frontier:");
    for (auto& f : _frontier)
        spdlog::log(lvl, "Qubit {}: {}", f->get_qubit(), f->get_id());
    spdlog::log(lvl, "");
}

/**
 * @brief Print neighbors
 *
 */
void Extractor::print_neighbors(spdlog::level::level_enum lvl) const {
    spdlog::log(lvl, "Neighbors:");
    for (auto& n : _neighbors)
        spdlog::log(lvl, "{}", n->get_id());
    spdlog::log(lvl, "");
}

/**
 * @brief Print axels
 *
 */
void Extractor::print_axels(spdlog::level::level_enum lvl) const {
    spdlog::log(lvl, "Axels:");
    for (auto& n : _axels) {
        spdlog::log(lvl,
                    "{} (phase gadget: {})",
                    n->get_id(),
                    fmt::join(_graph->get_neighbors(n) |
                                  std::views::keys |
                                  std::views::filter([this](ZXVertex* v) { return _graph->is_gadget_leaf(v); }) |
                                  std::views::transform([](ZXVertex* v) { return v->get_id(); }),
                              ", "));
    }
    spdlog::log(lvl, "");
}

void Extractor::print_cxs() const {
    fmt::println("CXs: {}", fmt::join(_cnots | std::views::transform([](auto& p) { return fmt::format("({}, {})", p.first, p.second); }), "  "));
}

}  // namespace qsyn::extractor
