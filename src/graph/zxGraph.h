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

class VT{
    public:
        VT(size_t id, int qubit): _id(id), _qubit(qubit) {}
        ~VT(){}

        // Getter and Setter
        size_t getId() const { return _id; }
        int getQubit() const { return _qubit; }
        VertexType getType() const { return _type; }

        void setId(size_t id) { _id = id; }
        void setQubit(int q) {_qubit = q; }
        void setType(VertexType vt) { _type = vt; }
        
    private:
        int                     _qubit;
        int                     _row;
        size_t                  _id;
        VertexType              _type;
        rationalNumber          _phase;
        vector<VT*>             _neighbors;
        vector<EdgeType>        _neighEdgesList;
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
        }
        ~ZXGraph() {}

        // Add and Remove
        void addInput(VT v);
        void addInputs(vector<VT> vecV);
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
        void setInputs(vector<VT> inputs) { _inputs = inputs; }
        void setOutputs(vector<VT> outputs) { _outputs = outputs; }
        void setVertices(vector<VT> vertices) { _vertices = vertices; }
        // void setEdges(vector<EdgeType> edges) { _edges = edges; }

        size_t getId() const { return _id; }
        size_t getQubitCount() const { return _nqubit; }

        vector<VT> getInputs() const { return _inputs; }
        size_t getNumInputs() const { return _inputs.size(); }

        vector<VT> getOutputs() const { return _outputs; }
        size_t getNumOutputs() const { return _outputs.size(); }

        vector<VT> getVertices() const { return _vertices; }
        size_t getNumVertices() const { return _vertices.size(); }


        vector<pair<VT, int> > getQubits() const { return _qubits; }

        // Print functions
        void printInputs() const;
        void printOutputs() const;
        void printVertices() const;
        

    private:
        size_t                             _id;
        size_t                             _nqubit;
        vector<VT>                         _inputs;
        vector<VT>                         _outputs;
        vector<VT>                         _vertices;
        vector<pair<VT, int> >             _qubits;

};

#endif