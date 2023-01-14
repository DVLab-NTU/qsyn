/****************************************************************************
  FileName     [ qcir.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define quantum circuit manager ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_H
#define QCIR_H

#include <fstream>
#include <functional>
#include <iostream>
#include <stack>
#include <string>
#include <vector>

#include "phase.h"
#include "qcirDef.h"
#include "qcirGate.h"
#include "qcirQubit.h"
#include "qtensor.h"
#include "zxGraph.h"

extern QCir* qCir;
using namespace std;

class QCir {
public:
    QCir(size_t id) : _id(id), _gateId(0), _ZXNodeId(0), _qubitId(0), _tensor(nullptr) {
        _dirty = true;
        _globalDFScounter = 0;
    }
    ~QCir() {}

    // Access functions
    size_t getId() const { return _id; }
    size_t getZXId() const { return _ZXNodeId; }
    size_t getNQubit() const { return _qubits.size(); }
    const vector<QCirQubit*>& getQubits() const { return _qubits; }
    const vector<QCirGate*>& getTopoOrderdGates() const { return _topoOrder; }
    QCirGate* getGate(size_t gid) const;
    QCirQubit* getQubit(size_t qid) const;
    void incrementZXId() { _ZXNodeId++; }
    void setId(size_t id) { _id = id; }
    // For Copy
    void setNextGateId(size_t id) { _gateId = id; }
    void setNextQubitId(size_t id) { _qubitId = id; }
    //
    void reset();
    QCir* copy();
    QCir* compose(QCir* target);
    QCir* tensorProduct(QCir* target);
    // Member functions about circuit construction
    QCirQubit* addSingleQubit();
    QCirQubit* insertSingleQubit(size_t);
    void addQubit(size_t num);
    bool removeQubit(size_t q);
    QCirGate* addGate(string, vector<size_t>, Phase, bool);
    QCirGate* addSingleRZ(size_t, Phase, bool);
    bool removeGate(size_t id);

    bool readQCirFile(string file);
    bool readQC(string qc_file);
    bool readQASM(string qasm_file);
    bool readQSIM(string qsim_file);
    bool readQUIPPER(string quipper_file);

    bool writeQASM(string qasm_output);

    void analysis(bool = false);
    void ZXMapping();
    void tensorMapping();

    void clearMapping();
    void updateGateTime();
    void printZXTopoOrder();

    // DFS functions
    template <typename F>
    void topoTraverse(F lambda) {
        if (_dirty) {
            updateTopoOrder();
            _dirty = false;
        }
        for_each(_topoOrder.begin(), _topoOrder.end(), lambda);
    }

    bool printTopoOrder();

    // pass a function F (public functions) into for_each
    // lambdaFn such as mappingToZX / updateGateTime
    void updateTopoOrder();

    // Member functions about circuit reporting
    void printGates();
    bool printGateInfo(size_t, bool);
    void printSummary();
    void printQubits();

private:
    void DFS(QCirGate*);
    void updateTensorPin(vector<BitInfo>, QTensor<double>);

    size_t _id;
    size_t _gateId;
    size_t _ZXNodeId;
    size_t _qubitId;
    bool _dirty;
    unsigned _globalDFScounter;
    QTensor<double>* _tensor;

    vector<QCirGate*> _qgates;
    vector<QCirQubit*> _qubits;
    vector<QCirGate*> _topoOrder;
    vector<ZXGraph*> _ZXGraphList;
    unordered_map<size_t, pair<size_t, size_t>> _qubit2pin;
};

#endif  // QCIR_MGR_H