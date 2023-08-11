/****************************************************************************
  FileName     [ qcirQubit.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirQubit structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

class QCirGate;

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

class QCirQubit {
public:
    QCirQubit(size_t id) : _id(id) {
        _bitFirst = nullptr;
        _bitLast = nullptr;
    }
    ~QCirQubit() {}

    // Basic access method
    void setId(size_t id) { _id = id; }
    void setLast(QCirGate* l) { _bitLast = l; }
    void setFirst(QCirGate* f) { _bitFirst = f; }
    size_t getId() const { return _id; }
    QCirGate* getLast() const { return _bitLast; }
    QCirGate* getFirst() const { return _bitFirst; }
    // Printing functions
    void printBitLine() const;

private:
    size_t _id;
    QCirGate* _bitLast;
    QCirGate* _bitFirst;
};

