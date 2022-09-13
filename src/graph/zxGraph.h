/****************************************************************************
  FileName     [ zxGraph.h ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX-graph structures ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_GRAPH_H
#define ZX_GRAPH_H

#include <iostream>
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

typedef pair<ZXVertex*, EdgeType> NeighborPair;
typedef pair<pair<ZXVertex*, ZXVertex*>, EdgeType> EdgePair;

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

enum class EdgeType{
    SIMPLE,
    HADAMARD,
    ERRORTYPE       // Never use this
};

EdgeType str2EdgeType(string str);

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
        }
        ZXVertex(const ZXVertex& zxVertex);
        ~ZXVertex(){}

        // Getter and Setter
        size_t getId() const                                                { return _id; }
        int getQubit() const                                                { return _qubit; }
        VertexType getType() const                                          { return _type; }
        Phase getPhase() const                                              { return _phase; }
        vector<NeighborPair > getNeighbors() const                          { return _neighbors; }
        vector<NeighborPair > getNeighborById(size_t id) const;
        vector<NeighborPair > getNeighborByPointer(ZXVertex* v) const;
        

        void setId(size_t id)                                               { _id = id; }
        void setQubit(int q)                                                {_qubit = q; }
        void setType(VertexType ZXVertex)                                   { _type = ZXVertex; }
        void setPhase(Phase p)                                              { _phase = p; }
        void setNeighbors(vector<NeighborPair > neighbors)                  { _neighbors = neighbors; }


        // Add and Remove
        void addNeighbor(NeighborPair neighbor)                             { _neighbors.push_back(neighbor); }
        void removeNeighbor(NeighborPair neighbor, bool silent = true);
        void removeNeighborById(size_t id, bool silent = true);


        // Print functions
        void printVertex() const;
        void printNeighbors() const;

        
        // Action
        void disconnect(ZXVertex* v, bool silent = true);
        void disconnectById(size_t id, bool silent = true);
        void connect(ZXVertex* v, EdgeType et, bool silent = true);
        void rearrange();


        // Test
        bool isNeighbor(ZXVertex* v) const;
        bool isNeighborById(size_t id) const;
        
        
    private:
        int                                  _qubit;
        size_t                               _id;
        VertexType                           _type;
        Phase                                _phase;
        vector<NeighborPair >                _neighbors;
};


class ZXGraph{
    public:
        ZXGraph(size_t id) : _id(id){
            _inputs.clear();
            _outputs.clear();
            _vertices.clear();
            _edges.clear();
        }
        // Copy Constructor
        ZXGraph(const ZXGraph &zxGraph);
        ~ZXGraph() {}


        // Getter and Setter
        void setId(size_t id)                           { _id = id; }
        void setQubitCount(size_t c)                    { _nqubit = c; }
        void setInputs(vector<ZXVertex*> inputs)        { _inputs = inputs; }
        void setOutputs(vector<ZXVertex*> outputs)      { _outputs = outputs; }
        void setVertices(vector<ZXVertex*> vertices)    { _vertices = vertices; }
        void setEdges(vector<EdgePair > edges)          { _edges = edges; }
        
        size_t getId() const                            { return _id; }
        size_t getQubitCount() const                    { return _nqubit; }
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
        void addInput(size_t id, int qubit, bool silent = true);
        void addOutput(size_t id, int qubit, bool silent = true);
        void addVertex(size_t id, int qubit, VertexType ZXVertex, Phase phase = Phase(), bool silent = true);
        void addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType et, bool silent = true);
        void addEdgeById(size_t id_s, size_t id_t, EdgeType et);
        void addInputs(vector<ZXVertex*> inputs);
        void addOutputs(vector<ZXVertex*> outputs);
        void addVertices(vector<ZXVertex*> vertices);
        void addEdges(vector<EdgePair> edges);

        void removeVertex(ZXVertex* v, bool silent = true);
        void removeVertices(vector<ZXVertex* > vertices, bool silent = true);
        void removeVertexById(size_t id, bool silent = true);
        void removeIsolatedVertices(bool silent = true);
        void removeEdge(ZXVertex* vs, ZXVertex* vt, bool silent = true);
        void removeEdgeById(size_t id_s, size_t id_t, bool silent = true);

        
        // Find functions
        ZXVertex* findVertexById(size_t id) const;
        size_t findNextId() const;

        // Action
        ZXGraph* copy() const;
        void sortIOByQubit();
        void sortVerticeById();


        // Print functions
        void printGraph() const;
        void printInputs() const;
        void printOutputs() const;
        void printVertices() const;
        void printEdges() const;
        

    private:
        size_t                            _id;
        size_t                            _nqubit;
        vector<ZXVertex*>                 _inputs;
        vector<ZXVertex*>                 _outputs;
        vector<ZXVertex*>                 _vertices;
        vector<EdgePair >                 _edges;

};

#endif