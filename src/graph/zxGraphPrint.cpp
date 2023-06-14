/****************************************************************************
  FileName     [ zxGraphPrint.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph Print functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t
#include <iostream>
#include <map>
#include <string>

#include "textFormat.h"
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
void ZXGraph::printVertices(vector<size_t> cand) const {
    unordered_map<size_t, ZXVertex*> id2Vmap = id2VertexMap();

    cout << "\n";
    for (size_t i = 0; i < cand.size(); i++) {
        if (isId(cand[i])) id2Vmap[cand[i]]->printVertex();
    }
    cout << "\n";
}

/**
 * @brief Print Vertices of ZX-graph in `cand` by qubit.
 *
 * @param cand
 */
void ZXGraph::printQubits(vector<int> cand) const {
    map<int, vector<ZXVertex*>> q2Vmap;
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

/**
 * @brief For each vertex ID, print the vertices that only present in one of the graph,
 *        or vertices that differs in the neighbors. This is not a graph isomorphism detector!!!
 *
 * @param other
 */
void ZXGraph::printDifference(ZXGraph* other) const {
    assert(other != nullptr);

    size_t nIDs = max(_nextVId, other->_nextVId);
    ZXVertexList v1s, v2s;
    for (size_t i = 0; i < nIDs; ++i) {
        auto v1 = findVertexById(i);
        auto v2 = other->findVertexById(i);
        if (v1 && v2) {
            if (v1->getNumNeighbors() != v2->getNumNeighbors() ||
                std::invoke([&v1, &v2, &other]() -> bool {
                    for (auto& [nb1, e1] : v1->getNeighbors()) {
                        ZXVertex* nb2 = other->findVertexById(nb1->getId());
                        if (!nb2) return true;
                        if (!nb2->isNeighbor(v2, e1)) return true;
                    }
                    return false;
                })) {
                v1s.insert(v1);
                v2s.insert(v2);
            }
        } else if (v1) {
            v1s.insert(v1);
        } else if (v2) {
            v2s.insert(v2);
        }
    }
    cout << ">>>" << endl;
    for (auto& v : v1s) {
        v->printVertex();
    }
    cout << "===" << endl;
    for (auto& v : v2s) {
        v->printVertex();
    }
    cout << "<<<" << endl;
}

/**
 * @brief Print the vertex with color
 *
 * @param v
 * @return string
 */
string printColoredVertex(ZXVertex* v) {
    if (v->getType() == VertexType::BOUNDARY)
        return to_string(v->getId());
    else if (v->getType() == VertexType::Z)
        return TF::BOLD(TF::GREEN(to_string(v->getId())));
    else if (v->getType() == VertexType::X)
        return TF::BOLD(TF::RED(to_string(v->getId())));
    else if (v->getType() == VertexType::H_BOX)
        return TF::BOLD(TF::YELLOW(to_string(v->getId())));
    else
        return to_string(v->getId());
}

/**
 * @brief Draw ZX-graph in CLI
 *
 */
void ZXGraph::draw() const {
    cout << endl;
    unsigned int maxCol = 0;  // number of columns -1
    unordered_map<int, int> qPair;
    vector<int> qubitNum;  // number of qubit

    // maxCol
    for (auto& o : getOutputs()) {
        if (o->getCol() > maxCol) maxCol = o->getCol();
    }

    // qubitNum
    vector<int> qubitNum_temp;  // number of qubit
    for (auto& v : getVertices()) {
        qubitNum_temp.push_back(v->getQubit());
    }
    sort(qubitNum_temp.begin(), qubitNum_temp.end());
    if (qubitNum_temp.size() == 0) {
        cout << "Empty graph!!" << endl;
        return;
    }
    size_t offset = qubitNum_temp[0];
    qubitNum.push_back(0);
    for (size_t i = 1; i < qubitNum_temp.size(); i++) {
        if (qubitNum_temp[i - 1] == qubitNum_temp[i]) {
            continue;
        } else {
            qubitNum.push_back(qubitNum_temp[i] - offset);
        }
    }
    qubitNum_temp.clear();

    for (size_t i = 0; i < qubitNum.size(); i++) qPair[i] = qubitNum[i];
    vector<ZXVertex*> tmp;
    tmp.resize(qubitNum.size());
    vector<vector<ZXVertex*>> colList(maxCol + 1, tmp);

    for (auto& v : getVertices()) colList[v->getCol()][qPair[v->getQubit() - offset]] = v;

    vector<size_t> maxLength(maxCol + 1, 0);
    for (size_t i = 0; i < colList.size(); i++) {
        for (size_t j = 0; j < colList[i].size(); j++) {
            if (colList[i][j] != nullptr) {
                if (to_string(colList[i][j]->getId()).length() > maxLength[i]) maxLength[i] = to_string(colList[i][j]->getId()).length();
            }
        }
    }
    size_t maxLengthQ = 0;
    for (size_t i = 0; i < qubitNum.size(); i++) {
        int temp = offset + i;
        if (to_string(temp).length() > maxLengthQ) maxLengthQ = to_string(temp).length();
    }

    for (size_t i = 0; i < qubitNum.size(); i++) {
        // print qubit
        int temp = offset + i;
        cout << "[";
        for (size_t i = 0; i < maxLengthQ - to_string(temp).length(); i++) {
            cout << " ";
        }
        cout << temp << "]";

        // print row
        for (size_t j = 0; j <= maxCol; j++) {
            if (i < -offset) {
                if (colList[j][i] != nullptr) {
                    cout << "(" << printColoredVertex(colList[j][i]) << ")   ";
                } else {
                    if (j == maxCol)
                        cout << endl;
                    else {
                        cout << "   ";
                        for (size_t k = 0; k < maxLength[j] + 2; k++) cout << " ";
                    }
                }
            } else if (colList[j][i] != nullptr) {
                if (j == maxCol)
                    cout << "(" << printColoredVertex(colList[j][i]) << ")" << endl;
                else
                    cout << "(" << printColoredVertex(colList[j][i]) << ")---";

                for (size_t k = 0; k < maxLength[j] - to_string(colList[j][i]->getId()).length(); k++) cout << "-";
            } else {
                cout << "---";
                for (size_t k = 0; k < maxLength[j] + 2; k++) cout << "-";
            }
        }
        cout << endl;
    }
    for (auto& a : colList) {
        a.clear();
    }
    colList.clear();

    maxLength.clear();
    qubitNum.clear();
}
