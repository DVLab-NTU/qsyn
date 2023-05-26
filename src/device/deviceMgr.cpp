/****************************************************************************
  FileName     [ deviceMgr.cpp ]
  PackageName  [ device ]
  Synopsis     [ Define class Device manager member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "deviceMgr.h"

#include <cstddef>  // for size_t
#include <iostream>

using namespace std;

DeviceMgr* deviceMgr = 0;
extern size_t verbose;

/**
 * @brief Reset DeviceMgr
 *
 */
void DeviceMgr::reset() {
    _topoList.clear();
    _topoListItr = _topoList.begin();
    _nextID = 0;
}

/**
 * @brief Check if `id` is an existed ID in DeviceMgr
 *
 * @param id
 * @return true
 * @return false
 */
bool DeviceMgr::isID(size_t id) const {
    for (size_t i = 0; i < _topoList.size(); i++) {
        if (_topoList[i].getId() == id) return true;
    }
    return false;
}

/**
 * @brief Add a device topology to DeviceMgr
 *
 * @param id
 * @return Device*
 */
const Device& DeviceMgr::addDevice(size_t id) {
    Device tmp = Device(id);
    _topoList.push_back(tmp);
    _topoListItr = _topoList.end() - 1;
    if (id == _nextID || _nextID < id) _nextID = id + 1;
    if (verbose >= 3) {
        cout << "Create and checkout to Device " << id << endl;
    }
    return _topoList.back();
}

/**
 * @brief Remove Device
 *
 * @param id
 */
void DeviceMgr::removeDevice(size_t id) {
    for (size_t i = 0; i < _topoList.size(); i++) {
        if (_topoList[i].getId() == id) {
            _topoList.erase(_topoList.begin() + i);
            if (verbose >= 3) cout << "Successfully removed Device " << id << endl;
            _topoListItr = _topoList.begin();
            if (verbose >= 3) {
                if (!_topoList.empty())
                    cout << "Checkout to Device " << _topoList[0].getId() << endl;
                else
                    cout << "Note: The Device list is empty now" << endl;
            }
            return;
        }
    }
    cerr << "Error: The id provided does not exist!!" << endl;
    return;
}

// Action
/**
 * @brief Checkout to Device
 *
 * @param id the id to be checkout
 */
void DeviceMgr::checkout2Device(size_t id) {
    for (size_t i = 0; i < _topoList.size(); i++) {
        if (_topoList[i].getId() == id) {
            _topoListItr = _topoList.begin() + i;
            if (verbose >= 3) cout << "Checkout to Device " << id << endl;
            return;
        }
    }
    cerr << "Error: The id provided does not exist!!" << endl;
    return;
}

/**
 * @brief Find Device by id
 *
 * @param id
 * @return Device*
 */
Device* DeviceMgr::findDeviceByID(size_t id) {
    if (!isID(id))
        cerr << "Error: Device " << id << " does not exist!" << endl;
    else {
        for (size_t i = 0; i < _topoList.size(); i++) {
            if (_topoList[i].getId() == id) return &_topoList[i];
            // if (_topoList[i].getId() == id) return &(*it)+i;
        }
    }
    return nullptr;
}

// Print
/**
 * @brief Print number of topologies and the focused id
 *
 */
void DeviceMgr::printDeviceMgr() const {
    cout << "-> #Device: " << _topoList.size() << endl;
    if (!_topoList.empty()) cout << "-> Now focus on: " << getDevice().getId() << " (" << getDevice().getName() << ")" << endl;
}

/**
 * @brief Print the id of focused topology
 *
 */
void DeviceMgr::printDeviceListItr() const {
    if (!_topoList.empty())
        cout << "Now focus on: " << getDevice().getId() << " (" << getDevice().getName() << ")" << endl;
    else
        cerr << "Error: DeviceMgr is empty now!" << endl;
}

/**
 * @brief Print the list of devices
 *
 */
void DeviceMgr::printDeviceList() const {
    if (!_topoList.empty()) {
        for (auto& tpg : _topoList) {
            if (tpg.getId() == getDevice().getId())
                cout << "★ ";
            else
                cout << "  ";
            cout << tpg.getId() << "   " << left << setw(20) << tpg.getName().substr(0, 20) << " #Q: " << right << setw(4) << tpg.getNQubit() << endl;
        }
    }
}

/**
 * @brief Print the number of circuits
 *
 */
void DeviceMgr::printDeviceListSize() const {
    cout << "#Device: " << _topoList.size() << endl;
}
