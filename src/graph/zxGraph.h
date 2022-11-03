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
#include <unordered_set>
#include <vector>
#include <string>
#include "phase.h"
#include "qtensor.h"
#include "zxDef.h"
#include <iterator>
using namespace std;

class ZXVertex{
    public:
        class EdgeIterator;
        friend class EdgeIterator;

        ZXVertex(size_t id, int qubit, VertexType vt, Phase phase = Phase()) {
            _id = id;
            _qubit = qubit;
            _type = vt;
            _phase = phase;
            _DFSCounter = 0;
            _pin = unsigned(-1);
            _neighborMap_depr.clear();
            _neighbors.clear();
        }
        ~ZXVertex(){}

        // Getter and Setter
        const size_t& getId() const                                         { return _id; }
        const int& getQubit() const                                         { return _qubit; }
        const VertexType& getType() const                                   { return _type; }
        const Phase& getPhase() const                                       { return _phase; }
        const size_t& getPin() const                                        { return _pin; }   
        
        vector<ZXVertex*> getNeighbors_depr() const;
        const Neighbors& getNeighbors() const                               { return _neighbors; }

        ZXVertex* getNeighbor_depr(size_t idx) const;
        const NeighborMap_depr& getNeighborMap() const                           { return _neighborMap_depr; }
        size_t getNumNeighbors_depr() const                                      { return _neighborMap_depr.size(); }
        size_t getNumNeighbors() const                                           { return _neighbors.size(); }
        QTensor<double> getTSform();

        void setId(const size_t& id)                                        { _id = id; }
        void setQubit(const int& q)                                         { _qubit = q; }
        void setType(const VertexType& ZXVertex)                            { _type = ZXVertex; }
        void setPhase(const Phase& p)                                       { _phase = p; }
        void setNeighborMap(const NeighborMap_depr& neighborMap)            { _neighborMap_depr = neighborMap; }
        void setPin(const size_t& p)                                        { _pin = p; }

        // Add and Remove
        void addNeighbor_depr(const NeighborPair_depr& nb)                  { _neighborMap_depr.insert(nb); }
        void addNeighbor(const NeighborPair& nb)                            { _neighbors.insert(nb); }
        void removeNeighbor(const NeighborPair& nb)                         { _neighbors.erase(nb); }


        // Print functions
        void printVertex_depr() const;
        void printVertex() const;
        void printNeighborMap_depr() const;
        void printNeighbors() const;

        
        // Action
        void disconnect_depr(ZXVertex* v, bool checked = false);
        void disconnect(ZXVertex* v, bool checked = false);

        // Test
        bool isNeighbor_depr(ZXVertex* v) const;
        bool isNeighbor(ZXVertex* v) const;
        bool isNeighbor(const NeighborPair& nb) const { return _neighbors.contains(nb); }
        bool isZ()        const { return getType() == VertexType::Z; }
        bool isX()        const { return getType() == VertexType::X; }
        bool isHBox()     const { return getType() == VertexType::H_BOX; }
        bool isBoundary() const { return getType() == VertexType::BOUNDARY; }
        
        // DFS
        bool isVisited(unsigned global) { return global == _DFSCounter; }
        void setVisited(unsigned global) { _DFSCounter = global; }
        static bool idLessThan(ZXVertex* a, ZXVertex* b) { return a->getId() < b->getId(); }

    private:
        int                                  _qubit;
        size_t                               _id;
        VertexType                           _type;
        Phase                                _phase;
        NeighborMap_depr                     _neighborMap_depr;
        Neighbors                            _neighbors;
        unsigned                             _DFSCounter;
        size_t                               _pin;
        
};

using ZXNeighborMap = unordered_map<ZXVertex*, EdgeType>;
using ZXVertexList  = unordered_set<ZXVertex*>;

class ZXGraph{
    public:
        ZXGraph(size_t id, void** ref = NULL) : _id(id), _ref(ref), _currentVertexId(0), _tensor(1.+0.i){
            _globalDFScounter = 1;
        }
        
        ~ZXGraph() {
            // for(size_t i = 0; i < _vertices.size(); i++) delete _vertices[i];
            for(const auto& ver: _vertices) delete ver;
        }


