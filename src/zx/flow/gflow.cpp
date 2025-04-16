/****************************************************************************
  PackageName  [ gflow ]
  Synopsis     [ Define class GFlow member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./gflow.hpp"

#include <spdlog/spdlog.h>

#include <cassert>
#include <cstddef>
#include <ranges>

#include "util/boolean_matrix.hpp"
#include "util/text_format.hpp"
#include "zx/simplifier/simplify.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

constexpr auto vertex_to_id = [](ZXVertex* v) { return v->get_id(); };

/**
 * @brief Calculate the Z correction set of a vertex,
 *        i.e., Odd(g(v))
 *
 * @param v
 * @return ZXVertexList
 */
ZXVertexList GFlow::get_z_correction_set(ZXVertex* v) const {
    ZXVertexList out;

    dvlab::utils::ordered_hashmap<ZXVertex*, size_t> num_occurrences;

    for (auto const& gv : get_x_correction_set(v)) {
        // FIXME - should count neighbor!
        for (auto const& [nb, et] : _zxgraph->get_neighbors(gv)) {
            if (num_occurrences.contains(nb)) {
                num_occurrences[nb]++;
            } else {
                num_occurrences.emplace(nb, 1);
            }
        }
    }

    for (auto const& [odd_gv, n] : num_occurrences) {
        if (n % 2 == 1) out.emplace(odd_gv);
    }

    return out;
}

/**
 * @brief Initialize the gflow calculator
 *
 */
void GFlow::_initialize() {
    _levels.clear();
    _x_correction_sets.clear();
    _measurement_planes.clear();
    _frontier.clear();
    _neighbors.clear();
    _taken.clear();
    _vertex2levels.clear();
    using MP = MeasurementPlane;

    // Measurement planes - See Table 1, p.10 of the paper
    // M. Backens, H. Miller-Bakewell, G. de Felice, L. Lobski, & J. van de Wetering (2021). There and back again: A circuit extraction tale. Quantum, 5, 421.
    // https://quantum-journal.org/papers/q-2021-03-25-421/
    for (auto const& v : _zxgraph->get_vertices()) {
        _measurement_planes.emplace(v, MP::xy);
    }
    // if calculating extended gflow, modify some of the measurement plane
    if (_do_extended) {
        for (auto const& v : _zxgraph->get_vertices()) {
            if (_zxgraph->is_gadget_leaf(v)) {
                _measurement_planes[v] = MP::not_a_qubit;
                _taken.insert(v);
            } else if (_zxgraph->is_gadget_axel(v))
                _measurement_planes[v] = v->has_n_pi_phase() ? MP::yz
                                         : v->phase().denominator() == 2
                                             ? MP::xz
                                             : MP::error;
            assert(_measurement_planes[v] != MP::error);
        }
    }
}

/**
 * @brief Calculate the GFlow to the ZXGraph
 *
 */
bool GFlow::calculate() {
    // REVIEW - exclude boundary nodes
    _initialize();

    _calculate_zeroth_layer();

    while (!_levels.back().empty()) {
        _update_neighbors_by_frontier();

        _levels.emplace_back();

        auto coefficient_matrix = get_biadjacency_matrix(*_zxgraph, _neighbors, _frontier);

        size_t i = 0;
        spdlog::trace("Frontier: {}", fmt::join(_frontier | std::views::transform(vertex_to_id), " "));
        spdlog::trace("Neighbors: {}", fmt::join(_neighbors | std::views::transform(vertex_to_id), " "));

        for (auto& v : _neighbors) {
            if (_do_independent_layers &&
                std::ranges::any_of(_zxgraph->get_neighbors(v), [this](NeighborPair const& nbpair) {
                    return this->_levels.back().contains(nbpair.first);
                })) {
                spdlog::trace("Skipping vertex {} : connected to current level", v->get_id());
                continue;
            }

            auto augmented_matrix = _prepare_matrix(v, i, coefficient_matrix);

            if (augmented_matrix.gaussian_elimination_augmented(false)) {
                spdlog::trace("Solved {}, adding to this level", v->get_id());
                _taken.insert(v);
                _levels.back().insert(v);
                _set_correction_set_by_matrix(v, augmented_matrix);
            } else {
                spdlog::trace("No solution for {}.", v->get_id());
            }
            ++i;
        }
        _update_frontier();

        for (auto& v : _levels.back()) {
            _vertex2levels.emplace(v, _levels.size() - 1);
        }
    }

    _valid = (_taken.size() == _zxgraph->num_vertices());
    _levels.pop_back();  // the back is always empty

    std::vector<std::pair<size_t, ZXVertex*>> inputs_to_move;
    for (size_t i = 0; i < _levels.size() - 1; ++i) {
        for (auto& v : _levels[i]) {
            if (_zxgraph->get_inputs().contains(v)) {
                inputs_to_move.emplace_back(i, v);
            }
        }
    }

    for (auto& [level, v] : inputs_to_move) {
        _levels[level].erase(v);
        _levels.back().insert(v);
    }

    for (auto& lvl : _levels) {
        lvl.sort([](ZXVertex* a, ZXVertex* b) { return a->get_id() < b->get_id(); });
    }
    return _valid;
}

/**
 * @brief Calculate 0th layer
 *
 */
void GFlow::_calculate_zeroth_layer() {
    // initialize the 0th layer to be output
    _frontier = _zxgraph->get_outputs();

    _levels.emplace_back(_zxgraph->get_outputs());

    for (auto& v : _zxgraph->get_outputs()) {
        assert(!_x_correction_sets.contains(v));
        _x_correction_sets[v] = ZXVertexList();
        _taken.insert(v);
    }
}

