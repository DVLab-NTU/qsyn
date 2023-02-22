/****************************************************************************
  FileName     [ zxVertex.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXVertex member functions and VT/ET functions]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>
#include <string>

#include "textFormat.h"  // for TextFormat
#include "zxDef.h"       // for EdgeType, VertexType, EdgePair, EdgeType::HA...
#include "zxGraph.h"     // for ZXVertex

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
        storage.push_back(neighbor.first);
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
    // if(_neighbors.size()==0) return;
    vector<NeighborPair> storage;
    for (const auto& neighbor : _neighbors) {
        storage.push_back(neighbor);
        // cout << "(" << nb->getId() << ", " << EdgeType2Str(etype) << ") ";
    }
    sort(begin(storage), end(storage), [](NeighborPair a, NeighborPair b) { return a.second < b.second; });
    sort(begin(storage), end(storage), [](NeighborPair a, NeighborPair b) { return a.first->getId() < b.first->getId(); });

    for (const auto& [nb, etype] : storage) {
        cout << "(" << nb->getId() << ", " << EdgeType2Str(etype) << ") ";
    }
    cout << endl;
}

/**
 * @brief Remove all the connection between `this` and `v`. (Overhauled)
 *
 * @param v
 * @param checked
 */
void ZXVertex::disconnect(ZXVertex* v, bool checked) {
    if (!checked) {
        if (!isNeighbor(v)) {
            cerr << "Error: Vertex " << v->getId() << " is not a neighbor of " << _id << endl;
            return;
        }
    }
    _neighbors.erase(make_pair(v, EdgeType::SIMPLE));
    _neighbors.erase(make_pair(v, EdgeType::HADAMARD));
    v->removeNeighbor(make_pair(this, EdgeType::SIMPLE));
    v->removeNeighbor(make_pair(this, EdgeType::HADAMARD));
}

bool ZXVertex::isGadgetAxel() const {
    return any_of(
        _neighbors.begin(),
        _neighbors.end(),
        [](const NeighborPair& nbp) {
            return nbp.first->getNumNeighbors() == 1;
        });
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
    if (et == EdgeType::SIMPLE) return EdgeType::HADAMARD;
    if (et == EdgeType::HADAMARD) return EdgeType::SIMPLE;
    return EdgeType::ERRORTYPE;
}

/**
 * @brief Convert string to `VertexType`
 *
 * @param str
 * @return VertexType
 */
VertexType str2VertexType(const string& str) {
    if (str == "BOUNDARY") return VertexType::BOUNDARY;
    if (str == "Z") return VertexType::Z;
    if (str == "X") return VertexType::X;
    if (str == "H_BOX") return VertexType::H_BOX;
    return VertexType::ERRORTYPE;
}

/**
 * @brief Convert `VertexType` to string
 *
 * @param vt
 * @return string
 */
string VertexType2Str(const VertexType& vt) {
    if (vt == VertexType::X) return TF::BOLD(TF::RED("X"));
    if (vt == VertexType::Z) return TF::BOLD(TF::GREEN("Z"));
    if (vt == VertexType::H_BOX) return TF::BOLD(TF::YELLOW("H"));
    if (vt == VertexType::BOUNDARY) return "●";
    return "";
}

/**
 * @brief Convert string to `EdgeType`
 *
 * @param str
 * @return EdgeType
 */
EdgeType str2EdgeType(const string& str) {
    if (str == "SIMPLE") return EdgeType::SIMPLE;
    if (str == "HADAMARD") return EdgeType::HADAMARD;
    return EdgeType::ERRORTYPE;
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
    return make_pair(make_pair(nullptr, nullptr), EdgeType::ERRORTYPE);
}