        // Getter and Setter
        void setId(size_t id)                                           { _id = id; }
        void setRef(void** ref)                                         { _ref = ref; }
        
        //REVIEW - SHOULD PASS BY CONST REF
        void setInputs_depr(vector<ZXVertex*> inputs)                   { _inputs_depr = inputs; }
        void setInputs(const ZXVertexList& inputs)                      { _inputs = inputs; }

        void setOutputs(vector<ZXVertex*> outputs)                      { _outputs_depr = outputs; }
        void SetOutputs(const ZXVertexList& outputs)                    { _outputs = outputs; }
        
        void setVertices(vector<ZXVertex*> vertices)                    { _vertices_depr = vertices; }
        void SetVertices(const ZXVertexList& vertices)                  { _vertices = vertices; }
        
        void setEdges(vector<EdgePair_depr > edges)                     { _edges_depr = edges; }
        
        const size_t& getId() const                                     { return _id; }
        void** getRef() const                                           { return _ref; }
        
        const vector<ZXVertex*>& getInputs_depr() const                      { return _inputs_depr; }
        const ZXVertexList& getInputs() const                           { return _inputs; }

        size_t getNumInputs_depr() const                                     { return _inputs_depr.size(); }
        size_t getNumInputs() const                                     { return _inputs.size(); }
        
        const vector<ZXVertex*>& getOutputs_depr() const                     { return _outputs_depr; }
        const ZXVertexList& getOutputs() const                          { return _outputs; }
        
        size_t getNumOutputs_depr() const                                    { return _outputs_depr.size(); }
        size_t getNumOutputs() const                                    { return _outputs.size(); }
        
        const vector<ZXVertex*>& getVertices_depr() const                    { return _vertices_depr; }
        const ZXVertexList& getVertices() const                         { return _vertices; }
        
        size_t getNumVertices_depr() const                                   { return _vertices_depr.size(); }
        size_t getNumVertices() const                                   { return _vertices.size(); }
        
        //REVIEW - delete: implement edge iterator instead
        const vector<EdgePair_depr>& getEdges() const                        { return _edges_depr; }
        //REVIEW - delete: identical to v->getNumNeighbors();
        size_t getNumIncidentEdges_depr(ZXVertex* v) const;
        //REVIEW - delete: now a member function of ZXVertex
        EdgePair_depr getFirstIncidentEdge_depr(ZXVertex* v) const;
        //REVIEW - delete: now a iterator of ZXVertex
        vector<EdgePair_depr> getIncidentEdges_depr(ZXVertex* v) const;
        
        size_t getNumEdges_depr() const                                      { return _edges_depr.size(); }
        size_t getNumEdges() const;

        //REVIEW - add: new function
        vector<ZXVertex*> getSortedListFromSet(const ZXVertexList& set) const;

        // For testing
        void generateCNOT();
        bool isEmpty() const;
        bool isValid() const;
        bool isConnected(ZXVertex* v1, ZXVertex* v2) const;
        bool isId_depr(size_t id) const;
        bool isId(size_t id) const;
        bool isGraphLike() const;
        bool isInputQubit(int qubit) const;
        bool isOutputQubit(int qubit) const;


        // Add and Remove
        ZXVertex* addInput_depr(size_t id, int qubit, bool checked = false);
        ZXVertex* addInput(int qubit, bool checked = false);

        ZXVertex* addOutput_depr(size_t id, int qubit, bool checked = false);
        ZXVertex* addOutput(int qubit, bool checked = false);

        ZXVertex* addVertex_depr(size_t id, int qubit, VertexType ZXVertex, Phase phase = Phase(), bool checked = false);
        ZXVertex* addVertex(int qubit, VertexType ZXVertex, Phase phase = Phase(), bool checked = false);
        
        void addInputs_depr(vector<ZXVertex*> inputs);
        void addInputs(const ZXVertexList& inputs);

        void addOutputs_depr(vector<ZXVertex*> outputs);
        void addOutputs(const ZXVertexList& outputs);

        void addVertices_depr(vector<ZXVertex*> vertices);
        void addVertices(const ZXVertexList& vertices);

