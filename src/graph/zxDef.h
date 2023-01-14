/****************************************************************************
  FileName     [ zxDef.h ]
  PackageName  [ graph ]
  Synopsis     [ Define basic data or var for graph package ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_DEF_H
#define ZX_DEF_H

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "myHashMap.h"
#include "ordered_hashset.h"
using namespace std;

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
using EdgePair = pair<pair<ZXVertex*, ZXVertex*>, EdgeType>;
using NeighborPair = pair<ZXVertex*, EdgeType>;
using Neighbors = ordered_hashset<NeighborPair>;

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

#endif  // ZX_DEF_H