/****************************************************************************
  FileName     [ topologyMgr.h ]
  PackageName  [ topology ]
  Synopsis     [ Define class DeviceTopo manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef TOPOLOGY_MGR_H
#define TOPOLOGY_MGR_H

#include <cstddef>  // for size_t
#include <vector>

#include "topology.h"
class DeviceTopoMgr;

extern DeviceTopoMgr* deviceTopoMgr;

//------------------------------------------------------------------------
//  Define types
//------------------------------------------------------------------------
using DeviceTopoList = std::vector<DeviceTopo*>;

//------------------------------------------------------------------------
//  Define classes
//------------------------------------------------------------------------
class DeviceTopoMgr {
public:
    DeviceTopoMgr() {
        _topoList.clear();
        _topoListItr = _topoList.begin();
        _nextID = 0;
    }
    ~DeviceTopoMgr() {}
    void reset();

    // Test
    bool isID(size_t id) const;

    // Setter and Getter
    size_t getNextID() const { return _nextID; }
    DeviceTopo* getDeviceTopo() const { return _topoList[_topoListItr - _topoList.begin()]; }
    const DeviceTopoList& getDeviceTopoList() const { return _topoList; }
    DeviceTopoList::iterator getDTListItr() const { return _topoListItr; }

    void setNextID(size_t id) { _nextID = id; }
    void setDeviceTopo(DeviceTopo* dt) {
        delete _topoList[_topoListItr - _topoList.begin()];
        _topoList[_topoListItr - _topoList.begin()] = dt;
        dt->setId(_topoListItr - _topoList.begin());
    }

    // Add and Remove
    DeviceTopo* addDeviceTopo(size_t id);
    void removeDeviceTopo(size_t id);

    // Action
    void checkout2DeviceTopo(size_t id);
    void copy(size_t id, bool toNew = true);
    DeviceTopo* findDeviceTopoByID(size_t id) const;

    // Print
    void printDeviceTopoMgr() const;
    void printTopoListItr() const;
    void printDeviceTopoListSize() const;

private:
    size_t _nextID;
    DeviceTopoList _topoList;
    DeviceTopoList::iterator _topoListItr;
};

#endif  // TOPOLOGY_MGR_H