        EdgePair_depr addEdge_depr(ZXVertex* vs, ZXVertex* vt, EdgeType* et, bool allowSelfLoop = false);
        //FIXME - 
        EdgePair addEdge_depr(ZXVertex* vs, ZXVertex* vt, EdgeType et);
        EdgePair addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType et);
        //FIXME - 
        void addEdgeById_depr(size_t id_s, size_t id_t, EdgeType* et);
        void addEdgeById(size_t id_s, size_t id_t, EdgeType et);
        //FIXME - 
        void addEdges(vector<EdgePair_depr> edges);
        
        void mergeInputList(unordered_map<size_t, ZXVertex*> lst);
        void mergeOutputList(unordered_map<size_t, ZXVertex*> lst);

        void removeVertex_depr(ZXVertex* v, bool checked = false);
        void removeVertex(ZXVertex* v, bool checked = false);

        //FIXME - 
        void removeVertices(vector<ZXVertex* > vertices, bool checked = false);
        void removeVertexById(const size_t& id);
        //FIXME - 
        void removeIsolatedVertices_depr();
        //FIXME - 
        void removeEdge_depr(ZXVertex* vs, ZXVertex* vt, bool checked = false);
        void removeAllEdgeBetween(ZXVertex* vs, ZXVertex* vt, bool checked = false);
        void removeEdgeByEdgePair_depr(const EdgePair_depr& ep);
        void removeEdge(const EdgePair& ep);
        void removeEdgesByEdgePairs_depr(const vector<EdgePair_depr>& eps);
        void removeEdgeById(const size_t& id_s, const size_t& id_t, EdgeType etype = EdgeType::ERRORTYPE);

        // Operation on graph
        void adjoint();
        void assignBoundary(size_t qubit, bool input, VertexType type, Phase phase);

                
        // Find functions
        ZXVertex* findVertexById_depr(const size_t& id) const;
        ZXVertex* findVertexById(const size_t& id) const;

        size_t findNextId() const;


        // Action
        void reset();
        ZXGraph* copy() const;
        void sortIOByQubit();
        void sortVerticeById();
        void liftQubit(const size_t& n);


        // Print functions
        void printGraph_depr() const;
        void printGraph() const;
        void printInputs_depr() const;
        void printInputs() const;
        void printOutputs_depr() const;
        void printOutputs() const;
        void printVertices_depr() const;
        void printVertices() const;
        void printEdges_depr() const;
        void printEdges() const;
        //REVIEW - unused function
        void printEdge_depr(size_t idx) const;
        
        // Traverse
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
                    lambda(makeEdgePair(v, nb, etype));
                }
            }
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
        vector<EdgePair_depr> getInnerEdges();
        void cleanRedundantEdges();

        // I/O
        //REVIEW - overhauled: add complete mode (print all neighbors) to write
        bool readZX(string, bool bzx = false);
        bool writeZX(string, bool complete = false, bool bzx = false);

    private:
        size_t                            _id;
        void**                            _ref;
        size_t                            _currentVertexId; // keep the index
        QTensor<double>                   _tensor;
        vector<ZXVertex*>                 _inputs_depr;
        vector<ZXVertex*>                 _outputs_depr;
        vector<ZXVertex*>                 _vertices_depr;
        vector<EdgePair_depr >                 _edges_depr;
        unordered_map<size_t, ZXVertex*>  _inputList;
        unordered_map<size_t, ZXVertex*>  _outputList;
        vector<ZXVertex*>                 _topoOrder;
        unsigned                          _globalDFScounter;
        ZXVertexList                      _vertices; // captical for reconstruction
        ZXVertexList                      _inputs;   // captical for reconstruction
        ZXVertexList                      _outputs;  // captical for reconstruction
        void DFS(ZXVertex*);

};

VertexType  str2VertexType(const string& str);
string      VertexType2Str(const VertexType& vt);
EdgeType    str2EdgeType(const string& str);
string      EdgeType2Str(const EdgeType& et);
EdgeType    toggleEdge(const EdgeType& et);


EdgeType*   str2EdgeType_depr(const string& str);
string      EdgeType2Str_depr(const EdgeType* et);

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