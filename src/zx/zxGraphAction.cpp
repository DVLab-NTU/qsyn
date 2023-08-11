/****************************************************************************
  FileName     [ zxGraphAction.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <iostream>
#include <queue>

#include "./zxDef.hpp"
#include "./zxGraph.hpp"
#include "util/phase.hpp"
#include "util/textFormat.hpp"

using namespace std;
namespace TF = TextFormat;
extern size_t verbose;

/**
 * @brief Sort _inputs and _outputs of graph by qubit (ascending)
 *
 */
void ZXGraph::sortIOByQubit() {
    _inputs.sort([](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
    _outputs.sort([](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
}

/**
 * @brief Toggle a vertex between type Z and X, and toggle the edges adjacent to `v`. ( H -> S / S -> H)
 *        Ex: [(3, S), (4, H), (5, S)] -> [(3, H), (4, S), (5, H)]
 *
 * @param v
 */
void ZXGraph::toggleVertex(ZXVertex* v) {
    if (!v->isZ() && !v->isX()) return;
    Neighbors toggledNeighbors;
    for (auto& itr : v->getNeighbors()) {
        toggledNeighbors.insert(make_pair(itr.first, toggleEdge(itr.second)));
        itr.first->removeNeighbor(make_pair(v, itr.second));
        itr.first->addNeighbor(make_pair(v, toggleEdge(itr.second)));
    }
    v->setNeighbors(toggledNeighbors);
    v->setType(v->getType() == VertexType::Z ? VertexType::X : VertexType::Z);
}

/**
 * @brief Lift each vertex's qubit in ZXGraph with `n`.
 *        Ex: origin: 0 -> after lifting: n
 *
 * @param n
 */
void ZXGraph::liftQubit(const size_t& n) {
    for (const auto& v : _vertices) {
        v->setQubit(v->getQubit() + n);
    }

    unordered_map<size_t, ZXVertex*> newInputList, newOutputList;

    for_each(_inputList.begin(), _inputList.end(),
             [&n, &newInputList](pair<size_t, ZXVertex*> itr) {
                 newInputList[itr.first + n] = itr.second;
             });
    for_each(_outputList.begin(), _outputList.end(),
             [&n, &newOutputList](pair<size_t, ZXVertex*> itr) {
                 newOutputList[itr.first + n] = itr.second;
             });

    _inputList = newInputList;
    _outputList = newOutputList;
}

/**
 * @brief Compose `target` to the original ZXGraph (horizontal concat)
 *
 * @param target
 * @return ZXGraph*
 */
ZXGraph& ZXGraph::compose(ZXGraph const& target) {
    // Check ori-outputNum == target-inputNum
    if (this->getNumOutputs() != target.getNumInputs()) {
        cerr << "Error: The composing ZXGraph's #input is not equivalent to the original ZXGraph's #output." << endl;
        return *this;
    }

    ZXGraph copiedGraph{target};

    // Get maximum column in `this`
    unsigned maxCol = 0;
    for (const auto& o : this->getOutputs()) {
        if (o->getCol() > maxCol) maxCol = o->getCol();
    }

    // Update `_col` of copiedGraph to make them unique to the original graph
    for (const auto& v : copiedGraph.getVertices()) {
        v->setCol(v->getCol() + maxCol + 1);
    }

    // Sort ori-output and copy-input
    this->sortIOByQubit();
    copiedGraph.sortIOByQubit();

    // Change ori-output and copy-inputs' vt to Z and link them respectively

    auto itr_ori = _outputs.begin();
    auto itr_cop = copiedGraph.getInputs().begin();
    for (; itr_ori != _outputs.end(); ++itr_ori, ++itr_cop) {
        (*itr_ori)->setType(VertexType::Z);
        (*itr_cop)->setType(VertexType::Z);
        this->addEdge((*itr_ori), (*itr_cop), EdgeType::SIMPLE);
    }

    _outputs = copiedGraph._outputs;
    _outputList = copiedGraph._outputList;

    this->moveVerticesFrom(copiedGraph);

    return *this;
}

/**
 * @brief Tensor `target` to the original ZXGraph (vertical concat)
 *
 * @param target
 * @return ZXGraph*
 */
ZXGraph& ZXGraph::tensorProduct(ZXGraph const& target) {
    ZXGraph copiedGraph{target};

    // Lift Qubit
    int oriMaxQubit = INT_MIN, oriMinQubit = INT_MAX;
    int copiedMinQubit = INT_MAX;
    for (const auto& i : getInputs()) {
        if (i->getQubit() > oriMaxQubit) oriMaxQubit = i->getQubit();
        if (i->getQubit() < oriMinQubit) oriMinQubit = i->getQubit();
    }
    for (const auto& i : getOutputs()) {
        if (i->getQubit() > oriMaxQubit) oriMaxQubit = i->getQubit();
        if (i->getQubit() < oriMinQubit) oriMinQubit = i->getQubit();
    }

    for (const auto& i : copiedGraph.getInputs()) {
        if (i->getQubit() < copiedMinQubit) copiedMinQubit = i->getQubit();
    }
    for (const auto& i : copiedGraph.getOutputs()) {
        if (i->getQubit() < copiedMinQubit) copiedMinQubit = i->getQubit();
    }
    size_t liftQ = (oriMaxQubit - oriMinQubit + 1) - copiedMinQubit;
    copiedGraph.liftQubit(liftQ);

    // Merge copiedGraph to original graph
    _inputs.insert(copiedGraph._inputs.begin(), copiedGraph._inputs.end());
    _inputList.merge(copiedGraph._inputList);
    _outputs.insert(copiedGraph._outputs.begin(), copiedGraph._outputs.end());
    _outputList.merge(copiedGraph._outputList);

    this->moveVerticesFrom(copiedGraph);

    return *this;
}

/**
 * @brief Check if v is a gadget leaf
 *
 * @param v
 * @return true
 * @return false
 */
bool ZXGraph::isGadgetLeaf(ZXVertex* v) const {
    if (v->getType() != VertexType::Z ||
        v->getNumNeighbors() != 1 ||
        v->getFirstNeighbor().first->getType() != VertexType::Z ||
        v->getFirstNeighbor().second != EdgeType::HADAMARD ||
        !v->getFirstNeighbor().first->hasNPiPhase()) {
        if (verbose >= 5) cout << "Note: (" << v->getId() << ") is not a gadget leaf vertex!" << endl;
        return false;
    }
    return true;
}

/**
 * @brief Check if v is a gadget axel
 *
 * @param v
 * @return true
 * @return false
 */
bool ZXGraph::isGadgetAxel(ZXVertex* v) const {
    return v->isZ() &&
           v->hasNPiPhase() &&
           any_of(v->getNeighbors().begin(), v->getNeighbors().end(),
                  [](const NeighborPair& nbp) {
                      return nbp.first->getNumNeighbors() == 1 && nbp.first->isZ() && nbp.second == EdgeType::HADAMARD;
                  });
}

bool ZXGraph::hasDanglingNeighbors(ZXVertex* v) const {
    return any_of(v->getNeighbors().begin(), v->getNeighbors().end(),
                  [](const NeighborPair& nbp) {
                      return nbp.first->getNumNeighbors() == 1;
                  });
}

/**
 * @brief Add phase gadget of phase `p` for each vertex in `verVec`.
 *
 * @param p
 * @param verVec
 */
void ZXGraph::addGadget(Phase p, const vector<ZXVertex*>& verVec) {
    for (size_t i = 0; i < verVec.size(); i++) {
        if (verVec[i]->getType() == VertexType::BOUNDARY || verVec[i]->getType() == VertexType::H_BOX) return;
    }

    ZXVertex* axel = addVertex(-1, VertexType::Z, Phase(0));
    ZXVertex* leaf = addVertex(-2, VertexType::Z, p);

    addEdge(axel, leaf, EdgeType::HADAMARD);
    for (const auto& v : verVec) addEdge(v, axel, EdgeType::HADAMARD);
    if (verbose >= 5) cout << "Add phase gadget (" << leaf->getId() << ") to graph!" << endl;
}

/**
 * @brief Remove phase gadget `v`. (Auto-check if `v` is a gadget first!)
 *
 * @param v
 */
void ZXGraph::removeGadget(ZXVertex* v) {
    if (!isGadgetLeaf(v)) return;
    ZXVertex* axel = v->getFirstNeighbor().first;
    removeVertex(axel);
    removeVertex(v);
}

/**
 * @brief Generate a id-2-ZXVertex* map
 *
 * @return unordered_map<size_t, ZXVertex*>
 */
unordered_map<size_t, ZXVertex*> ZXGraph::id2VertexMap() const {
    unordered_map<size_t, ZXVertex*> id2VertexMap;
    for (const auto& v : _vertices) id2VertexMap[v->getId()] = v;
    return id2VertexMap;
}

/**
 * @brief Rearrange nodes on each qubit so that each node can be seperated in the printed graph.
 *
 */
void ZXGraph::normalize() {
    unordered_map<int, vector<ZXVertex*> > mp;
    unordered_set<int> vis;
    queue<ZXVertex*> cand;
    for (const auto& i : _inputs) {
        cand.push(i);
        vis.insert(i->getId());
    }
    while (!cand.empty()) {
        ZXVertex* node = cand.front();
        cand.pop();
        mp[node->getQubit()].emplace_back(node);
        for (const auto& n : node->getNeighbors()) {
            if (vis.find(n.first->getId()) == vis.end()) {
                cand.push(n.first);
                vis.insert(n.first->getId());
            }
        }
    }
    int maxCol = 0;
    for (auto& i : mp) {
        int col = 0;
        for (auto& v : i.second) {
            v->setCol(col);
            col++;
        }
        col--;
        maxCol = max(maxCol, col);
    }
    for (auto& o : _outputs) o->setCol(maxCol);
}