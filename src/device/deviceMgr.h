/****************************************************************************
  FileName     [ deviceMgr.h ]
  PackageName  [ device ]
  Synopsis     [ Define class Device manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef DEVICE_MGR_H
#define DEVICE_MGR_H

#include <cstddef>  // for size_t
#include <optional>
#include <vector>

#include "device.h"
class DeviceMgr;

extern DeviceMgr* deviceMgr;

//------------------------------------------------------------------------
//  Define types
//------------------------------------------------------------------------
using DeviceList = std::vector<Device>;

//------------------------------------------------------------------------
//  Define classes
//------------------------------------------------------------------------
class DeviceMgr {
public:
    DeviceMgr() {
        _topoList.clear();
        _topoListItr = _topoList.begin();
        _nextID = 0;
    }
    ~DeviceMgr() {}
    void reset();

    // Test
    bool isID(size_t id) const;

    // Setter and Getter
    size_t getNextID() const { return _nextID; }
    const Device& getDevice() const { return _topoList[_topoListItr - _topoList.begin()]; }
    const DeviceList& getDeviceList() const { return _topoList; }
    DeviceList::iterator getDTListItr() const { return _topoListItr; }

    void setNextID(size_t id) { _nextID = id; }
    void setDevice(Device& dt) {
        dt.setId(_topoListItr - _topoList.begin());
        _topoList[_topoListItr - _topoList.begin()] = dt;
    }

    // Add and Remove
    const Device& addDevice(size_t id);
    void removeDevice(size_t id);

    // Action
    void checkout2Device(size_t id);
    void copy(size_t id, bool toNew = true);
    Device* findDeviceByID(size_t id);

    // Print
    void printDeviceMgr() const;
    void printDeviceListItr() const;
    void printDeviceList() const;
    void printDeviceListSize() const;

private:
    size_t _nextID;
    DeviceList _topoList;
    DeviceList::iterator _topoListItr;
};

#endif  // DEVICE_MGR_H
