/****************************************************************************
  FileName     [ optimizer.h ]
  PackageName  [ optimizer ]
  Synopsis     [ Define class Optimizer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#include <cstddef>  // for size_t
#include <set>

#include "qcir.h"   // for QCir

class QCir;

class Optimizer {
public:
    Optimizer(QCir* c = nullptr) {
        _circuit = c;
    }
    ~Optimizer() {}

    QCir* parseCircuit();
    QCir* parseForward();
    void parseGate();

    void addHadamard();
    void addCZ();
    void addCX();
    void addGate();

    void topologicalSort();

private:
    QCir* _circuit;
    std::vector<QCirGate*> _gates;
};

#endif