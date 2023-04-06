/****************************************************************************
  FileName     [ qcirMgr.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir manager member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
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

/**
 * @brief Remove QCir
 *
 * @param id
 */
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
/**
 * @brief Checkout to QCir
 *
 * @param id the id to be checkout
 */
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

/**
 * @brief Copy the QCir
 *
 * @param id the id to be copied
 * @param toNew if true, checkout to new circuit
 */
void QCirMgr::copy(size_t id, bool toNew) {
    if (_circuitList.empty())
        cerr << "Error: QCirMgr is empty now! Action \"copy\" failed!" << endl;
    else {
        size_t oriCircuitID = getQCircuit()->getId();
        QCir* copiedCircuit = getQCircuit()->copy();
        copiedCircuit->setId(id);
        copiedCircuit->setFileName(getQCircuit()->getFileName());
        copiedCircuit->addProcedure("", getQCircuit()->getProcedures());
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

/**
 * @brief Find QCir by id
 *
 * @param id
 * @return QCir*
 */
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
/**
 * @brief Print number of circuits and the focused id
 *
 */
void QCirMgr::printQCirMgr() const {
    cout << "-> #QCir: " << _circuitList.size() << endl;
    if (!_circuitList.empty()) cout << "-> Now focus on: " << getQCircuit()->getId() << endl;
}

/**
 * @brief Print list of circuits
 *
 */
void QCirMgr::printCList() const {
    if (!_circuitList.empty()) {
        for (auto& cir : _circuitList) {
            if (cir->getId() == getQCircuit()->getId())
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
        cerr << "Error: QCirMgr is empty now!" << endl;
}

/**
 * @brief Print the id of focused circuit
 *
 */
void QCirMgr::printCListItr() const {
    if (!_circuitList.empty())
        cout << "Now focus on: " << getQCircuit()->getId() << endl;
    else
        cerr << "Error: QCirMgr is empty now!" << endl;
}

/**
 * @brief Print the number of circuits
 *
 */
void QCirMgr::printQCircuitListSize() const {
    cout << "#QCir: " << _circuitList.size() << endl;
}
