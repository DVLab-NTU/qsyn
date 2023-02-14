/****************************************************************************
  FileName     [ optimizer.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define class Optimizer member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "optimizer.h"

#include <assert.h>  // for assert

using namespace std;

extern size_t verbose;

// FIXME - All functions can be modified, i.e. you may need to pass some parameters or change return type into some functions

/**
 * @brief
 *
 * @return QCir*
 */
QCir* Optimizer::parseCircuit() {
    // FIXME - Delete after the function is finished
    cout << "Parse Circuit" << endl;
    return nullptr;
}

/**
 * @brief
 *
 * @return QCir*
 */
QCir* Optimizer::parseForward() {
    return nullptr;
}

/**
 * @brief
 *
 */
void Optimizer::parseGate() {
}

/**
 * @brief Add a Hadamard gate to the output. Called by `parseGate`
 *
 * @param target Index of the target qubit
 */
void Optimizer::addHadamard(size_t target) {
    QCirGate* had = new HGate(_gateCnt);
    had->addQubit(target, true);
    _gateCnt++;
    _gates[target].emplace_back(had);
    remove(_hadamards.begin(), _hadamards.end(), target);
    _available[target].clear();
    _availty[target] = 1;
}

/**
 * @brief
 *
 */
void Optimizer::addCZ() {
}

/**
 * @brief
 *
 */
void Optimizer::addCX() {
}

/**
 * @brief Add a single qubit rotate gate to the output.
 *
 * @param target Index of the target qubit
 * @param type 0: Z-axis, 1: X-axis, 2: Y-axis
 */
void Optimizer::addGate(size_t target, size_t type) {
    QCirGate* rotate = nullptr;
    if (type == 0) {
        rotate = new PGate(_gateCnt);
    } else if (type == 1) {
        rotate = new PXGate(_gateCnt);
    } else if (type == 2) {
        rotate = new PYGate(_gateCnt);
    } else {
        cerr << "Error: wrong type!! Type shoud be 0, 1, or 2";
        return;
    }
    rotate->addQubit(target, true);
    _gates[target].emplace_back(rotate);
    _available[target].emplace_back(target);
    _gateCnt++;
}

/**
 * @brief
 *
 */
void Optimizer::topologicalSort() {
}