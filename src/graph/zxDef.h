/****************************************************************************
  FileName     [ zxDef.h ]
  PackageName  [ graph ]
  Synopsis     [ Define basic data or var for graph package ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_DEF_H
#define ZX_DEF_H

#include <iosfwd>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ordered_hashmap.h"
#include "ordered_hashset.h"
#include "phase.h"

class ZXVertex;
class ZXGraph;
class ZXGraphMgr;

enum class VertexType {
    BOUNDARY,
    Z,
    X,
    H_BOX,
    ERRORTYPE  // Never use this
};

enum class EdgeType {
    SIMPLE,
    HADAMARD,
    ERRORTYPE  // Never use this
};

//------------------------------------------------------------------------
//  Define types
//------------------------------------------------------------------------

using ZXVertexList = ordered_hashset<ZXVertex*>;
using EdgePair = std::pair<std::pair<ZXVertex*, ZXVertex*>, EdgeType>;
using NeighborPair = std::pair<ZXVertex*, EdgeType>;
using Neighbors = ordered_hashset<NeighborPair>;

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

template <typename T>
std::ostream& operator<<(typename std::enable_if<std::is_enum<T>::value, std::ostream>::type& stream, const T& e) {
    return stream << static_cast<typename std::underlying_type<T>::type>(e);
}

#endif  // ZX_DEF_H