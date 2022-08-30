/****************************************************************************
  FileName     [ zxGraphMgr.h ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX-graph manager ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_GRAPH_MGR_H
#define ZX_GRAPH_MGR_H

#include <iostream>
#include <vector>
#include "zxGraph.h"
#include "zxDef.h"

extern ZXGraphMgr *zxGraphMgr;
using namespace std;

class ZXGraphMgr{
    public:
        ZXGraphMgr(){
            _graphList.clear();
            _gListItr = _graphList.begin();
            _nextID = 0;
        }
        ~ZXGraphMgr(){}
        void reset();

        // Test
        bool isID(size_t id) const;

        // Setter and Getter
        size_t getNextID() const { return _nextID; }
        void setNextID(size_t id) { _nextID = id; }

        // Add and Remove
        void addZXGraph(size_t id);
        void removeZXGraph(size_t id);

    private:
        size_t                          _nextID;
        vector<ZXGraph* >               _graphList;
        vector<ZXGraph* >::iterator     _gListItr;

};




#endif // ZX_GRAPH_MGR_H

