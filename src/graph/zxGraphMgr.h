/****************************************************************************
  FileName     [ zxGraphMgr.h ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX-graph manager ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_GRAPH_MGR_H
#define ZX_GRAPH_MGR_H

#include <cstddef>
#include <vector>

class ZXGraph;
class ZXGraphMgr;

extern ZXGraphMgr* zxGraphMgr;

//------------------------------------------------------------------------
//  Define types
//------------------------------------------------------------------------
using ZXGraphList = std::vector<ZXGraph*>;

//------------------------------------------------------------------------
//  Define classes
//------------------------------------------------------------------------
class ZXGraphMgr {
public:
    ZXGraphMgr() {
        _graphList.clear();
        _gListItr = _graphList.begin();
        _nextID = 0;
    }
    ~ZXGraphMgr() {}
    void reset();

    // Test
    bool isID(size_t id) const;

    // Setter and Getter
    size_t getNextID() const { return _nextID; }
    ZXGraph* getGraph() const { return _graphList[_gListItr - _graphList.begin()]; }
    const ZXGraphList& getGraphList() const { return _graphList; }
    ZXGraphList::iterator getgListItr() const { return _gListItr; }

    void setNextID(size_t id) { _nextID = id; }
    void setGraph(ZXGraph* g);

    // Add and Remove
    ZXGraph* addZXGraph(size_t id);
    void removeZXGraph(size_t id);

    // Action
    void checkout2ZXGraph(size_t id);
    void copy(size_t id, bool toNew = true);
    ZXGraph* findZXGraphByID(size_t id) const;

    // Print
    void printZXGraphMgr() const;
    void printGList() const;
    void printGListItr() const;
    void printGraphListSize() const;

private:
    size_t _nextID;
    ZXGraphList _graphList;
    ZXGraphList::iterator _gListItr;
};

#endif  // ZX_GRAPH_MGR_H
