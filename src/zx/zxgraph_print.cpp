/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Print functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cstddef>
#include <gsl/narrow>
#include <map>
#include <ranges>

#include "fmt/core.h"
#include "spdlog/common.h"
#include "zx/zxgraph.hpp"

namespace qsyn {

namespace zx {

/**
 * @brief Print information of ZXGraph
 *
 */
void ZXGraph::print_graph(spdlog::level::level_enum lvl) const {
    if (!spdlog::should_log(lvl)) return;
    spdlog::log(lvl, "Graph ({} inputs, {} outputs, {} vertices, {} edges)", num_inputs(), num_outputs(), num_vertices(), num_edges());
}

/**
 * @brief Print Inputs of ZXGraph
 *
 */
void ZXGraph::print_inputs() const {
    fmt::println("Input:  ({})", fmt::join(_inputs | std::views::transform([](ZXVertex* v) { return v->get_id(); }), ", "));
    fmt::println("Total #Inputs: {}", num_inputs());
}

/**
 * @brief Print Outputs of ZXGraph
 *
 */
void ZXGraph::print_outputs() const {
    fmt::println("Output: ({})", fmt::join(_outputs | std::views::transform([](ZXVertex* v) { return v->get_id(); }), ", "));
    fmt::println("Total #Outputs: {}", num_outputs());
}

/**
 * @brief Print Gadgets of ZXGraph
 *
 */
void ZXGraph::print_gadgets() const {
    for (const auto& cand : get_vertices()) {
        if (is_gadget_leaf(cand)) {
            fmt::print("Gadget leaf: {:>4}, ", cand->get_id());
            fmt::print("axel: {:>4}, ", get_first_neighbor(cand).first->get_id());
            fmt::println("phase: {}", cand->phase());
        }
    }
    fmt::println("Total #Gadgets: {}", num_gadgets());
}

/**
 * @brief Print Inputs and Outputs of ZXGraph
 *
 */
void ZXGraph::print_io() const {
    fmt::println("Input:  ({})", fmt::join(_inputs | std::views::transform([](ZXVertex* v) { return v->get_id(); }), ", "));
    fmt::println("Output: ({})", fmt::join(_outputs | std::views::transform([](ZXVertex* v) { return v->get_id(); }), ", "));
    fmt::println("Total #(I,O): ({}, {})", num_inputs(), num_outputs());
}

/**
 * @brief Print Vertices of ZXGraph
 *
 */
void ZXGraph::print_vertices(spdlog::level::level_enum lvl) const {
    if (!spdlog::should_log(lvl)) return;
    spdlog::log(lvl, "");
    std::ranges::for_each(get_vertices(), [&lvl](ZXVertex* v) { v->print_vertex(lvl); });
    spdlog::log(lvl, "Total #Vertices: {}", num_vertices());
    spdlog::log(lvl, "");
}

/**
 * @brief Print Vertices of ZXGraph in `cand`.
 *
 * @param cand
 */
void ZXGraph::print_vertices(std::vector<size_t> cand) const {
    std::unordered_map<size_t, ZXVertex*> id2_vmap = create_id_to_vertex_map();

    fmt::println("");
    for (size_t i = 0; i < cand.size(); i++) {
        if (is_v_id(cand[i])) id2_vmap[cand[i]]->print_vertex();
    }
    fmt::println("");
}

/**
 * @brief Print Vertices of ZXGraph in `cand` by qubit.
 *
 * @param cand
 */
void ZXGraph::print_vertices_by_rows(spdlog::level::level_enum lvl, std::vector<float> const& cand) const {
    if (!spdlog::should_log(lvl)) return;
    std::map<float, std::vector<ZXVertex*>> q2_vmap;
    for (auto const& v : get_vertices()) {
        if (!q2_vmap.contains(v->get_row())) {
            q2_vmap.emplace(v->get_row(), std::vector<ZXVertex*>(1, v));
        } else {
            q2_vmap[v->get_row()].emplace_back(v);
        }
    }
    if (cand.empty()) {
        for (auto const& [key, vec] : q2_vmap) {
            spdlog::log(lvl, "");
            for (auto const& v : vec) v->print_vertex(lvl);
            spdlog::log(lvl, "");
        }
    } else {
        for (auto const& v : cand) {
            if (q2_vmap.contains(v)) {
                spdlog::log(lvl, "");
                for (auto const& v : q2_vmap[v]) v->print_vertex(lvl);
            }
            spdlog::log(lvl, "");
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
    fmt::println("Total #Edges: {}", num_edges());
}

/**
 * @brief Rearrange vertices on each qubit so that each vertex can be separated in the printed graph.
 *
 */
void ZXGraph::adjust_vertex_coordinates() {
    std::unordered_map<float, std::vector<ZXVertex*>> row_to_vertices_map;
    std::unordered_set<QubitIdType> visited_rows;
    std::vector<ZXVertex*> vertex_queue;

    for (auto const& i : _inputs) {
        vertex_queue.emplace_back(i);
        visited_rows.insert(i->get_id());
    }
    while (!vertex_queue.empty()) {
        ZXVertex* v = vertex_queue.front();
        vertex_queue.erase(vertex_queue.begin());
        row_to_vertices_map[v->get_row()].emplace_back(v);
        for (auto const& nb : get_neighbors(v) | std::views::keys) {
            if (!dvlab::contains(visited_rows, nb->get_id())) {
                vertex_queue.emplace_back(nb);
                visited_rows.insert(nb->get_id());
            }
        }
    }
    std::vector<ZXVertex*> gadgets;
    float non_gadget = 0;
    for (size_t i = 0; i < row_to_vertices_map[-2].size(); i++) {
        if (num_neighbors(row_to_vertices_map[-2][i]) == 1) {  // Not Gadgets
            gadgets.emplace_back(row_to_vertices_map[-2][i]);
        } else
            non_gadget++;
    }
    std::erase_if(row_to_vertices_map[-2],
                  [this](ZXVertex* v) { return this->num_neighbors(v) == 1; });

    row_to_vertices_map[-2].insert(
        row_to_vertices_map[-2].end(), gadgets.begin(), gadgets.end());

    for (auto const& [qid, vertices] : row_to_vertices_map) {
        auto col = std::invoke([qid = qid, non_gadget]() -> float {
            if (qid == -2) return 0.5;
            if (qid == -1) return 0.5f + non_gadget;
            return 0.0f;
        });
        for (auto const& v : vertices) {
            v->set_col(col);
            col++;
        }
    }

    auto const max_col = std::ceil(
        std::ranges::max(
            row_to_vertices_map |
            std::views::values |
            std::views::transform([](std::vector<ZXVertex*> const& v) {
                return v.empty()
                           ? 0
                           : std::ranges::max(
                                 v |
                                 std::views::transform(
                                     [](ZXVertex* v) {
                                         return v->get_col();
                                     }));
            })));
    for (auto& o : _outputs) o->set_col(max_col);
}

}  // namespace zx

}  // namespace qsyn
