/****************************************************************************
  FileName     [ zxGraph.h ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX-graph structures ]
  Author       [ Cheng-Hua Lu, Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_GRAPH_H
#define ZX_GRAPH_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include "phase.h"
#include "qtensor.h"
#include "zxDef.h"
using namespace std;


class ZXVertex;
class ZXGraph;
enum class VertexType;
enum class EdgeType;

//------------------------------------------------------------------------
//  Define types
//------------------------------------------------------------------------

typedef pair<ZXVertex*, EdgeType*> NeighborPair;
typedef pair<pair<ZXVertex*, ZXVertex*>, EdgeType*> EdgePair;
typedef unordered_multimap<ZXVertex*, EdgeType*> NeighborMap;


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


class ZXVertex{
    public:
        ZXVertex(size_t id, int qubit, VertexType vt, Phase phase = Phase()) {
            _id = id;
            _qubit = qubit;
            _type = vt;
            _phase = phase;
            _DFSCounter = 0;
            _pin = unsigned(-1);
            _neighborMap.clear();
        }
        ~ZXVertex(){}

        // Getter and Setter
        const size_t& getId() const                                         { return _id; }
        const int& getQubit() const                                         { return _qubit; }
        const VertexType& getType() const                                   { return _type; }
        const Phase& getPhase() const                                       { return _phase; }
        const size_t& getPin() const                                        { return _pin; }   
        vector<ZXVertex*> getNeighbors() const;
        ZXVertex* getNeighbor(size_t idx) const;
        const NeighborMap& getNeighborMap() const                           { return _neighborMap; }
        size_t getNumNeighbors() const                                      { return _neighborMap.size(); }
        QTensor<double> getTSform();
        
        void setId(const size_t& id)                                        { _id = id; }
        void setQubit(const int& q)                                         { _qubit = q; }
        void setType(const VertexType& ZXVertex)                            { _type = ZXVertex; }
        void setPhase(const Phase& p)                                       { _phase = p; }
        void setNeighborMap(const NeighborMap& neighborMap)                 { _neighborMap = neighborMap; }
        void setPin(const size_t& p)                                        { _pin = p; }

        // Add and Remove
        void addNeighbor(const NeighborPair& neighbor)                      { _neighborMap.insert(neighbor); }
        void removeNeighbor(const NeighborPair& neighbor); // not defined
        void removeNeighborById(const size_t& id); // not defined!


        // Print functions
        void printVertex() const;
        void printNeighborMap() const;

        
        // Action
        void disconnect(ZXVertex* v, bool checked = false);


        // Test
        bool isNeighbor(ZXVertex* v) const;

        
        // DFS
        bool isVisited(unsigned global) { return global == _DFSCounter; }
        void setVisited(unsigned global) { _DFSCounter = global; }

    private:
        int                                  _qubit;
        size_t                               _id;
        VertexType                           _type;
        Phase                                _phase;
        NeighborMap                          _neighborMap;
        unsigned                             _DFSCounter;
        size_t                               _pin;
        
};


class ZXGraph{
    public:
        ZXGraph(size_t id, void** ref = NULL) : _id(id), _ref(ref), _tensor(1.+0.i){
            _inputs.clear();
            _outputs.clear();
            _vertices.clear();
            _edges.clear();
            _inputList.clear();
            _outputList.clear();
            _topoOrder.clear();
            _globalDFScounter = 1;
        }
        
        ~ZXGraph() {
            for(size_t i = 0; i < _vertices.size(); i++) delete _vertices[i];
        }


        // Getter and Setter
        void setId(size_t id)                           { _id = id; }
        void setRef(void** ref)                         { _ref = ref; }
        void setInputs(vector<ZXVertex*> inputs)        { _inputs = inputs; }
        void setOutputs(vector<ZXVertex*> outputs)      { _outputs = outputs; }
        void setVertices(vector<ZXVertex*> vertices)    { _vertices = vertices; }
        void setEdges(vector<EdgePair > edges)          { _edges = edges; }
        
        const size_t& getId() const                     { return _id; }
        void** getRef() const                           { return _ref; }
        const vector<ZXVertex*>& getInputs() const      { return _inputs; }
        size_t getNumInputs() const                     { return _inputs.size(); }
        const vector<ZXVertex*>& getOutputs() const     { return _outputs; }
        size_t getNumOutputs() const                    { return _outputs.size(); }
        const vector<ZXVertex*>& getVertices() const    { return _vertices; }
        size_t getNumVertices() const                   { return _vertices.size(); }
        const vector<EdgePair >& getEdges() const       { return _edges; }
        size_t getNumEdges() const                      { return _edges.size(); }


        // For testing
        void generateCNOT();
        bool isEmpty() const;
        bool isValid() const;
        bool isConnected(ZXVertex* v1, ZXVertex* v2) const;
        bool isId(size_t id) const;
        bool isInputQubit(int qubit) const;
        bool isOutputQubit(int qubit) const;


        // Add and Remove
        ZXVertex* addInput(size_t id, int qubit);
        ZXVertex* addOutput(size_t id, int qubit);
        ZXVertex* addVertex(size_t id, int qubit, VertexType ZXVertex, Phase phase = Phase() );
        EdgePair addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType* et);
        void addEdgeById(size_t id_s, size_t id_t, EdgeType* et);
        void addInputs(vector<ZXVertex*> inputs);
        void addOutputs(vector<ZXVertex*> outputs);
        void addVertices(vector<ZXVertex*> vertices);
        void addEdges(vector<EdgePair> edges);
        
        void mergeInputList(unordered_map<size_t, ZXVertex*> lst);
        void mergeOutputList(unordered_map<size_t, ZXVertex*> lst);

        void removeVertex(ZXVertex* v, bool checked = false);
        void removeVertices(vector<ZXVertex* > vertices, bool checked = false);
        void removeVertexById(const size_t& id);
        void removeIsolatedVertices();
        void removeEdge(ZXVertex* vs, ZXVertex* vt, bool checked = false);
        void removeEdgeByEdgePair(const EdgePair& ep);
        void removeEdgeById(const size_t& id_s, const size_t& id_t);

                
        // Find functions
        ZXVertex* findInputById(const size_t& id) const; // not defined!
        ZXVertex* findOutputById(const size_t& id) const; // not defined!
        ZXVertex* findVertexById(const size_t& id) const;
        size_t findNextId() const;


        // Action
        void reset();
        ZXGraph* copy() const;
        void sortIOByQubit();
        void sortVerticeById();
        void liftQubit(const size_t& n);


        // Print functions
        void printGraph() const;
        void printInputs() const;
        void printOutputs() const;
        void printVertices() const;
        void printEdges() const;
        
        // Traverse
        void updateTopoOrder();
        template<typename F>
        void topoTraverse(F lambda){
            updateTopoOrder();
            for_each(_topoOrder.begin(),_topoOrder.end(),lambda);
        }

        // For mapping
        void tensorMapping();
        void concatenate(ZXGraph* tmp, bool remove_imm = false);
        void setInputHash(const size_t& q, ZXVertex* v)                    { _inputList[q] = v; }
        void setOutputHash(const size_t& q, ZXVertex* v)                   { _outputList[q] = v; }
        void setInputList(const unordered_map<size_t, ZXVertex*>& lst)     { _inputList = lst; }
        void setOutputList(const unordered_map<size_t, ZXVertex*>& lst)    { _outputList = lst; }
        const unordered_map<size_t, ZXVertex*>& getInputList() const       { return _inputList; }
        const unordered_map<size_t, ZXVertex*>& getOutputList() const      { return _outputList; }
        ZXVertex* getInputFromHash(const size_t& q);
        ZXVertex* getOutputFromHash(const size_t& q);
        vector<ZXVertex*> getNonBoundary();
        vector<EdgePair> getInnerEdges();
        void cleanRedundantEdges();

        
    private:
        size_t                            _id;
        void**                            _ref;
        QTensor<double>                   _tensor;
        vector<ZXVertex*>                 _inputs;
        vector<ZXVertex*>                 _outputs;
        vector<ZXVertex*>                 _vertices;
        vector<EdgePair >                 _edges;
        unordered_map<size_t, ZXVertex*>  _inputList;
        unordered_map<size_t, ZXVertex*>  _outputList;
        vector<ZXVertex*>                 _topoOrder;
        unsigned                          _globalDFScounter;
        void DFS(ZXVertex*);

};

#endif