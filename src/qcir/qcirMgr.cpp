/****************************************************************************
  FileName     [ qcirMgr.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define QCir manager ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcirMgr.h"

#include <cstddef>  // for size_t
#include <iostream>

using namespace std;

QCirMgr* qcirMgr = 0;
extern size_t verbose;

/*****************************************/
/*   class QCirMgr member functions   */
/*****************************************/

/**
 * @brief reset QCirMgr
 *
 */
void QCirMgr::reset() {
    for (auto& qcir : _circuitList) delete qcir;
    _circuitList.clear();
    _cListItr = _circuitList.begin();
    _nextID = 0;
}

// Test

/**
 * @brief Check if `id` is an existed ID in QCirMgr
 *
 * @param id
 * @return true
 * @return false
 */
bool QCirMgr::isID(size_t id) const {
    for (size_t i = 0; i < _circuitList.size(); i++) {
        if (_circuitList[i]->getId() == id) return true;
    }
    return false;
}

// Add and Remove

/**
 * @brief Add a quantum circuit to
 *
 * @param id
 * @return QCir*
 */
QCir* QCirMgr::addQCir(size_t id) {
    QCir* qcir = new QCir(id);
    _circuitList.push_back(qcir);
    _cListItr = _circuitList.end() - 1;
    if (id == _nextID || _nextID < id) _nextID = id + 1;
    if (verbose >= 3) {
        cout << "Create and checkout to QCir " << id << endl;
    }
    return qcir;
}

void QCirMgr::removeQCir(size_t id) {
    for (size_t i = 0; i < _circuitList.size(); i++) {
        if (_circuitList[i]->getId() == id) {
            QCir* rmCircuit = _circuitList[i];
            _circuitList.erase(_circuitList.begin() + i);
            delete rmCircuit;
            if (verbose >= 3) cout << "Successfully removed QCir " << id << endl;
            _cListItr = _circuitList.begin();
            if (verbose >= 3) {
                if (!_circuitList.empty())
                    cout << "Checkout to QCir " << _circuitList[0]->getId() << endl;
                else
                    cout << "Note: The QCir list is empty now" << endl;
            }
            return;
        }
    }
    cerr << "Error: The id provided does not exist!!" << endl;
    return;
}

// Action
void QCirMgr::checkout2QCir(size_t id) {
    for (size_t i = 0; i < _circuitList.size(); i++) {
        if (_circuitList[i]->getId() == id) {
            _cListItr = _circuitList.begin() + i;
            if (verbose >= 3) cout << "Checkout to QCir " << id << endl;
            return;
        }
    }
    cerr << "Error: The id provided does not exist!!" << endl;
    return;
}

void QCirMgr::copy(size_t id, bool toNew) {
    if (_circuitList.empty())
        cerr << "Error: QCirMgr is empty now! Action \"copy\" failed!" << endl;
    else {
        size_t oriCircuitID = getQCircuit()->getId();
        QCir* copiedCircuit = getQCircuit()->copy();
        copiedCircuit->setId(id);

        if (toNew) {
            _circuitList.push_back(copiedCircuit);
            _cListItr = _circuitList.end() - 1;
            if (_nextID <= id) _nextID = id + 1;
            if (verbose >= 3) {
                cout << "Successfully copied QCir " << oriCircuitID << " to QCir " << id << "\n";
                cout << "Checkout to QCir " << id << "\n";
            }
        } else {
            for (size_t i = 0; i < _circuitList.size(); i++) {
                if (_circuitList[i]->getId() == id) {
                    _circuitList[i] = copiedCircuit;
                    if (verbose >= 3) cout << "Successfully copied QCir " << oriCircuitID << " to QCir " << id << endl;
                    checkout2QCir(id);
                    break;
                }
            }
        }
    }
}

QCir* QCirMgr::findQCirByID(size_t id) const {
    if (!isID(id))
        cerr << "Error: QCir " << id << " does not exist!" << endl;
    else {
        for (size_t i = 0; i < _circuitList.size(); i++) {
            if (_circuitList[i]->getId() == id) return _circuitList[i];
        }
    }
    return nullptr;
}

// Print
void QCirMgr::printQCirMgr() const {
    cout << "-> #QCir: " << _circuitList.size() << endl;
    if (!_circuitList.empty()) cout << "-> Now focus on: " << getQCircuit()->getId() << endl;
}

void QCirMgr::printCListItr() const {
    if (!_circuitList.empty())
        cout << "Now focus on: " << getQCircuit()->getId() << endl;
    else
        cerr << "Error: QCirMgr is empty now!" << endl;
}

void QCirMgr::printQCircuitListSize() const {
    cout << "#QCir: " << _circuitList.size() << endl;
}
