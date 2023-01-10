/****************************************************************************
  FileName     [ zxGraphMgr.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX-graph manager ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxGraphMgr.h"

#include <cstddef>  // for size_t, NULL
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
ZXGraph* ZXGraphMgr::addZXGraph(size_t id, void** ref) {
    ZXGraph* zxGraph = new ZXGraph(id, ref);
    _graphList.push_back(zxGraph);
    _gListItr = _graphList.end() - 1;
    if (id == _nextID || _nextID < id) _nextID = id + 1;
    if (verbose >= 3) {
        cout << "Create and checkout to Graph " << id << endl;
    }
    return zxGraph;
}

void ZXGraphMgr::removeZXGraph(size_t id) {
    for (size_t i = 0; i < _graphList.size(); i++) {
        if (_graphList[i]->getId() == id) {
            ZXGraph* rmGraph = _graphList[i];
            rmGraph->setRef(NULL);
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

void ZXGraphMgr::copy(size_t id, bool toNew) {
    if (_graphList.empty())
        cerr << "Error: ZXGraphMgr is empty now! Action \"copy\" failed!" << endl;
    else {
        size_t oriGraphID = getGraph()->getId();
        ZXGraph* copiedGraph = getGraph()->copy();
        copiedGraph->setId(id);

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
void ZXGraphMgr::printZXGraphMgr() const {
    cout << "-> #Graph: " << _graphList.size() << endl;
    if (!_graphList.empty()) cout << "-> Now focus on: " << getGraph()->getId() << endl;
}

void ZXGraphMgr::printGListItr() const {
    if (!_graphList.empty())
        cout << "Now focus on: " << getGraph()->getId() << endl;
    else
        cerr << "Error: ZXGraphMgr is empty now!" << endl;
}

void ZXGraphMgr::printGraphListSize() const {
    cout << "#Graph: " << _graphList.size() << endl;
}
