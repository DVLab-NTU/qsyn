/****************************************************************************
  FileName     [ zxDef.hpp ]
  PackageName  [ zx ]
  Synopsis     [ Define basic data or var for graph package ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <functional>
#include <iosfwd>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "util/ordered_hashmap.hpp"
#include "util/ordered_hashset.hpp"
#include "util/phase.hpp"

class ZXVertex;
class ZXGraph;

enum class VertexType {
    BOUNDARY,
    Z,
    X,
    H_BOX
};

enum class EdgeType {
    SIMPLE,
    HADAMARD
};

//------------------------------------------------------------------------
//  Define types
//------------------------------------------------------------------------

using ZXVertexList = ordered_hashset<ZXVertex*>;
using EdgePair = std::pair<std::pair<ZXVertex*, ZXVertex*>, EdgeType>;
using NeighborPair = std::pair<ZXVertex*, EdgeType>;
using Neighbors = ordered_hashset<NeighborPair>;

// two boundary vertices from different ZXGraph and the edge type between them
using ZXCut = std::tuple<ZXVertex*, ZXVertex*, EdgeType>;
using ZXPartitionStrategy = std::function<std::vector<ZXVertexList>(const ZXGraph&, size_t)>;

struct ZXCutHash {
    size_t operator()(const ZXCut& cut) const {
        auto [v1, v2, edgeType] = cut;
        // the order of v1 and v2 does not matter
        if (v1 > v2) std::swap(v1, v2);
        size_t result = std::hash<ZXVertex*>()(v1) ^ std::hash<ZXVertex*>()(v2);
        result ^= std::hash<EdgeType>()(edgeType) << 1;
        return result;
    }
};

struct ZXCutEqual {
    bool operator()(const ZXCut& lhs, const ZXCut& rhs) const {
        auto [v1, v2, edgeType] = lhs;
        auto [v3, v4, edgeType2] = rhs;
        // the order of v1 and v2 does not matter
        if (v1 > v2) std::swap(v1, v2);
        if (v3 > v4) std::swap(v3, v4);
        return v1 == v3 && v2 == v4 && edgeType == edgeType2;
    }
};

using ZXCutSet = ordered_hashset<ZXCut, ZXCutHash, ZXCutEqual>;

namespace ZXParserDetail {

struct VertexInfo {
    char type;
    int qubit;
    float column;
    std::vector<std::pair<char, size_t>> neighbors;
    Phase phase;
};

using StorageType = ordered_hashmap<size_t, VertexInfo>;

}  // namespace ZXParserDetail

//------------------------------------------------------------------------
//   Define hashes
//------------------------------------------------------------------------

namespace std {
template <>
struct hash<vector<ZXVertex*>> {
    size_t operator()(const vector<ZXVertex*>& k) const {
        size_t ret = hash<ZXVertex*>()(k[0]);
        for (size_t i = 1; i < k.size(); i++) {
            ret ^= hash<ZXVertex*>()(k[i]);
        }

        return ret;
    }
};

template <>
struct hash<NeighborPair> {
    size_t operator()(const NeighborPair& k) const {
        return (
            (hash<ZXVertex*>()(k.first) ^
             (hash<EdgeType>()(k.second) << 1)) >>
            1);
    }
};

template <>
struct hash<EdgePair> {
    size_t operator()(const EdgePair& k) const {
        return (
                   (hash<ZXVertex*>()(k.first.first) ^
                    (hash<ZXVertex*>()(k.first.second) << 1)) >>
                   1) ^
               (hash<EdgeType>()(k.second) << 1);
    }
};
}  // namespace std

std::ostream& operator<<(std::ostream& stream, VertexType const& vt);
std::ostream& operator<<(std::ostream& stream, EdgeType const& et);
