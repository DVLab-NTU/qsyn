/****************************************************************************
  FileName     [ qcirQubit.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirQubit structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_QUBIT_H
#define QCIR_QUBIT_H

#include <cstddef>  // for size_t, NULL

class QCirGate;

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

class QCirQubit {
public:
    QCirQubit(size_t id) : _id(id) {
        _bitFirst = NULL;
        _bitLast = NULL;
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

protected:
};

#endif  // QCIR_QUBIT_H