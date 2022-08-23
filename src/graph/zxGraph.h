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

class VerType;
class EdgeType;
class ZXGraph;

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------
class VerType{
    public:
        VerType(){}
        ~VerType(){}

        // Getter and Setter
        size_t getId() const { return _id; }
        int getQubit() const { return _qubit; }

        void setId(size_t id) { _id = id; }
        void setQubit(int q) {_qubit = q; }
        
    private:
        size_t      _id;
        int         _qubit;
};

class EdgeType{
    public:
        EdgeType(){}
        ~EdgeType(){}

        // Getter and Setter
        size_t getId() const { return _id; }
        string getType() const { return _type; }

        void setId(size_t id) { _id = id; }
        void setQubit(string type) { _type = type; }

    private:
        size_t _id;
        string _type; // [ SIMPLE / HADAMARD ]
        pair<VerType, VerType> _edge;
};

class ZXGraph{
    public:
        ZXGraph(){

        }
        ~ZXGraph(){}

        // Add and Remove
        void addInput(VerType v);
        void addInputs(vector<VerType> vecV);
        void removeInput(VerType v);
        void removeInputs(vector<VerType> vecV);

        void addOutput(VerType v);
        void addOutputs(vector<VerType> vecV);
        void removeOutput(VerType v);
        void removeOutputs(vector<VerType> vecV);

        // Getter and Setter
        void setId(size_t id) { _id = id; }
        void setQubitCount(size_t c) { _nqubit = c; }
        void setInputs(vector<VerType> inputs) { _inputs = inputs; }
        void setOutputs(vector<VerType> outputs) { _outputs = outputs; }
        void setVertices(vector<VerType> vertices) { _vertices = vertices; }
        void setEdges(vector<EdgeType> edges) { _edges = edges; }

        size_t getId() const { return _id; }
        size_t getQubitCount() const { return _nqubit; }

        vector<VerType> getInputs() const { return _inputs; }
        size_t getNumInputs() const { return _inputs.size(); }

        vector<VerType> getOutputs() const { return _outputs; }
        size_t getNumOutputs() const { return _outputs.size(); }

        vector<VerType> getVertices() const { return _vertices; }
        size_t getNumVertices() const { return _vertices.size(); }

        vector<EdgeType> getEdges() const { return _edges; }
        size_t getNumEdges() const { return _edges.size(); }

        vector<pair<VerType, int> > getQubits() const { return _qubits; }

        // Print functions
        void printInputs() const;
        void printOutputs() const;
        void printVertices() const;
        void printEdges() const;

    private:
        size_t                                  _id;
        size_t                                  _nqubit;
        vector<VerType>                         _inputs;
        vector<VerType>                         _outputs;
        vector<VerType>                         _vertices;
        vector<EdgeType>                        _edges;
        vector<pair<VerType, int> >             _qubits;

};

#endif