/**
 * @brief Update neighbors by frontier
 *
 */
void GFlow::_update_neighbors_by_frontier() {
    _neighbors.clear();

    for (auto& v : _frontier) {
        for (auto& [nb, _] : _zxgraph->get_neighbors(v)) {
            if (_taken.contains(nb))
                continue;
            if (_measurement_planes[nb] == MeasurementPlane::not_a_qubit) {
                _taken.insert(nb);
                continue;
            }

            _neighbors.insert(nb);
        }
    }
}

/**
 * @brief Set the correction set to v by the matrix
 *
 * @param v correction set of whom
 * @param matrix
 */
void GFlow::_set_correction_set_by_matrix(ZXVertex* v, dvlab::BooleanMatrix const& matrix) {
    assert(!_x_correction_sets.contains(v));
    _x_correction_sets[v] = ZXVertexList();

    for (size_t r = 0; r < matrix.num_rows(); ++r) {
        if (matrix[r].back() == 0) continue;
        size_t c = 0;
        for (auto& f : _frontier) {
            if (matrix[r][c] == 1) {
                _x_correction_sets[v].insert(f);
                break;
            }
            c++;
        }
    }
    if (is_x_error(v)) _x_correction_sets[v].insert(v);

    assert(!_x_correction_sets[v].empty());
}

/**
 * @brief prepare the matrix to solve depending on the measurement plane.
 *
 */
dvlab::BooleanMatrix GFlow::_prepare_matrix(ZXVertex* v, size_t i, dvlab::BooleanMatrix const& matrix) {
    dvlab::BooleanMatrix augmented_matrix = matrix;
    augmented_matrix.push_zeros_column();

    auto itr = std::begin(_neighbors);
    for (size_t j = 0; j < augmented_matrix.num_rows(); ++j) {
        if (is_z_error(v)) {
            augmented_matrix[j][augmented_matrix.num_cols() - 1] += (i == j) ? 1 : 0;
        }
        if (is_x_error(v)) {
            if (_zxgraph->is_neighbor(v, *itr, EdgeType::hadamard)) {
                augmented_matrix[j][augmented_matrix.num_cols() - 1] += 1;
            }
        }
        ++itr;
    }

    for (size_t j = 0; j < augmented_matrix.num_rows(); ++j) {
        augmented_matrix[j][augmented_matrix.num_cols() - 1] %= 2;
    }

    return augmented_matrix;
}

/**
 * @brief Update frontier
 *
 */
void GFlow::_update_frontier() {
    // remove vertex that are not frontiers anymore
    std::vector<ZXVertex*> to_remove;
    for (auto& v : _frontier) {
        if (std::ranges::all_of(_zxgraph->get_neighbors(v),
                                [this](NeighborPair const& nbp) {
                                    return _taken.contains(nbp.first);
                                })) {
            to_remove.emplace_back(v);
        }
    }

    for (auto& v : to_remove) {
        _frontier.erase(v);
    }

    // add the last layer to the frontier
    for (auto& v : _levels.back()) {
        if (!_zxgraph->get_inputs().contains(v)) {
            _frontier.insert(v);
        }
    }
}

/**
 * @brief Print gflow
 *
 */
void GFlow::print() const {
    fmt::println("GFlow of the graph:");
    for (size_t i = 0; i < _levels.size(); ++i) {
        fmt::println("Level {}", i);
        for (auto const& v : _levels[i]) {
            print_x_correction_set(v);
        }
    }
}

/**
 * @brief Print gflow according to levels
 *
 */
void GFlow::print_levels() const {
    fmt::println("GFlow levels of the graph:");
    for (size_t i = 0; i < _levels.size(); ++i) {
        fmt::println("Level {:>4}: {}", i, fmt::join(_levels[i] | std::views::transform(vertex_to_id), " "));
    }
}

/**
 * @brief Print correction set of v
 *
 * @param v correction set of whom
 */
void GFlow::print_x_correction_set(ZXVertex* v) const {
    fmt::print("{:>4} ({}): ", v->get_id(), _measurement_planes.at(v));
    if (_x_correction_sets.contains(v)) {
        if (_x_correction_sets.at(v).empty()) {
            fmt::println("(None)");
        } else {
            fmt::println("{}", fmt::join(_x_correction_sets.at(v) | std::views::transform(vertex_to_id), " "));
        }
    } else {
        fmt::println("Does not exist");
    }
}

/**
 * @brief Print correction sets
 *
 */
void GFlow::print_x_correction_sets() const {
    for (auto& v : _zxgraph->get_vertices()) {
        print_x_correction_set(v);
    }
}

/**
 * @brief Print if gflow exists. If not, print which level it breaks
 *
 */
void GFlow::print_summary() const {
    using namespace dvlab;
    if (_valid) {
        fmt::println("{}", fmt_ext::styled_if_ansi_supported("GFlow exists.", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
        fmt::println("#Levels: {}", _levels.size());
    } else {
        fmt::println("{}", fmt_ext::styled_if_ansi_supported("No GFlow exists.", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
        fmt::println("The flow breaks at level {}.", _levels.size());
    }
}

/**
 * @brief Print the vertices with no correction sets
 *
 */
void GFlow::print_failed_vertices() const {
    fmt::println("No correction sets found for the following vertices:");
    fmt::println("{}", fmt::join(_neighbors | std::views::transform(vertex_to_id), " "));
}

std::ostream& operator<<(std::ostream& os, GFlow::MeasurementPlane const& plane) {
    return os << fmt::format("{}", plane);
}
