/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXVertex member functions and VT/ET functions]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>

#include <ranges>
#include <string>
#include <unicode/display_width.hpp>

#include "./zxgraph.hpp"
#include "spdlog/common.h"
#include "util/dvlab_string.hpp"

namespace qsyn::zx {

/**
 * @brief Returns true if the two vertices have the same type and phase
 *
 * @param other
 * @return true
 * @return false
 */
bool ZXVertex::operator==(ZXVertex const& other) const {
    return _attrs.type == other._attrs.type &&
           _attrs.phase == other._attrs.phase;
}

/**
 * @brief Returns true if the two vertices have different type or phase
 *
 * @param other
 * @return true
 * @return false
 */
bool ZXVertex::operator!=(ZXVertex const& other) const {
    return !(*this == other);
}

/**
 * @brief return a vector of neighbor vertices
 *
 * @return vector<ZXVertex*>
 */
std::vector<ZXVertex*> ZXGraph::get_copied_neighbors(ZXVertex* v) const {
    std::vector<ZXVertex*> storage;
    for (auto const& neighbor : v->_neighbors) {
        storage.emplace_back(neighbor.first);
    }
    return storage;
}

// FIXME - should raise this function to the ZXGraph class

/**
 * @brief Print summary of ZXVertex
 *
 */
void ZXVertex::print_vertex(spdlog::level::level_enum lvl) const {
    std::vector<NeighborPair> storage(std::begin(_neighbors), std::end(_neighbors));
    std::ranges::sort(storage, [](NeighborPair const& a, NeighborPair const& b) {
        return (a.first->get_id() != b.first->get_id()) ? (a.first->get_id() < b.first->get_id()) : (a.second < b.second);
    });
    auto type_str       = fmt::format("{}", type());
    auto ansi_token_len = type_str.size();
    spdlog::log(
        lvl,
        "ID: {0:>4} {1:<{2}} (Qubit, Col): {3:<14} #Neighbors: {4:>3}    {5}",
        get_id(),
        fmt::format("({}, {})", type_str, phase().get_print_string()),
        11ul + ansi_token_len - 2 * (is_boundary() ? 1 : 0),
        is_boundary() ? fmt::format("({}, {})", get_qubit(), get_col()) : fmt::format("({}, {})", get_row(), get_col()),
        _neighbors.size(),
        fmt::join(storage | std::views::transform([](NeighborPair const& nbp) { return fmt::format("({}, {})", nbp.first->get_id(), nbp.second); }), " "));
}

/*****************************************************/
/*   Vertex Type & Edge Type functions               */
/*****************************************************/

/**
 * @brief Return toggled EdgeType of `et`
 *        Ex: et = SIMPLE, return HADAMARD
 *
 * @param et
 * @return EdgeType
 */
EdgeType toggle_edge(EdgeType const& et) {
    return (et == EdgeType::simple) ? EdgeType::hadamard : EdgeType::simple;
}

/**
 * @brief Convert string to `VertexType`
 *
 * @param str
 * @return VertexType
 */
std::optional<VertexType> str_to_vertex_type(std::string const& str) {
    using namespace std::string_view_literals;
    using dvlab::str::tolower_string;
    if (dvlab::str::is_prefix_of(tolower_string(str), "boundary")) return VertexType::boundary;
    if (dvlab::str::is_prefix_of(tolower_string(str), "zspider")) return VertexType::z;
    if (dvlab::str::is_prefix_of(tolower_string(str), "xspider")) return VertexType::x;
    if (dvlab::str::is_prefix_of(tolower_string(str), "hbox")) return VertexType::h_box;
    if (dvlab::str::is_prefix_of(tolower_string(str), "hadamard")) return VertexType::h_box;
    return std::nullopt;
}

/**
 * @brief Convert string to `EdgeType`
 *
 * @param str
 * @return EdgeType
 */
std::optional<EdgeType> str_to_edge_type(std::string const& str) {
    using namespace std::string_view_literals;
    if (dvlab::str::is_prefix_of(dvlab::str::tolower_string(str), "simple")) return EdgeType::simple;
    if (dvlab::str::is_prefix_of(dvlab::str::tolower_string(str), "hadamard")) return EdgeType::hadamard;
    return std::nullopt;
}

/**
 * @brief Make `EdgePair` and make sure that source's id is not greater than target's id.
 *
 * @param v1
 * @param v2
 * @param et
 * @return EdgePair
 */
EdgePair make_edge_pair(ZXVertex* v1, ZXVertex* v2, EdgeType et) {
    return make_pair(
        (v1->get_id() < v2->get_id()) ? std::make_pair(v1, v2) : std::make_pair(v2, v1),
        et);
}

/**
 * @brief Make `EdgePair` and make sure that source's id is not greater than target's id.
 *
 * @param epair
 * @return EdgePair
 */
EdgePair make_edge_pair(EdgePair epair) {
    return make_pair(
        (epair.first.first->get_id() < epair.first.second->get_id()) ? std::make_pair(epair.first.first, epair.first.second) : std::make_pair(epair.first.second, epair.first.first),
        epair.second);
}

/**
 * @brief Make dummy `EdgePair`
 *
 * @return EdgePair
 */
EdgePair make_edge_pair_dummy() {
    return std::make_pair(std::make_pair(nullptr, nullptr), EdgeType::simple);
}

}  // namespace qsyn::zx

std::ostream& operator<<(std::ostream& stream, qsyn::zx::VertexType const& vt) {
    return stream << fmt::format("{}", vt);
}
std::ostream& operator<<(std::ostream& stream, qsyn::zx::EdgeType const& et) {
    return stream << fmt::format("{}", et);
}
