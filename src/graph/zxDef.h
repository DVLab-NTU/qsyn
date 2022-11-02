/****************************************************************************
  FileName     [ zxDef.h ]
  PackageName  [ graph ]
  Synopsis     [ Define basic data or var for graph package ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_DEF_H
#define ZX_DEF_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "myHashMap.h"
using namespace std;

class ZXVertex;
class ZXGraph;
class ZXGraphMgr;

enum class VertexType{
    BOUNDARY,
    Z,
    X,
    H_BOX,
    ERRORTYPE       // Never use this
};

enum class EdgeType{
    SIMPLE,
    HADAMARD,
    ERRORTYPE       // Never use this
};

//------------------------------------------------------------------------
//  Define types
//------------------------------------------------------------------------

using EdgePair     = pair<pair<ZXVertex*, ZXVertex*>, EdgeType> ;
using NeighborPair = pair<ZXVertex*, EdgeType>;
using Neighbors    = unordered_set<pair<ZXVertex*, EdgeType>>;

using EdgePair_depr     = pair<pair<ZXVertex*, ZXVertex*>, EdgeType*> ;
using NeighborPair_depr = pair<ZXVertex*, EdgeType*> ;
using NeighborMap_depr  = unordered_multimap<ZXVertex*, EdgeType*>;

//------------------------------------------------------------------------
//   Define hashes
//------------------------------------------------------------------------

namespace std {
template <>
struct hash<NeighborPair> {
    size_t operator()(const NeighborPair& k) const {
        return (
            (hash<ZXVertex*>()(k.first) ^ 
            (hash<EdgeType>()(k.second) << 1)) >> 1
        );
    }
};

template <>
struct hash<EdgePair_depr> {
    size_t operator()(const EdgePair_depr& k) const {
        return (
            (hash<ZXVertex*>()(k.first.first) ^ 
            (hash<ZXVertex*>()(k.first.second) << 1)) >> 1) ^ 
            (hash<EdgeType*>()(k.second) << 1
        );
    }
};

template <>
struct hash<EdgePair> {
    size_t operator()(const EdgePair& k) const {
        return (
            (hash<ZXVertex*>()(k.first.first) ^ 
            (hash<ZXVertex*>()(k.first.second) << 1)) >> 1) ^ 
            (hash<EdgeType>()(k.second) << 1
        );
    }
};
}  // namespace std

#endif // ZX_DEF_H