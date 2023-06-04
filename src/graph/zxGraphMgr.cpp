/****************************************************************************
  FileName     [ zxGraphMgr.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX-graph manager ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxGraphMgr.h"

#include <cstddef>  // for size_t, NULL
#include <iomanip>
#include <iostream>

#include "zxGraph.h"  // for ZXGraph

using namespace std;

ZXGraphMgr* zxGraphMgr = 0;
extern size_t verbose;

/*****************************************/
/*   class ZXGraphMgr member functions   */
/*****************************************/

/**
 * @brief reset ZXGraphMgr
 *
 */
void ZXGraphMgr::reset() {
    for (auto& graph : _graphList) {
        delete graph;
    }
    _graphList.clear();
    _gListItr = _graphList.begin();
    _nextID = 0;
}

// Test

/**
 * @brief Check if `id` is an existed ID in ZXGraphMgr
 *
 * @param id
 * @return true
 * @return false
 */
bool ZXGraphMgr::isID(size_t id) const {
    for (size_t i = 0; i < _graphList.size(); i++) {
        if (_graphList[i]->getId() == id) return true;
    }
    return false;
}

void ZXGraphMgr::setGraph(ZXGraph* g) {
    delete _graphList[_gListItr - _graphList.begin()];
    _graphList[_gListItr - _graphList.begin()] = g;
    g->setId(_gListItr - _graphList.begin());
}

// Add and Remove

/**
 * @brief Add a ZX-graph to
 *
 * @param id
 * @param ref
 * @return ZXGraph*
 */
ZXGraph* ZXGraphMgr::addZXGraph(size_t id) {
    ZXGraph* zxGraph = new ZXGraph(id);
    _graphList.push_back(zxGraph);
    _gListItr = _graphList.end() - 1;
    if (id == _nextID || _nextID < id) _nextID = id + 1;
    if (verbose >= 3) {
        cout << "Create and checkout to Graph " << id << endl;
    }
    return zxGraph;
}

/**
 * @brief Remove a ZXGraph
 *
 * @param id the id to be removed
 */
void ZXGraphMgr::removeZXGraph(size_t id) {
    for (size_t i = 0; i < _graphList.size(); i++) {
        if (_graphList[i]->getId() == id) {
            ZXGraph* rmGraph = _graphList[i];
            _graphList.erase(_graphList.begin() + i);
            delete rmGraph;
            if (verbose >= 3) cout << "Successfully removed Graph " << id << endl;
            _gListItr = _graphList.begin();
            if (verbose >= 3) {
                if (!_graphList.empty())
                    cout << "Checkout to Graph " << _graphList[0]->getId() << endl;
                else
                    cout << "Note: The graph list is empty now" << endl;
            }
            return;
        }
    }
    cerr << "Error: The id provided does not exist!!" << endl;
    return;
}

// Action
/**
 * @brief Checkout to ZXGraph
 *
 * @param id the id to be checkout
 */
void ZXGraphMgr::checkout2ZXGraph(size_t id) {
    for (size_t i = 0; i < _graphList.size(); i++) {
        if (_graphList[i]->getId() == id) {
            _gListItr = _graphList.begin() + i;
            if (verbose >= 3) cout << "Checkout to Graph " << id << endl;
            return;
        }
    }
    cerr << "Error: The id provided does not exist!!" << endl;
    return;
}

/**
 * @brief Copy the ZXGraph
 *
 * @param id the id to be copied
 * @param toNew if true, check to the new graph
 */
void ZXGraphMgr::copy(size_t id, bool toNew) {
    if (_graphList.empty())
        cerr << "Error: ZXGraphMgr is empty now! Action \"copy\" failed!" << endl;
    else {
        size_t oriGraphID = getGraph()->getId();
        ZXGraph* copiedGraph = getGraph()->copy();
        copiedGraph->setId(id);
        copiedGraph->setFileName(getGraph()->getFileName());
        copiedGraph->addProcedure("", getGraph()->getProcedures());
        if (toNew) {
            _graphList.push_back(copiedGraph);
            _gListItr = _graphList.end() - 1;
            if (_nextID <= id) _nextID = id + 1;
            if (verbose >= 3) {
                cout << "Successfully copied Graph " << oriGraphID << " to Graph " << id << "\n";
                cout << "Checkout to Graph " << id << "\n";
            }
        } else {
            for (size_t i = 0; i < _graphList.size(); i++) {
                if (_graphList[i]->getId() == id) {
                    _graphList[i] = copiedGraph;
                    if (verbose >= 3) cout << "Successfully copied Graph " << oriGraphID << " to Graph " << id << endl;
                    checkout2ZXGraph(id);
                    break;
                }
            }
        }
    }
}

/**
 * @brief Find ZXGraph by id
 *
 * @param id
 * @return ZXGraph*
 */
ZXGraph* ZXGraphMgr::findZXGraphByID(size_t id) const {
    if (!isID(id))
        cerr << "Error: Graph " << id << " does not exist!" << endl;
    else {
        for (size_t i = 0; i < _graphList.size(); i++) {
            if (_graphList[i]->getId() == id) return _graphList[i];
        }
    }
    return nullptr;
}

// Print
/**
 * @brief Print number of graphs and the focused id
 *
 */
void ZXGraphMgr::printZXGraphMgr() const {
    cout << "-> #Graph: " << _graphList.size() << endl;
    if (!_graphList.empty()) cout << "-> Now focus on: " << getGraph()->getId() << endl;
}

/**
 * @brief Print list of graphs
 *
 */
void ZXGraphMgr::printGList() const {
    if (!_graphList.empty()) {
        for (auto& cir : _graphList) {
            if (cir->getId() == getGraph()->getId())
                cout << "★ ";
            else
                cout << "  ";
            cout << cir->getId() << "    " << left << setw(20) << cir->getFileName().substr(0, 20);
            for (size_t i = 0; i < cir->getProcedures().size(); i++) {
                if (i != 0) cout << " ➔ ";
                cout << cir->getProcedures()[i];
            }
            cout << endl;
        }
    } else
        cerr << "Error: ZXGraphMgr is empty now!" << endl;
}

/**
 * @brief Print the id of the focused graph
 *
 */
void ZXGraphMgr::printGListItr() const {
    if (!_graphList.empty())
        cout << "Now focus on: " << getGraph()->getId() << endl;
    else
        cerr << "Error: ZXGraphMgr is empty now!" << endl;
}

/**
 * @brief Print the number of graphs
 *
 */
void ZXGraphMgr::printGraphListSize() const {
    cout << "#Graph: " << _graphList.size() << endl;
}
