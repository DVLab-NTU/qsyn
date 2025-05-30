/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./extract.hpp"

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <random>
#include <ranges>

#include "duostra/duostra.hpp"
#include "duostra/mapping_eqv_checker.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include "util/boolean_matrix.hpp"
#include "util/ordered_hashmap.hpp"
#include "util/util.hpp"
#include "zx/flow/gflow.hpp"
#include "zx/simplifier/simplify.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;
using namespace qsyn::qcir;

namespace qsyn::extractor {

Extractor::Extractor(
    ZXGraph* graph,
    ExtractorConfig config,
    QCir* qcir, bool random)
    : _graph(graph),
      _logical_circuit{qcir},
      _random(random),
      _config(config) {
    initialize();
}

/**
 * @brief Initialize the extractor. Set ZXGraph to QCir qubit map.
 *
 */
void Extractor::initialize() {
    spdlog::debug("Initializing extractor");

    QubitIdType cnt = 0;
    if (_logical_circuit == nullptr) {
        _logical_circuit = new QCir(_graph->num_outputs());
    }
    for (auto& o : _graph->get_outputs()) {
        ZXVertex* neighbor_to_output = _graph->get_first_neighbor(o).first;
        if (!neighbor_to_output->is_boundary()) {
            neighbor_to_output->set_qubit(o->get_qubit());
            _frontier.emplace(neighbor_to_output);
        }
        _qubit_map[o->get_qubit()] = cnt;
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
    _max_axel = _axels.size();
    print_frontier(spdlog::level::level_enum::trace);
    print_neighbors(spdlog::level::level_enum::trace);
    _graph->print_vertices_by_rows(spdlog::level::level_enum::trace);
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
    _graph->print_vertices_by_rows(spdlog::level::level_enum::trace);

    if (_config.permute_qubits) {
        permute_qubits();
        _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
        _graph->print_vertices_by_rows(spdlog::level::level_enum::trace);
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
            _graph->print_vertices_by_rows(spdlog::level::level_enum::trace);
            _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
            continue;
        }
        if (_config.dynamic_order) {
            // Should clean CZs before further extraction
            extract_czs();
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
        _graph->print_vertices_by_rows(spdlog::level::level_enum::trace);
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
    extract_singles();
    if (!_config.dynamic_order) {
        extract_czs();
    }
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
            _logical_circuit->prepend(HGate(), {_qubit_map[o->get_qubit()]});
            toggle_list.emplace_back(o, _graph->get_first_neighbor(o).first);
        }
        auto const ph = _graph->get_first_neighbor(o).first->phase();
        if (ph != dvlab::Phase(0)) {
            _logical_circuit->prepend(PZGate(ph), {_qubit_map[o->get_qubit()]});
            _graph->get_first_neighbor(o).first->phase() = dvlab::Phase(0);
        }
    }
    for (auto& [s, t] : toggle_list) {
        _graph->remove_edge(s, t, EdgeType::hadamard);
        _graph->add_edge(s, t, EdgeType::simple);
    }
    _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
    _graph->print_vertices_by_rows(spdlog::level::level_enum::trace);
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
            if (f->phase() != dvlab::Phase(0)) {
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
    _num_cz_rms += remove_list.size();
    if (_previous_gadget) {
        _num_cz_rms_after_gadget += remove_list.size();
    }

    _biadjacency = get_biadjacency_matrix(*_graph, _frontier, _frontier);
    std::vector<ZXVertex*> idx2vertex;
    for (auto const v : _frontier)
        idx2vertex.emplace_back(v);

    for (auto const& [s, t] : remove_list)
        _graph->remove_edge(s, t, EdgeType::hadamard);

    std::vector<qcir::QCirGate> gates;

    size_t saved_cz_cnt = 0;
    if (_config.reduce_czs) {
        // Remove two most similar rows by CXs and CZs
        auto [overlap, commons] = _max_overlap(_biadjacency);
        while (commons.size() > 2) {
            auto [i, j] = overlap;
            saved_cz_cnt += commons.size() - 2;
            gates.emplace_back(0, CXGate(), QubitIdList{_qubit_map[idx2vertex[i]->get_qubit()], _qubit_map[idx2vertex[j]->get_qubit()]});
            for (auto const& idx : commons) {
                gates.emplace_back(0, CZGate(), QubitIdList{_qubit_map[idx2vertex[j]->get_qubit()], _qubit_map[idx2vertex[idx]->get_qubit()]});
                _biadjacency[i][idx] = 0;
                _biadjacency[j][idx] = 0;
                _biadjacency[idx][i] = 0;
                _biadjacency[idx][j] = 0;
            }
            gates.emplace_back(0, CXGate(), QubitIdList{_qubit_map[idx2vertex[i]->get_qubit()], _qubit_map[idx2vertex[j]->get_qubit()]});
            std::tie(overlap, commons) = _max_overlap(_biadjacency);
        }
        if (saved_cz_cnt > 0) spdlog::info("Reduce {} 2-qubit gate(s)", saved_cz_cnt);
    }

    // Add CZs from the remaining biadj matrix
    for (size_t i = 0; i < _biadjacency.num_rows(); i++) {
        for (size_t j = i + 1; j < _biadjacency.num_rows(); j++) {
            if (_biadjacency[i][j])
                gates.emplace_back(0, CZGate(), QubitIdList{_qubit_map[idx2vertex[i]->get_qubit()], _qubit_map[idx2vertex[j]->get_qubit()]});
        }
    }
    _biadjacency.clear();

    if (!gates.empty())
        prepend_series_gates(gates);

    _logical_circuit->print_circuit_diagram(spdlog::level::level_enum::trace);
    _graph->print_vertices_by_rows(spdlog::level::level_enum::trace);

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
    _num_cx_rms += _cnots.size();
    for (auto& [t, c] : _cnots) {
        // NOTE - targ and ctrl are opposite here
        auto ctrl = _qubit_map[front_id2_vertex[c]->get_qubit()];
        auto targ = _qubit_map[front_id2_vertex[t]->get_qubit()];
        spdlog::debug("Adding CX: {} {}", ctrl, targ);
        _logical_circuit->prepend(CXGate(), {ctrl, targ});
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
        _logical_circuit->prepend(HGate(), {_qubit_map[f->get_qubit()]});
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

    if (check && front_neigh_pairs.empty()) {
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
    print_frontier(spdlog::level::level_enum::trace);
    print_axels(spdlog::level::level_enum::trace);

    std::vector<ZXVertex*> shuffle_neighbors;
    for (const auto& v : _neighbors) {
        shuffle_neighbors.push_back(v);
    }

    if (_random) {
        static std::random_device rd1;
        static std::mt19937 g1(rd1());
        std::shuffle(std::begin(shuffle_neighbors), std::end(shuffle_neighbors), g1);
    }

    bool removed_some_gadgets = false;

    for (auto& n : shuffle_neighbors) {
        if (!_axels.contains(n)) {
            continue;
        }
        for (auto& [candidate, _] : _graph->get_neighbors(n)) {
            if (_frontier.contains(candidate)) {
                auto const qubit = candidate->get_qubit();
                _axels.erase(n);
                _frontier.erase(candidate);

                ZXVertex* target_boundary = nullptr;
                for (auto& [boundary, _] : _graph->get_neighbors(candidate)) {
                    if (boundary->is_boundary()) {
                        target_boundary = boundary;
                        break;
                    }
                }
                if (_config.dynamic_order) {
                    const std::vector<qcir::QCirGate> gates;
                    int diff = 0;
                    int n_cz = 0;
                    for (auto const& [cz_cand, _] : _graph->get_neighbors(candidate)) {
                        if (_frontier.contains(cz_cand)) {
                            diff += _calculate_diff_pivot_edges_if_extracting_cz(candidate, n, cz_cand);
                            n_cz++;
                        }
                    }
                    if (_config.pred_coeff * float(diff) + 1 * float(n_cz) < 0) {
                        extract_czs();
                    }
                }

                PivotUnfusion const
                    pvu{candidate->get_id(), n->get_id(), {}, {}};
                pvu.apply_unchecked(*_graph);

                assert(target_boundary != nullptr);
                auto new_frontier = _graph->get_first_neighbor(target_boundary).first;
                new_frontier->set_qubit(qubit);
                _frontier.emplace(_graph->get_first_neighbor(target_boundary).first);
                // REVIEW - qubit_map
                removed_some_gadgets = true;
                break;
            }
        }
    }

    _graph->print_vertices(spdlog::level::level_enum::trace);
    print_frontier(spdlog::level::level_enum::trace);
    print_axels(spdlog::level::level_enum::trace);

    return removed_some_gadgets;
}

/**
 * @brief Calculate the difference in the number of pivot edges before and after extracting CZ gates
 *
 * @param frontier
 * @param axel
 * @param cz_target
 * @return int
 */
int Extractor::_calculate_diff_pivot_edges_if_extracting_cz(ZXVertex* frontier, ZXVertex* axel, ZXVertex* cz_target) {
    std::vector<ZXVertex*> n_frontier, n_axel, n_both;
    const bool cz_target_is_both = _graph->is_neighbor(cz_target, axel);
    for (auto const& [n, _] : _graph->get_neighbors(frontier)) {
        if (_graph->is_neighbor(n, axel)) {
            n_both.emplace_back(n);
        } else {
            n_frontier.emplace_back(n);
        }
    }
    for (auto const& [n, _] : _graph->get_neighbors(axel)) {
        if (!_graph->is_neighbor(n, axel)) {
            n_axel.emplace_back(n);
        }
    }
    int edge_cnt = 0;
    if (cz_target_is_both) {
        // If remove CZ: from n(both) to n(axel)
        // Pivot edges from --frontier & --axel to --frontier & --both
        // Difference: # --both - # --axel
        for (const auto& v : n_both) {
            if (_graph->is_neighbor(v, cz_target))
                edge_cnt--;
            else
                edge_cnt++;
        }
        for (const auto& v : n_axel) {
            if (_graph->is_neighbor(v, cz_target))
                edge_cnt++;
            else
                edge_cnt--;
        }
    } else {
        // If remove CZ: from n(frontier) to n(none)
        // Pivot edges from --axel & --both to nothing
        // Difference: -(# --axel + # --both)
        for (const auto& v : n_axel) {
            if (_graph->is_neighbor(v, cz_target))
                edge_cnt++;
            else
                edge_cnt--;
        }
        for (const auto& v : n_both) {
            if (_graph->is_neighbor(v, cz_target))
                edge_cnt++;
            else
                edge_cnt--;
        }
    }
    return edge_cnt;
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

    std::ranges::set_difference(
        col_set, targ_val, inserter(left, left.end()));
    std::ranges::set_difference(
        col_set, targ_key, inserter(right, right.end()));
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
            std::ranges::set_difference(
                _row_info[i], claimed_cols,
                inserter(free_cols, free_cols.end()));
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

            if (free_cols.empty()) {
                spdlog::debug("No free column for column optimal swap!!");
                return {};  // NOTE - Contradiction
            }

            for (auto& j : free_cols) {
                std::set<size_t> free_rows;
                std::ranges::set_difference(
                    _col_info[j], claimed_rows,
                    inserter(free_rows, free_rows.end()));
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

    if (_config.sort_frontier) {
        _frontier.sort([](ZXVertex const* a, ZXVertex const* b) {
            return a->get_qubit() < b->get_qubit();
        });
    }
    if (_config.sort_neighbors) {
        // REVIEW - Do not know why sort here would be better
        _neighbors.sort([](ZXVertex const* a, ZXVertex const* b) {
            return a->get_id() < b->get_id();
        });
    }

    std::vector<dvlab::BooleanMatrix::RowOperation> greedy_opers;

    update_matrix();

    if (_config.optimize_level == 0) {
        column_optimal_swap();
        update_matrix();
        _biadjacency.gaussian_elimination_skip(_config.block_size, true, true);
        if (_config.filter_duplicate_cxs) _filter_duplicate_cxs();
        _cnots = _biadjacency.get_row_operations();
        return true;
    }

    dvlab::BooleanMatrix greedy_matrix = _biadjacency;
    auto const backup_neighbors        = _neighbors;

    DVLAB_ASSERT(_config.optimize_level <= 3, "Error: wrong optimize level");

    if (_config.optimize_level > 1) {
        // NOTE - opt = 2 or 3
        greedy_opers = greedy_reduction(greedy_matrix);
        for (auto const& oper : greedy_opers) {
            greedy_matrix.row_operation(oper.first, oper.second, true);
        }
    }

    if (_config.optimize_level != 2 || greedy_opers.empty()) {
        // NOTE - opt = 1, 3 or when 2 cannot find the result
        column_optimal_swap();
        update_matrix();

        auto min_cnots = SIZE_MAX;
        dvlab::BooleanMatrix best_matrix;
        for (size_t blk = 1; blk < _biadjacency.num_cols(); blk++) {
            _block_elimination(best_matrix, min_cnots, blk);
        }
        if (_config.optimize_level == 1) {
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
    if (_config.filter_duplicate_cxs) _filter_duplicate_cxs();
    if (copied_matrix.get_row_operations().size() < min_n_cxs) {
        min_n_cxs   = copied_matrix.get_row_operations().size();
        best_matrix = copied_matrix;
    }
}

/**
 * @brief Permute qubit if input and output are not match
 *
 */
void Extractor::permute_qubits() {
    spdlog::debug("Permuting qubits");
    dvlab::utils::ordered_hashmap<QubitIdType, QubitIdType> swap_map;  // o to i
    std::unordered_map<QubitIdType, QubitIdType> swap_inv_map;         // i to o
    bool matched = true;
    for (auto& o : _graph->get_outputs()) {
        if (_graph->num_neighbors(o) != 1) {
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
        _graph->remove_edge(_graph->get_first_neighbor(o).first, o);
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

        if (_graph->num_neighbors(f) == 2 && f->phase() == dvlab::Phase(0)) {
            // NOTE - Remove
            for (auto& [b, ep] : _graph->get_neighbors(f)) {
                if (_graph->get_inputs().contains(b)) {
                    if (ep == EdgeType::hadamard) {
                        _logical_circuit->prepend(HGate(), {_qubit_map[f->get_qubit()]});
                    }
                    break;
                }
            }
            rm_vs.emplace_back(f);
        } else {
            for (auto [b, ep] : _graph->get_neighbors(f)) {
                // Deliberately pass-by-copy. Passing-by-ref causes segfault
                if (_graph->get_inputs().contains(b)) {
                    zx::add_identity_vertex(
                        *_graph, f->get_id(), b->get_id(),
                        VertexType::z, EdgeType::hadamard);
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
 * @brief Create bi-adjacency matrix from frontier and neighbors
 *
 */
void Extractor::update_matrix() {
    _biadjacency = get_biadjacency_matrix(*_graph, _frontier, _neighbors);
}

// /**
//  * @brief Prepend series of gates.
//  *
//  * @param logical
//  * @param physical
//  */
void Extractor::prepend_series_gates(std::vector<qcir::QCirGate> const& logical /*, std::vector<Operation> const& physical*/) {
    for (auto const& gate : logical) {
        _logical_circuit->prepend(gate.get_operation(), gate.get_qubits());
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
    circuit->prepend(CXGate(), {q0, q1});
    circuit->prepend(CXGate(), {q1, q0});
    circuit->prepend(CXGate(), {q0, q1});
}

/**
 * @brief Check whether the frontier is clean
 *
 * @return true if clean
 * @return false if dirty
 */
bool Extractor::frontier_is_cleaned() {
    for (auto& f : _frontier) {
        if (f->phase() != dvlab::Phase(0)) return false;
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
    return std::ranges::any_of(_neighbors, [&](auto& n) {
        return _axels.contains(n);
    });
}

/**
 * @brief Check whether the frontier contains a vertex has only a single neighbor (boundary excluded).
 *
 * @return true
 * @return false
 */
bool Extractor::contains_single_neighbor() {
    return std::ranges::any_of(_frontier, [&](auto& f) {
        return _graph->num_neighbors(f) == 2;
    });
}

/**
 * @brief Print frontier
 *
 */
void Extractor::print_frontier(spdlog::level::level_enum lvl) const {
    if (!spdlog::should_log(lvl)) return;
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
    if (!spdlog::should_log(lvl)) return;
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
    if (!spdlog::should_log(lvl)) return;
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
