/****************************************************************************
  FileName     [ zxGraphPrint.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph Print functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <unordered_set>
#include <vector>

#include "textFormat.h"
#include "util.h"
#include "zxGraph.h"

using namespace std;
namespace TF = TextFormat;
extern size_t verbose;

/**
 * @brief Print information of ZX-graph
 *
 */
void ZXGraph::printGraph() const {
    cout << "Graph " << _id << "( "
         << getNumInputs() << " inputs, "
         << getNumOutputs() << " outputs, "
         << getNumVertices() << " vertices, "
         << getNumEdges() << " edges )\n";
    // cout << setw(15) << left << "Inputs: " << getNumInputs() << endl;
    // cout << setw(15) << left << "Outputs: " << getNumOutputs() << endl;
    // cout << setw(15) << left << "Vertices: " << getNumVertices() << endl;
    // cout << setw(15) << left << "Edges: " << getNumEdges() << endl;
}

/**
 * @brief Print Inputs of ZX-graph
 *
 */
void ZXGraph::printInputs() const {
    cout << "Input ( ";
    for (const auto& v : _inputs) cout << v->getId() << " ";
    cout << ")\nTotal #Inputs: " << getNumInputs() << endl;
}

/**
 * @brief Print Outputs of ZX-graph
 *
 */
void ZXGraph::printOutputs() const {
    cout << "Output ( ";
    for (const auto& v : _outputs) cout << v->getId() << " ";
    cout << ")\nTotal #Outputs: " << getNumOutputs() << endl;
}

/**
 * @brief Print Inputs and Outputs of ZX-graph
 *
 */
void ZXGraph::printIO() const {
    cout << "Input ( ";
    for (const auto& v : _inputs) cout << v->getId() << " ";
    cout << ")\nOutput ( ";
    for (const auto& v : _outputs) cout << v->getId() << " ";
    cout << ")\nTotal #(I,O): (" << getNumInputs() << "," << getNumOutputs() << ")\n";
}

/**
 * @brief Print Vertices of ZX-graph
 *
 */
void ZXGraph::printVertices() const {
    cout << "\n";
    for (const auto& v : _vertices) {
        v->printVertex();
    }
    cout << "Total #Vertices: " << getNumVertices() << endl;
    cout << "\n";
}

/**
 * @brief Print Vertices of ZX-graph in `cand`.
 *
 * @param cand
 */
void ZXGraph::printVertices(vector<unsigned> cand) const {
    unordered_map<size_t, ZXVertex*> id2Vmap = id2VertexMap();

    cout << "\n";
    for (size_t i = 0; i < cand.size(); i++) {
        if (isId(cand[i])) id2Vmap[((size_t)cand[i])]->printVertex();
    }
    cout << "\n";
}

/**
 * @brief Print Vertices of ZX-graph in `cand` by qubit.
 *
 * @param cand
 */
void ZXGraph::printQubits(vector<int> cand) const {
    map<int, vector<ZXVertex*> > q2Vmap;
    for (const auto& v : _vertices) {
        if (!q2Vmap.contains(v->getQubit())) {
            vector<ZXVertex*> tmp(1, v);
            q2Vmap[v->getQubit()] = tmp;
        } else
            q2Vmap[v->getQubit()].push_back(v);
    }
    if (cand.empty()) {
        for (const auto& [key, vec] : q2Vmap) {
            cout << "\n";
            for (const auto& v : vec) v->printVertex();
            cout << "\n";
        }
    } else {
        for (size_t i = 0; i < cand.size(); i++) {
            if (q2Vmap.contains(cand[i])) {
                cout << "\n";
                for (const auto& v : q2Vmap[cand[i]]) v->printVertex();
            }
            cout << "\n";
        }
    }
}

/**
 * @brief Print Edges of ZX-graph
 *
 */
void ZXGraph::printEdges() const {
    forEachEdge([](const EdgePair& epair) {
        cout << "( " << epair.first.first->getId() << ", " << epair.first.second->getId() << " )\tType:\t" << EdgeType2Str(epair.second) << endl;
    });
    cout << "Total #Edges: " << getNumEdges() << endl;
}
