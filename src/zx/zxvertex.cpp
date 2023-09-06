/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXVertex member functions and VT/ET functions]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <exception>
#include <ranges>
#include <string>

#include "./zxgraph.hpp"

using namespace std;

/**
 * @brief return a vector of neighbor vertices
 *
 * @return vector<ZXVertex*>
 */
vector<ZXVertex*> ZXVertex::get_copied_neighbors() {
    vector<ZXVertex*> storage;
    for (auto const& neighbor : _neighbors) {
        storage.emplace_back(neighbor.first);
    }
    return storage;
}

/**
 * @brief Print summary of ZXVertex
 *
 */
void ZXVertex::print_vertex() const {
    cout << "ID:" << right << setw(4) << _id;
    cout << " (" << _type << ", " << left << setw(12 - ((_phase == Phase(0)) ? 1 : 0)) << (_phase.get_print_string() + ")");
    cout << "  (Qubit, Col): (" << _qubit << ", " << _col << ")\t"
         << "  #Neighbors: " << right << setw(3) << _neighbors.size() << "     ";
    print_neighbors();
}

/**
 * @brief Print each element in _neighborMap
 *
 */
void ZXVertex::print_neighbors() const {
    vector<NeighborPair> storage(_neighbors.begin(), _neighbors.end());

    ranges::sort(storage, [](NeighborPair const& a, NeighborPair const& b) {
        return (a.first->get_id() != b.first->get_id()) ? (a.first->get_id() < b.first->get_id()) : (a.second < b.second);
    });

    for (auto const& [nb, etype] : storage) {
        cout << "(" << nb->get_id() << ", " << etype << ") ";
    }
    cout << endl;
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
std::optional<VertexType> str_to_vertex_type(string const& str) {
    using dvlab::str::to_lower_string;
    if ("boundary"s.starts_with(to_lower_string(str))) return VertexType::boundary;
    if ("zspider"s.starts_with(to_lower_string(str))) return VertexType::z;
    if ("xspider"s.starts_with(to_lower_string(str))) return VertexType::x;
    if ("hbox"s.starts_with(to_lower_string(str))) return VertexType::h_box;
    return std::nullopt;
}

std::ostream& operator<<(std::ostream& stream, VertexType const& vt) {
    return stream << fmt::format("{}", vt);
}

/**
 * @brief Convert string to `EdgeType`
 *
 * @param str
 * @return EdgeType
 */
std::optional<EdgeType> str_to_edge_type(string const& str) {
    if ("simple"s.starts_with(dvlab::str::to_lower_string(str))) return EdgeType::simple;
    if ("hadamard"s.starts_with(dvlab::str::to_lower_string(str))) return EdgeType::hadamard;
    return std::nullopt;
}

std::ostream& operator<<(std::ostream& stream, EdgeType const& et) {
    return stream << fmt::format("{}", et);
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
        (v1->get_id() < v2->get_id()) ? make_pair(v1, v2) : make_pair(v2, v1),
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
        (epair.first.first->get_id() < epair.first.second->get_id()) ? make_pair(epair.first.first, epair.first.second) : make_pair(epair.first.second, epair.first.first),
        epair.second);
}

/**
 * @brief Make dummy `EdgePair`
 *
 * @return EdgePair
 */
EdgePair make_edge_pair_dummy() {
    return make_pair(make_pair(nullptr, nullptr), EdgeType::simple);
}
