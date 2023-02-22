/****************************************************************************
  FileName     [ topologyMgr.cpp ]
  PackageName  [ topology ]
  Synopsis     [ Define class DeviceTopo manager member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "topologyMgr.h"

#include <cstddef>  // for size_t
#include <iostream>

using namespace std;

DeviceTopoMgr* deviceTopoMgr = 0;
extern size_t verbose;

/**
 * @brief Reset DeviceTopoMgr
 *
 */
void DeviceTopoMgr::reset() {
    for (auto& topo : _topoList) delete topo;
    _topoList.clear();
    _topoListItr = _topoList.begin();
    _nextID = 0;
}

/**
 * @brief Check if `id` is an existed ID in DeviceTopoMgr
 *
 * @param id
 * @return true
 * @return false
 */
bool DeviceTopoMgr::isID(size_t id) const {
    for (size_t i = 0; i < _topoList.size(); i++) {
        if (_topoList[i]->getId() == id) return true;
    }
    return false;
}

/**
 * @brief Add a device topology to DeviceTopoMgr
 *
 * @param id
 * @return DeviceTopo*
 */
DeviceTopo* DeviceTopoMgr::addDeviceTopo(size_t id) {
    DeviceTopo* qcir = new DeviceTopo(id);
    _topoList.push_back(qcir);
    _topoListItr = _topoList.end() - 1;
    if (id == _nextID || _nextID < id) _nextID = id + 1;
    if (verbose >= 3) {
        cout << "Create and checkout to DeviceTopo " << id << endl;
    }
    return qcir;
}

/**
 * @brief Remove DeviceTopo
 *
 * @param id
 */
void DeviceTopoMgr::removeDeviceTopo(size_t id) {
    for (size_t i = 0; i < _topoList.size(); i++) {
        if (_topoList[i]->getId() == id) {
            DeviceTopo* rmCircuit = _topoList[i];
            _topoList.erase(_topoList.begin() + i);
            delete rmCircuit;
            if (verbose >= 3) cout << "Successfully removed DeviceTopo " << id << endl;
            _topoListItr = _topoList.begin();
            if (verbose >= 3) {
                if (!_topoList.empty())
                    cout << "Checkout to DeviceTopo " << _topoList[0]->getId() << endl;
                else
                    cout << "Note: The DeviceTopo list is empty now" << endl;
            }
            return;
        }
    }
    cerr << "Error: The id provided does not exist!!" << endl;
    return;
}

// Action
/**
 * @brief Checkout to DeviceTopo
 *
 * @param id the id to be checkout
 */
void DeviceTopoMgr::checkout2DeviceTopo(size_t id) {
    for (size_t i = 0; i < _topoList.size(); i++) {
        if (_topoList[i]->getId() == id) {
            _topoListItr = _topoList.begin() + i;
            if (verbose >= 3) cout << "Checkout to DeviceTopo " << id << endl;
            return;
        }
    }
    cerr << "Error: The id provided does not exist!!" << endl;
    return;
}

/**
 * @brief Find DeviceTopo by id
 *
 * @param id
 * @return DeviceTopo*
 */
DeviceTopo* DeviceTopoMgr::findDeviceTopoByID(size_t id) const {
    if (!isID(id))
        cerr << "Error: DeviceTopo " << id << " does not exist!" << endl;
    else {
        for (size_t i = 0; i < _topoList.size(); i++) {
            if (_topoList[i]->getId() == id) return _topoList[i];
        }
    }
    return nullptr;
}

// Print
/**
 * @brief Print number of topologies and the focused id
 *
 */
void DeviceTopoMgr::printDeviceTopoMgr() const {
    cout << "-> #DeviceTopo: " << _topoList.size() << endl;
    if (!_topoList.empty()) cout << "-> Now focus on: " << getDeviceTopo()->getId() << " (" << getDeviceTopo()->getName() << ")" << endl;
}

/**
 * @brief Print the id of focused topology
 *
 */
void DeviceTopoMgr::printTopoListItr() const {
    if (!_topoList.empty())
        cout << "Now focus on: " << getDeviceTopo()->getId() << " (" << getDeviceTopo()->getName() << ")" << endl;
    else
        cerr << "Error: DeviceTopoMgr is empty now!" << endl;
}

/**
 * @brief Print the list of topology
 *
 */
void DeviceTopoMgr::printTopoList() const {
    if (!_topoList.empty()) {
        for (auto& tpg : _topoList) {
            if (tpg->getId() == getDeviceTopo()->getId())
                cout << "★ ";
            else
                cout << "  ";
            cout << tpg->getId() << " " << left << setw(20) << tpg->getName().substr(0, 20) << " #Q: " << right << setw(4) << tpg->getNQubit() << endl;
        }
    }
}

/**
 * @brief Print the number of circuits
 *
 */
void DeviceTopoMgr::printDeviceTopoListSize() const {
    cout << "#DeviceTopo: " << _topoList.size() << endl;
}
