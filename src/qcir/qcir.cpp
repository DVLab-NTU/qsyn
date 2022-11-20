/****************************************************************************
  FileName     [ qcir.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define QCir Edition functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
extern size_t verbose;

/**
 * @brief Get Gate.
 *
 * @param id
 * @return QCirGate*
 */
QCirGate *QCir::getGate(size_t id) const {
    for (size_t i = 0; i < _qgate.size(); i++) {
        if (_qgate[i]->getId() == id)
            return _qgate[i];
    }
    return NULL;
}

/**
 * @brief Get Qubit.
 *
 * @param id
 * @return QCirQubit
 */
QCirQubit *QCir::getQubit(size_t id) const {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]->getId() == id)
            return _qubits[i];
    }
    return NULL;
}

/**
 * @brief Print QCir Summary
 */
void QCir::printSummary() {
    if (_dirty)
        updateGateTime();
    cout << "Listed by gate ID" << endl;
    for (size_t i = 0; i < _qgate.size(); i++) {
        _qgate[i]->printGate();
    }
}

/**
 * @brief Print Qubits
 */
void QCir::printQubits() {
    if (_dirty)
        updateGateTime();

    for (size_t i = 0; i < _qubits.size(); i++)
        _qubits[i]->printBitLine();
}

/**
 * @brief Print Gate information
 *
 * @param id
 * @param showTime
 */
bool QCir::printGateInfo(size_t id, bool showTime) {
    if (getGate(id) != NULL) {
        if (showTime && _dirty)
            updateGateTime();
        getGate(id)->printGateInfo(showTime);
        return true;
    } else {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    }
}

/**
 * @brief Add Qubit.
 *
 * @param num
 */
void QCir::addQubit(size_t num) {
    for (size_t i = 0; i < num; i++) {
        QCirQubit *temp = new QCirQubit(_qubitId);
        _qubits.push_back(temp);
        _qubitId++;
        clearMapping();
    }
}

/**
 * @brief Remove Qubit with specific id
 *
 * @param id
 * @return true if succcessfully removed
 * @return false if not found or the qubit is not empty
 */
bool QCir::removeQubit(size_t id) {
    // Delete the ancilla only if whole line is empty
    QCirQubit *target = getQubit(id);
    if (target == NULL) {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    } else {
        if (target->getLast() != NULL || target->getFirst() != NULL) {
            cerr << "Error: id " << id << " is not an empty qubit!!" << endl;
            return false;
        } else {
            _qubits.erase(remove(_qubits.begin(), _qubits.end(), target), _qubits.end());
            clearMapping();
            return true;
        }
    }
}

/**
 * @brief Add Gate
 *
 * @param type
 * @param bits
 * @param phase
 * @param append
 */
void QCir::addGate(string type, vector<size_t> bits, Phase phase, bool append) {
    QCirGate *temp = NULL;
    for_each(type.begin(), type.end(), [](char &c) { c = ::tolower(c); });
    if (type == "h")
        temp = new HGate(_gateId);
    else if (type == "z")
        temp = new ZGate(_gateId);
    else if (type == "s")
        temp = new SGate(_gateId);
    else if (type == "s*" || type == "sdg" || type == "sd")
        temp = new SDGGate(_gateId);
    else if (type == "t")
        temp = new TGate(_gateId);
    else if (type == "tdg" || type == "td" || type == "t*")
        temp = new TDGGate(_gateId);
    else if (type == "p")
        temp = new RZGate(_gateId);
    else if (type == "cz")
        temp = new CZGate(_gateId);
    else if (type == "x" || type == "not")
        temp = new XGate(_gateId);
    else if (type == "y")
        temp = new YGate(_gateId);
    else if (type == "sx" || type == "x_1_2")
        temp = new SXGate(_gateId);
    else if (type == "sy" || type == "y_1_2")
        temp = new SYGate(_gateId);
    else if (type == "cx" || type == "cnot")
        temp = new CXGate(_gateId);
    else if (type == "ccx" || type == "ccnot")
        temp = new CCXGate(_gateId);
    else if (type == "ccz")
        temp = new CCZGate(_gateId);
    // Note: rz and p has a little difference
    else if (type == "rz") {
        temp = new RZGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "rx") {
        temp = new RXGate(_gateId);
        temp->setRotatePhase(phase);
    } else {
        cerr << "Error: The gate " << type << " is not implemented!!" << endl;
        abort();
        return;
    }
    if (append) {
        size_t max_time = 0;
        for (size_t k = 0; k < bits.size(); k++) {
            size_t q = bits[k];
            temp->addQubit(q, k == bits.size() - 1);  // target is the last one
            QCirQubit *target = getQubit(q);
            if (target->getLast() != NULL) {
                temp->setParent(q, target->getLast());
                target->getLast()->setChild(q, temp);
                if ((target->getLast()->getTime() + 1) > max_time)
                    max_time = target->getLast()->getTime() + 1;
            } else
                target->setFirst(temp);
            target->setLast(temp);
        }
        temp->setTime(max_time);
    } else {
        for (size_t k = 0; k < bits.size(); k++) {
            size_t q = bits[k];
            temp->addQubit(q, k == bits.size() - 1);  // target is the last one
            QCirQubit *target = getQubit(q);
            if (target->getFirst() != NULL) {
                temp->setChild(q, target->getFirst());
                target->getFirst()->setParent(q, temp);
            } else
                target->setLast(temp);
            target->setFirst(temp);
        }
        _dirty = true;
    }
    _qgate.push_back(temp);
    _gateId++;
    clearMapping();
}

bool QCir::removeGate(size_t id) {
    QCirGate *target = getGate(id);
    if (target == NULL) {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    } else {
        vector<BitInfo> Info = target->getQubits();
        for (size_t i = 0; i < Info.size(); i++) {
            if (Info[i]._parent != NULL)
                Info[i]._parent->setChild(Info[i]._qubit, Info[i]._child);
            else
                getQubit(Info[i]._qubit)->setFirst(Info[i]._child);
            if (Info[i]._child != NULL)
                Info[i]._child->setParent(Info[i]._qubit, Info[i]._parent);
            else
                getQubit(Info[i]._qubit)->setLast(Info[i]._parent);
            Info[i]._parent = NULL;
            Info[i]._child = NULL;
        }
        _qgate.erase(remove(_qgate.begin(), _qgate.end(), target), _qgate.end());
        _dirty = true;
        clearMapping();
        return true;
    }
}
