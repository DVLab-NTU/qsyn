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

enum class EdgeType{
    SIMPLE,
    HADAMARD
};

template<typename T> ostream& operator<<(typename enable_if<is_enum<T>::value, ostream>::type& stream, const T& e){
    return stream << static_cast<typename underlying_type<T>::type>(e);
}


class VT{
    public:
        VT(size_t id, int qubit, int row, VertexType vt) {
            _id = id;
            _qubit = qubit;
            _row = row;
            _type = vt;
        }
        ~VT(){}

        // Getter and Setter
        size_t getId() const { return _id; }
        int getQubit() const { return _qubit; }
        int getRow() const { return _row; }
        VertexType getType() const { return _type; }
        rationalNumber getPhase() const { return _phase; }

        void setId(size_t id) { _id = id; }
        void setQubit(int q) {_qubit = q; }
        void setRow(int row) { _row = row; }
        void setType(VertexType vt) { _type = vt; }
        void setPhase(rationalNumber p) { _phase = p; }
        void setNeighbors(vector<pair<VT*, EdgeType> > neighbors){ _neighbors = neighbors; }

        // Add and Remove
        void addNeighbor(pair<VT*, EdgeType> neighbor) { _neighbors.push_back(neighbor); }

        // Print functions
        void printVertex() const;
        
        
    private:
        int                                         _row;
        int                                         _qubit;
        size_t                                      _id;
        VertexType                                  _type;
        rationalNumber                              _phase;
        vector<pair<VT*, EdgeType> >                _neighbors;
};

// class EdgeType{
//     public:
//         EdgeType(): {}
//         ~EdgeType() {}

//         // Getter and Setter
//         size_t getId() const { return _id; }
//         string getType() const { return _type; }
//         pair<VT, VT> getEdge() const { return _edge; }
//         VT getEdgeS() const { return _edge.first; }
//         VT getEdgeT() const { return _edge.second; }

//         void setId(size_t id) { _id = id; }
//         void setQubit(string type) { _type = type; }
//         void setEdge(pair<VT, VT> edge ) { _edge = edge; }

//     private:
//         size_t _id;
//         string _type; // [ SIMPLE / HADAMARD ]
//         pair<VT, VT> _edge;
// };

class ZXGraph{
    public:
        ZXGraph(size_t id) : _id(id){
            _inputs.clear();
            _outputs.clear();
            _vertices.clear();
            _qubits.clear();
            _edges.clear();
        }
        ~ZXGraph() {}

        // For testing
        void generateCNOT();

        // Add and Remove
        void addInput(VT* v) { _inputs.push_back(v); }
        void addInputs(vector<VT*> vecV) {
            for(size_t v = 0; v < vecV.size(); v++) _inputs.push_back(vecV[v]);
        }
        void removeInput(VT v);
        void removeInputs(vector<VT> vecV);

        void addOutput(VT v);
        void addOutputs(vector<VT> vecV);
        void removeOutput(VT v);
        void removeOutputs(vector<VT> vecV);

        void addVertex(VT v);
        void addVertices(vector<VT> vecV);
        void removeVertex(VT v);
        void removeVertices(vector<VT> vecV);

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

        vector<pair<VT*, int> > getQubits() const { return _qubits; }

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
        vector<pair<VT*, int> >                             _qubits;
        vector<pair<pair<VT*, VT*>, EdgeType> >             _edges;

};

#endif