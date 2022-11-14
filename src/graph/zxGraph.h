/****************************************************************************
  FileName     [ zxGraph.h ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX-graph structures ]
  Author       [ Cheng-Hua Lu, Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_GRAPH_H
#define ZX_GRAPH_H

#include <vector>
#include <string>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

#include "zxDef.h"
#include "phase.h"
#include "qtensor.h"
#include "ordered_hashset.h"
using namespace std;

class ZXVertex{
    public:
        class EdgeIterator;
        friend class EdgeIterator;

        ZXVertex(size_t id, int qubit, VertexType vt, Phase phase = Phase()) {
            _id = id;
            _type = vt;
            _qubit = qubit;
            _phase = phase;
            _DFSCounter = 0;
            _pin = unsigned(-1);
            _neighbors.clear();
        }
        ~ZXVertex(){}

        // Getter and Setter
        const size_t& getId() const                                         { return _id; }
        const int& getQubit() const                                         { return _qubit; }
        const size_t& getPin() const                                        { return _pin; }
        const Phase& getPhase() const                                       { return _phase; }
        const VertexType& getType() const                                   { return _type; }
        const Neighbors& getNeighbors() const                               { return _neighbors; }
        const NeighborPair& getFirstNeighbor() const                        { return *(_neighbors.begin()); }
        const NeighborPair& getSecondNeighbor() const                       { return *next((_neighbors.begin())); }

        vector<ZXVertex*> getCopiedNeighbors();                              
        size_t getNumNeighbors() const                                      { return _neighbors.size(); }
        QTensor<double> getTSform();

        void setId(const size_t& id)                                        { _id = id; }
        void setQubit(const int& q)                                         { _qubit = q; }
        void setPin(const size_t& p)                                        { _pin = p; }
        void setPhase(const Phase& p)                                       { _phase = p; }
        void setType(const VertexType& vt)                                  { _type = vt; }
        void setNeighbors(const Neighbors& n)                               { _neighbors = n; }
        
        
        
        // Add and Remove
        void addNeighbor(const NeighborPair& n)                             { _neighbors.insert(n); }
        size_t removeNeighbor(const NeighborPair& n)                        { return _neighbors.erase(n); }
        size_t removeNeighbor(ZXVertex* v, EdgeType et)                     { return removeNeighbor(make_pair(v, et)); }



        // Print functions
        void printVertex() const;
        void printNeighbors() const;

        

        // Action
        void disconnect(ZXVertex* v, bool checked = false);


        // Test
        bool isZ()                                      const { return getType() == VertexType::Z; }
        bool isX()                                      const { return getType() == VertexType::X; }
        bool isHBox()                                   const { return getType() == VertexType::H_BOX; }
        bool isBoundary()                               const { return getType() == VertexType::BOUNDARY; }
        bool isNeighbor(ZXVertex* v)                    const { return _neighbors.contains(make_pair(v, EdgeType::SIMPLE)) || _neighbors.contains(make_pair(v, EdgeType::HADAMARD)); }
        bool isNeighbor(const NeighborPair& n)          const { return _neighbors.contains(n); }
        bool isNeighbor(ZXVertex* v, EdgeType et)       const { return isNeighbor(make_pair(v, et)); }
        
        

        // DFS
        bool isVisited(unsigned global) { return global == _DFSCounter; }
        void setVisited(unsigned global) { _DFSCounter = global; }

    private:
        int                                  _qubit;
        Phase                                _phase;
        size_t                               _id;
        size_t                               _pin;
        unsigned                             _DFSCounter;
        Neighbors                            _neighbors;
        VertexType                           _type;
};


class ZXGraph{
    public:
        ZXGraph(size_t id, void** ref = NULL) : _id(id), _ref(ref), _nextVId(0), _tensor(1.+0.i){
            _globalDFScounter = 1;
        }
        
        ~ZXGraph() { for(const auto& v: _vertices) delete v; }


        // Getter and Setter
        void setId(size_t id)                                           { _id = id; }
        void setRef(void** ref)                                         { _ref = ref; }
        
        void setInputs(const ZXVertexList& inputs)                      { _inputs = inputs; }
        void setOutputs(const ZXVertexList& outputs)                    { _outputs = outputs; }
        void setVertices(const ZXVertexList& vertices)                  { _vertices = vertices; }
        
        const size_t& getId() const                                     { return _id; }
        void** getRef() const                                           { return _ref; }
        const size_t& getNextVId() const                                { return _nextVId; }
        const ZXVertexList& getInputs() const                           { return _inputs; }
        const ZXVertexList& getOutputs() const                          { return _outputs; }
        const ZXVertexList& getVertices() const                         { return _vertices; }
        size_t getNumEdges() const;
        size_t getNumInputs() const                                     { return _inputs.size(); }
        size_t getNumOutputs() const                                    { return _outputs.size(); }
        size_t getNumVertices() const                                   { return _vertices.size(); }

        // For testings
        bool isEmpty() const;
        bool isValid() const;
        void generateCNOT();
        // REVIEW unused
        // bool isConnected(ZXVertex* v1, ZXVertex* v2) const; 
        bool isId(size_t id) const;
        bool isGraphLike() const;
        bool isInputQubit(int qubit) const                              { return (_inputList.contains(qubit)); }
        bool isOutputQubit(int qubit) const                             { return (_outputList.contains(qubit)); }


        // Add and Remove
        ZXVertex* addInput(int qubit, bool checked = false);
        ZXVertex* addOutput(int qubit, bool checked = false);
        ZXVertex* addVertex(int qubit, VertexType ZXVertex, Phase phase = Phase(), bool checked = false);

        void addInputs(const ZXVertexList& inputs);
        void addOutputs(const ZXVertexList& outputs);
        EdgePair addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType et);
        void addVertices(const ZXVertexList& vertices, bool reordered = false);
        
        size_t removeIsolatedVertices();
        size_t removeVertex(ZXVertex* v);
        size_t removeVertices(vector<ZXVertex* > vertices);
        size_t removeEdge(const EdgePair& ep);
        size_t removeEdge(ZXVertex* vs, ZXVertex* vt, EdgeType etype);
        size_t removeEdges(const vector<EdgePair>& eps);
        size_t removeAllEdgesBetween(ZXVertex* vs, ZXVertex* vt, bool checked = false);
        


        // Operation on graph
        void adjoint();
        void assignBoundary(size_t qubit, bool input, VertexType type, Phase phase);



        // Find functions
        size_t findNextId() const;
        ZXVertex* findVertexById(const size_t& id) const;



        // Action
        void reset();
        void sortIOByQubit(); 
        ZXGraph* copy() const;
        void toggleEdges(ZXVertex* v);
        void liftQubit(const size_t& n);
        ZXGraph* compose(ZXGraph* target);
        ZXGraph* tensorProduct(ZXGraph* target);
        unordered_map<size_t, ZXVertex*> id2VertexMap() const;
        void mergeInputList(unordered_map<size_t, ZXVertex*> lst)    { _inputList.merge(lst); }
        void mergeOutputList(unordered_map<size_t, ZXVertex*> lst)   { _outputList.merge(lst); }



        // Print functions
        //REVIEW provides filters?
        void printGraph() const;
        void printInputs() const;
        void printOutputs() const;
        void printIO() const;
        void printVertices() const;
        void printVertices(vector<unsigned> cand) const;
        void printQubits(vector<unsigned> cand) const;
        void printEdges() const;

        

        // For mapping (in zxMapping.cpp)
        void toTensor();
        ZXVertexList getNonBoundary();
        ZXVertex* getInputFromHash(const size_t& q);
        ZXVertex* getOutputFromHash(const size_t& q);
        void concatenate(ZXGraph* tmp, bool remove_imm = false);
        void setInputHash(const size_t& q, ZXVertex* v)                    { _inputList[q] = v; }
        void setOutputHash(const size_t& q, ZXVertex* v)                   { _outputList[q] = v; }
        void setInputList(const unordered_map<size_t, ZXVertex*>& lst)     { _inputList = lst; }
        void setOutputList(const unordered_map<size_t, ZXVertex*>& lst)    { _outputList = lst; }
        const unordered_map<size_t, ZXVertex*>& getInputList() const       { return _inputList; }
        const unordered_map<size_t, ZXVertex*>& getOutputList() const      { return _outputList; }



        // I/O (in zxIO.cpp)
        bool readZX(string, bool bzx = false);
        bool writeZX(string, bool complete = false, bool bzx = false);



        // Traverse (in zxTraverse.cpp)
        void updateTopoOrder();
        template<typename F>
        void topoTraverse(F lambda){
            updateTopoOrder();
            for_each(_topoOrder.begin(),_topoOrder.end(),lambda);
        }
        template<typename F>
        void forEachEdge(F lambda) const {
            for (auto v : _vertices) {
                for (auto [nb, etype] : v->getNeighbors()) {
                    if(nb->getId() > v->getId())
                        lambda(makeEdgePair(v, nb, etype));
                }
            }
        }

    private:
        size_t                            _id;
        void**                            _ref;
        size_t                            _nextVId;
        QTensor<double>                   _tensor;
        ZXVertexList                      _inputs;
        ZXVertexList                      _outputs;
        ZXVertexList                      _vertices;
        vector<ZXVertex*>                 _topoOrder;
        unordered_map<size_t, ZXVertex*>  _inputList;
        unordered_map<size_t, ZXVertex*>  _outputList;
        unsigned                          _globalDFScounter;
        
        void DFS(ZXVertex*);

};

EdgeType    str2EdgeType(const string& str);
VertexType  str2VertexType(const string& str);
string      EdgeType2Str(const EdgeType& et);
string      VertexType2Str(const VertexType& vt);
EdgeType    toggleEdge(const EdgeType& et);


template <typename T>
ostream& operator<<(typename enable_if<is_enum<T>::value, ostream>::type& stream, const T& e) {
    return stream << static_cast<typename underlying_type<T>::type>(e);
}


EdgePair makeEdgePair(ZXVertex* v1, ZXVertex* v2, EdgeType et);
EdgePair makeEdgePair(EdgePair epair);
EdgePair makeEdgePairDummy();

EdgePair_depr makeEdgeKey_depr(ZXVertex* v1, ZXVertex* v2, EdgeType* et);
EdgePair_depr makeEdgeKey_depr(EdgePair_depr epair);

#endif