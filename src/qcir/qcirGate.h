/****************************************************************************
  FileName     [ qcirGate.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define basic gate data structures ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_GATE_H
#define QCIR_GATE_H

#include <string>
#include <vector>
#include <iostream>

using namespace std;

// TODO: Feel free to define your own classes, variables, or functions.

class QCirGate;

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------
class QCirGate
{
public:
    // size_t id, string type, vector<size_t> pins
    QCirGate(size_t id, string type, vector<size_t> pins) : _id(id), _type(type), _pins(pins) {}
    ~QCirGate() {}

    // Basic access method
    string getTypeStr() const { return _type; }
    size_t getId() const { return _id; }
    // Printing functions
    void printGate() const;
    void reportGate() const;
    void reportFanin(int level) const;
    void reportFanout(int level) const;

private:
    vector<size_t> _pins;
    string _type;
    size_t _id;

protected:
};

#endif // QCIR_GATE_H