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

VertexType str2VertexType(string str);

string VertexType2Str(VertexType vt);

enum class EdgeType{
    SIMPLE,
    HADAMARD,
    ERRORTYPE       // Never use this
};

EdgeType toggleEdge(EdgeType et);

EdgeType* str2EdgeType(string str);

string EdgeType2Str(EdgeType* et);

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
            _neighborMap.clear();
        }
        ~ZXVertex(){}

        // Getter and Setter
        size_t getId() const                                                { return _id; }
        int getQubit() const                                                { return _qubit; }
        VertexType getType() const                                          { return _type; }
        Phase getPhase() const                                              { return _phase; }
        NeighborMap getNeighborMap() const                                  { return _neighborMap; }

        void setId(size_t id)                                                           { _id = id; }
        void setQubit(int q)                                                            {_qubit = q; }
        void setType(VertexType ZXVertex)                                               { _type = ZXVertex; }
        void setPhase(Phase p)                                                          { _phase = p; }
        void setNeighborMap(NeighborMap neighborMap)                                    { _neighborMap = neighborMap; }


        // Add and Remove
        void addNeighbor(NeighborPair neighbor)                             { _neighborMap.insert(neighbor);}
        void removeNeighbor(NeighborPair neighbor);
        void removeNeighborById(size_t id);


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
        int                                     _qubit;
        size_t                                  _id;
        Phase                                   _phase;
        VertexType                              _type;
        NeighborMap                             _neighborMap;
        unsigned                                _DFSCounter;
};


class ZXGraph{
    public:
        ZXGraph(size_t id, void** ref = NULL) : _id(id), _ref(ref){
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
            // for(size_t i = 0; i < _vertices.size(); i++) delete _vertices[i];
            // for(size_t i = 0; i < _topoOrder.size(); i++) delete _topoOrder[i];
        }


        // Getter and Setter
        void setId(size_t id)                           { _id = id; }
        void setRef(void** ref)                         { _ref = ref; }
        void setInputs(vector<ZXVertex*> inputs)        { _inputs = inputs; }
        void setOutputs(vector<ZXVertex*> outputs)      { _outputs = outputs; }
        void setVertices(vector<ZXVertex*> vertices)    { _vertices = vertices; }
        void setEdges(vector<EdgePair > edges)          { _edges = edges; }
        
        size_t getId() const                            { return _id; }
        void** getRef() const                           { return _ref; }
        vector<ZXVertex*> getInputs() const             { return _inputs; }
        size_t getNumInputs() const                     { return _inputs.size(); }
        vector<ZXVertex*> getOutputs() const            { return _outputs; }
        size_t getNumOutputs() const                    { return _outputs.size(); }
        vector<ZXVertex*> getVertices() const           { return _vertices; }
        size_t getNumVertices() const                   { return _vertices.size(); }
        vector<EdgePair > getEdges() const              { return _edges; }
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

        void removeVertex(ZXVertex* v, bool checked = false);
        void removeVertices(vector<ZXVertex* > vertices, bool checked = false);
        void removeVertexById(size_t id);
        void removeIsolatedVertices();
        void removeEdge(ZXVertex* vs, ZXVertex* vt, bool checked = false);
        void removeEdgeById(size_t id_s, size_t id_t);

                
        // Find functions
        ZXVertex* findInputById(size_t id) const;
        ZXVertex* findOutputById(size_t id) const;
        ZXVertex* findVertexById(size_t id) const;
        size_t findNextId() const;


        // Action
        void reset();
        ZXGraph* copy() const;
        void sortIOByQubit();
        void sortVerticeById();


        // Print functions
        void printGraph() const;
        void printInputs() const;
        void printOutputs() const;
        void printVertices() const;
        void printEdges() const;
        
        //Traverse
        void updateTopoOrder();

        // For mapping
        void concatenate(ZXGraph* tmp, bool remove_imm = false);
        void setInputHash(size_t q, ZXVertex* v)                    { _inputList[q] = v; }
        void setOutputHash(size_t q, ZXVertex* v)                   { _outputList[q] = v; }
        unordered_map<size_t, ZXVertex*> getInputList() const       { return _inputList; }
        unordered_map<size_t, ZXVertex*> getOutputList() const      { return _outputList; }
        ZXVertex* getInputFromHash(size_t q);
        ZXVertex* getOutputFromHash(size_t q);
        vector<ZXVertex*> getNonBoundary();
        vector<EdgePair> getInnerEdges();
        void cleanRedundantEdges();

        
    private:
        size_t                            _id;
        void**                            _ref;
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