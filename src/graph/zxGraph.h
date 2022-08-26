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


class VT;
class ZXGraph;
enum class VertexType;
enum class EdgeType;

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------
struct rationalNumber{};
enum class VertexType{
    BOUNDARY,
    Z,
    X,
    H_BOX
};

VertexType str2VertexType(string str);

enum class EdgeType{
    SIMPLE,
    HADAMARD
};

EdgeType str2EdgeType(string str);

template<typename T> ostream& operator<<(typename enable_if<is_enum<T>::value, ostream>::type& stream, const T& e){
    return stream << static_cast<typename underlying_type<T>::type>(e);
}


class VT{
    public:
        VT(size_t id, int qubit, VertexType vt) {
            _id = id;
            _qubit = qubit;
            _type = vt;
        }
        ~VT(){}

        // Getter and Setter
        size_t getId() const { return _id; }
        int getQubit() const { return _qubit; }
        VertexType getType() const { return _type; }
        rationalNumber getPhase() const { return _phase; }
        vector<pair<VT*, EdgeType> > getNeighbors() const { return _neighbors; }
        pair<VT*, EdgeType> getNeighborById(size_t id) const;

        void setId(size_t id) { _id = id; }
        void setQubit(int q) {_qubit = q; }
        void setType(VertexType vt) { _type = vt; }
        void setPhase(rationalNumber p) { _phase = p; }
        void setNeighbors(vector<pair<VT*, EdgeType> > neighbors){ _neighbors = neighbors; }


        // Add and Remove
        void addNeighbor(pair<VT*, EdgeType> neighbor) { _neighbors.push_back(neighbor); }
        void removeNeighbor(pair<VT*, EdgeType> neighbor);
        void removeNeighborById(size_t id);


        // Print functions
        void printVertex() const;
        void printNeighbors() const;


        // Test
        bool isNeighbor(VT* v) const;
        bool isNeighborById(size_t id) const;
        
        
    private:
        int                                         _qubit;
        size_t                                      _id;
        VertexType                                  _type;
        rationalNumber                              _phase;
        vector<pair<VT*, EdgeType> >                _neighbors;
};


class ZXGraph{
    public:
        ZXGraph(size_t id) : _id(id){
            _inputs.clear();
            _outputs.clear();
            _vertices.clear();
            _edges.clear();
        }
        ~ZXGraph() {}


        // Getter and Setter
        void setId(size_t id) { _id = id; }
        void setQubitCount(size_t c) { _nqubit = c; }
        void setInputs(vector<VT*> inputs) { _inputs = inputs; }
        void setOutputs(vector<VT*> outputs) { _outputs = outputs; }
        void setVertices(vector<VT*> vertices) { _vertices = vertices; }
        void setEdges(vector<pair<pair<VT*, VT*>, EdgeType> > edges) { _edges = edges; }
        
        size_t getId() const { return _id; }
        size_t getQubitCount() const { return _nqubit; }
        vector<VT*> getInputs() const { return _inputs; }
        size_t getNumInputs() const { return _inputs.size(); }
        vector<VT*> getOutputs() const { return _outputs; }
        size_t getNumOutputs() const { return _outputs.size(); }
        vector<VT*> getVertices() const { return _vertices; }
        size_t getNumVertices() const { return _vertices.size(); }
        vector<pair<pair<VT*, VT*>, EdgeType> > getEdges() const { return _edges; }
        size_t getNumEdges() const { return _edges.size(); }



        // For testing
        void generateCNOT();
        bool isEmpty() const;
        bool isValid() const;
        bool isConnected(VT* v1, VT* v2) const;
        bool isId(size_t id) const;



        // Add and Remove
        void addInput(size_t id, int qubit, VertexType vt);
        void addOutput(size_t id, int qubit, VertexType vt);
        void addVertex(size_t id, int qubit, VertexType vt);
        void addEdgeById(size_t id_s, size_t id_t, EdgeType et);
        void removeVertex(VT* v);
        void removeVertexById(size_t id);
        void removeIsolatedVertices();
        void removeEdgeById(size_t id_s, size_t id_t);

        
        // Find functions
        VT* findVertexById(size_t id) const;

        // Print functions
        void printGraph() const;
        void printInputs() const;
        void printOutputs() const;
        void printVertices() const;
        void printEdges() const;
        

    private:
        size_t                                              _id;
        size_t                                              _nqubit;
        vector<VT*>                                         _inputs;
        vector<VT*>                                         _outputs;
        vector<VT*>                                         _vertices;
        vector<pair<pair<VT*, VT*>, EdgeType> >             _edges;

};

#endif