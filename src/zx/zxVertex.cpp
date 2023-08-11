/****************************************************************************
  FileName     [ zxVertex.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXVertex member functions and VT/ET functions]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <string>

#include "./zxDef.h"
#include "./zxGraph.h"
#include "util/textFormat.h"

using namespace std;
namespace TF = TextFormat;
extern size_t verbose;

/**
 * @brief return a vector of neighbor vertices
 *
 * @return vector<ZXVertex*>
 */
vector<ZXVertex*> ZXVertex::getCopiedNeighbors() {
    vector<ZXVertex*> storage;
    for (const auto& neighbor : _neighbors) {
        storage.emplace_back(neighbor.first);
    }
    return storage;
}

/**
 * @brief Print summary of ZXVertex
 *
 */
void ZXVertex::printVertex() const {
    cout << "ID:" << right << setw(4) << _id;
    cout << " (" << VertexType2Str(_type) << ", " << left << setw(12 - ((_phase == Phase(0)) ? 1 : 0)) << (_phase.getPrintString() + ")");
    cout << "  (Qubit, Col): (" << _qubit << ", " << _col << ")\t"
         << "  #Neighbors: " << right << setw(3) << _neighbors.size() << "     ";
    printNeighbors();
}

/**
 * @brief Print each element in _neighborMap
 *
 */
void ZXVertex::printNeighbors() const {
    vector<NeighborPair> storage(_neighbors.begin(), _neighbors.end());

    ranges::sort(storage, [](NeighborPair const& a, NeighborPair const& b) {
        return (a.first->getId() != b.first->getId()) ? (a.first->getId() < b.first->getId()) : (a.second < b.second);
    });

    for (const auto& [nb, etype] : storage) {
        cout << "(" << nb->getId() << ", " << EdgeType2Str(etype) << ") ";
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
EdgeType toggleEdge(const EdgeType& et) {
    return (et == EdgeType::SIMPLE) ? EdgeType::HADAMARD : EdgeType::SIMPLE;
}

/**
 * @brief Convert string to `VertexType`
 *
 * @param str
 * @return VertexType
 */
std::optional<VertexType> str2VertexType(const string& str) {
    if ("boundary"s.starts_with(toLowerString(str))) return VertexType::BOUNDARY;
    if ("zspider"s.starts_with(toLowerString(str))) return VertexType::Z;
    if ("xspider"s.starts_with(toLowerString(str))) return VertexType::X;
    if ("hbox"s.starts_with(toLowerString(str))) return VertexType::H_BOX;
    return std::nullopt;
}

/**
 * @brief Convert `VertexType` to string
 *
 * @param vt
 * @return string
 */
string VertexType2Str(const VertexType& vt) {
    switch (vt) {
        case VertexType::X:
            return TF::BOLD(TF::RED("X"));
        case VertexType::Z:
            return TF::BOLD(TF::GREEN("Z"));
        case VertexType::H_BOX:
            return TF::BOLD(TF::YELLOW("H"));
        case VertexType::BOUNDARY:
        default:
            return "●";
    }
}

/**
 * @brief Convert string to `EdgeType`
 *
 * @param str
 * @return EdgeType
 */
std::optional<EdgeType> str2EdgeType(const string& str) {
    if ("simple"s.starts_with(toLowerString(str))) return EdgeType::SIMPLE;
    if ("hadamard"s.starts_with(toLowerString(str))) return EdgeType::HADAMARD;
    return std::nullopt;
}

/**
 * @brief Convert `EdgeType` to string
 *
 * @param et
 * @return string
 */
string EdgeType2Str(const EdgeType& et) {
    if (et == EdgeType::SIMPLE) return "-";
    if (et == EdgeType::HADAMARD) return TF::BOLD(TF::BLUE("H"));
    return "";
}

/**
 * @brief Make `EdgePair` and make sure that source's id is not greater than target's id.
 *
 * @param v1
 * @param v2
 * @param et
 * @return EdgePair
 */
EdgePair makeEdgePair(ZXVertex* v1, ZXVertex* v2, EdgeType et) {
    return make_pair(
        (v1->getId() < v2->getId()) ? make_pair(v1, v2) : make_pair(v2, v1),
        et);
}

/**
 * @brief Make `EdgePair` and make sure that source's id is not greater than target's id.
 *
 * @param epair
 * @return EdgePair
 */
EdgePair makeEdgePair(EdgePair epair) {
    return make_pair(
        (epair.first.first->getId() < epair.first.second->getId()) ? make_pair(epair.first.first, epair.first.second) : make_pair(epair.first.second, epair.first.first),
        epair.second);
}

/**
 * @brief Make dummy `EdgePair`
 *
 * @return EdgePair
 */
EdgePair makeEdgePairDummy() {
    return make_pair(make_pair(nullptr, nullptr), EdgeType::SIMPLE);
}
