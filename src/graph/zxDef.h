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

enum class VertexType;
enum class EdgeType;

//------------------------------------------------------------------------
//  Define types
//------------------------------------------------------------------------

typedef pair<ZXVertex*, EdgeType*> NeighborPair;
typedef pair<pair<ZXVertex*, ZXVertex*>, EdgeType*> EdgePair;
typedef unordered_multimap<ZXVertex*, EdgeType*> NeighborMap;
typedef unordered_set<pair<ZXVertex*, EdgeType>> Neighbors;

typedef pair<ZXVertex*, EdgeType> NeighborP;

namespace std{
template <>
struct hash<NeighborP>
  {
    size_t operator()(const NeighborP& k) const
    {
      return ((hash<ZXVertex*>()(k.first)
               ^ (hash<EdgeType>()(k.second) << 1)) >> 1);
    }
  };
}


namespace std{
template <>
struct hash<EdgePair>
  {
    size_t operator()(const EdgePair& k) const
    {
      return ((hash<ZXVertex*>()(k.first.first)
               ^ (hash<ZXVertex*>()(k.first.second) << 1)) >> 1)
               ^ (hash< EdgeType*>()(k.second) << 1);
    }
  };
}

EdgePair makeEdgeKey(ZXVertex* v1, ZXVertex* v2, EdgeType* et);
EdgePair makeEdgeKey(EdgePair epair);

typedef pair<pair<ZXVertex*, ZXVertex*>, EdgeType> EdgeKey;

namespace std{
template <>
struct hash<EdgeKey>
  {
    size_t operator()(const EdgeKey& k) const
    {
      return ((hash<ZXVertex*>()(k.first.first)
               ^ (hash<ZXVertex*>()(k.first.second) << 1)) >> 1)
               ^ (hash< EdgeType>()(k.second) << 1);
    }
  };
}
EdgeKey makeEdgeKey(ZXVertex* v1, ZXVertex* v2, EdgeType e);
EdgeKey makeEdgeKey(EdgeKey epair);
//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------
enum class VertexType{
    BOUNDARY,
    Z,
    X,
    H_BOX,
    ERRORTYPE       // Never use this
};

VertexType str2VertexType(const string& str);

string VertexType2Str(const VertexType& vt);

enum class EdgeType{
    SIMPLE,
    HADAMARD,
    ERRORTYPE       // Never use this
};

EdgeType toggleEdge(const EdgeType& et);

EdgeType* str2EdgeType(const string& str);

string EdgeType2Str(const EdgeType* et);

template<typename T> ostream& operator<<(typename enable_if<is_enum<T>::value, ostream>::type& stream, const T& e){
    return stream << static_cast<typename underlying_type<T>::type>(e);
}
#endif // ZX_DEF_H