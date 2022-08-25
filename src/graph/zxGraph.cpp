/****************************************************************************
  FileName     [ zxGraph.h ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph member functions ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <vector>
#include "zxGraph.h"

using namespace std;

/**************************************/
/*   class VT member functions   */
/**************************************/

// Print functions

void VT::printVertex() const{
    cout << "ID:\t" << _id << "\t";
    cout << "VertexType:\t" << _type << "\t";
    cout << "Qubit:\t" << _qubit << "\t";
    cout << "#Neighbors:\t" << _neighbors.size() << "\t";
    cout << endl;;
}


/**************************************/
/*   class ZXGraph member functions   */
/**************************************/

// For testing
void ZXGraph::generateCNOT(){
    cout << "Generate a 2-qubit CNOT graph for testing" << endl;
    // Generate Inputs
    vector<VT*> inputs, outputs, vertices;
    for(size_t i = 0; i < 2; i++){
        VT* input = new VT(vertices.size(), i, i, VertexType::BOUNDARY);
        inputs.push_back(input);
        vertices.push_back(input);
    }

    // Generate CNOT
    VT* z_spider = new VT(vertices.size(), 0, 0, VertexType::Z);
    vertices.push_back(z_spider);
    VT* x_spider = new VT(vertices.size(), 1, 1, VertexType::X);
    vertices.push_back(x_spider);

    // Generate Outputs
    for(size_t i = 0; i < 2; i++){
        VT* output = new VT(vertices.size(), i, i, VertexType::BOUNDARY);
        outputs.push_back(output);
        vertices.push_back(output);
    }
    setInputs(inputs);
    setOutputs(outputs);
    setVertices(vertices);

    // Generate edges [(0,2), (1,3), (2,3), (2,4), (3,5)]
    vector<pair<size_t, size_t> > edgeList;
    edgeList.push_back(make_pair(0,2));
    edgeList.push_back(make_pair(1,3));
    edgeList.push_back(make_pair(2,3));
    edgeList.push_back(make_pair(2,4));
    edgeList.push_back(make_pair(3,5));

    vector<pair<pair<VT*, VT*>, EdgeType> > edges;

    for(size_t i = 0; i < edgeList.size(); i++){
        if(findVertexById(edgeList[i].first) != nullptr && findVertexById(edgeList[i].second) != nullptr){
            VT* s = findVertexById(edgeList[i].first); VT* t = findVertexById(edgeList[i].second);
            s->addNeighbor(make_pair(t, EdgeType::SIMPLE));
            t->addNeighbor(make_pair(s, EdgeType::SIMPLE));
            edges.push_back(make_pair(make_pair(s, t), EdgeType::SIMPLE));
        }
    }
    setEdges(edges);
}


// Find functions

VT* ZXGraph::findVertexById(size_t id) const{
    for(size_t i = 0; i < _vertices.size(); i++){
        if(_vertices[i]->getId() == id) return _vertices[i];
    }
    return nullptr;
}


// Print functions

void ZXGraph::printGraph() const{
    cout << "Graph " << _id << endl;
    cout << "Inputs: \t" << _inputs.size() << endl;
    cout << "Outputs: \t" << _outputs.size() << endl;
    cout << "Vertices: \t" << _vertices.size() << endl;
}

void ZXGraph::printInputs() const{
    for(size_t i = 0; i < _inputs.size(); i++){
        cout << "Input " << i+1 << ":\t" << _inputs[i]->getId() << endl;
    }
}

void ZXGraph::printOutputs() const{
    for(size_t i = 0; i < _outputs.size(); i++){
        cout << "Output " << i+1 << ":\t" << _outputs[i]->getId() << endl;
    }
}

void ZXGraph::printVertices() const{
    for(size_t i = 0; i < _vertices.size(); i++){
        cout << "Vertex " << i+1 << ":\t";
        _vertices[i]->printVertex();
    }
}

void ZXGraph::printEdges() const{
    for(size_t i = 0; i < _edges.size(); i++){
        cout << "Edge " << i+1 << ":\t";
        cout << "( " << _edges[i].first.first->getId() << ", " <<  _edges[i].first.second->getId() << " )\tType:\t" << _edges[i].second << endl; 
    }
}