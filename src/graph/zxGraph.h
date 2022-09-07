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
struct rationalNumber{};
enum class VertexType{
    BOUNDARY,
    Z,
    X,
    H_BOX,
    ERRORTYPE
};

VertexType str2VertexType(string str);

enum class EdgeType{
    SIMPLE,
    HADAMARD,
    ERRORTYPE
};

EdgeType str2EdgeType(string str);

template<typename T> ostream& operator<<(typename enable_if<is_enum<T>::value, ostream>::type& stream, const T& e){
    return stream << static_cast<typename underlying_type<T>::type>(e);
}


class ZXVertex{
    public:
        ZXVertex(size_t id, int qubit, VertexType ZXVertex) {
            _id = id;
            _qubit = qubit;
            _type = ZXVertex;
        }
        ZXVertex(const ZXVertex& zxVertex);
        ~ZXVertex(){}

        // Getter and Setter
        size_t getId() const                                { return _id; }
        int getQubit() const                                { return _qubit; }
        VertexType getType() const                          { return _type; }
        rationalNumber getPhase() const                     { return _phase; }
        vector<NeighborPair > getNeighbors() const          { return _neighbors; }
        NeighborPair getNeighborById(size_t id) const;

        void setId(size_t id)                               { _id = id; }
        void setQubit(int q)                                {_qubit = q; }
        void setType(VertexType ZXVertex)                   { _type = ZXVertex; }
        void setPhase(rationalNumber p)                     { _phase = p; }
        void setNeighbors(vector<NeighborPair > neighbors)  { _neighbors = neighbors; }


        // Add and Remove
        void addNeighbor(NeighborPair neighbor)             { _neighbors.push_back(neighbor); }
        void removeNeighbor(NeighborPair neighbor);
        void removeNeighborById(size_t id);


        // Print functions
        void printVertex() const;
        void printNeighbors() const;


        // Test
        bool isNeighbor(ZXVertex* v) const;
        bool isNeighborById(size_t id) const;
        
        
    private:
        int                                  _qubit;
        size_t                               _id;
        VertexType                           _type;
        rationalNumber                       _phase;
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


        // Add and Remove
        void addInput(size_t id, int qubit);
        void addOutput(size_t id, int qubit);
        void addVertex(size_t id, int qubit, VertexType ZXVertex);
        void addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType et);
        void addEdgeById(size_t id_s, size_t id_t, EdgeType et);
        void addInputs(vector<ZXVertex*> inputs);
        void addOutputs(vector<ZXVertex*> outputs);
        void addVertices(vector<ZXVertex*> vertices);
        void addEdges(vector<EdgePair> edges);

        void removeVertex(ZXVertex* v);
        void removeVertexById(size_t id);
        void removeIsolatedVertices();
        void removeEdgeById(size_t id_s, size_t id_t);

        
        // Find functions
        ZXVertex* findVertexById(size_t id) const;
        size_t findNextId() const;

        // Action
        ZXGraph* copy() const;


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