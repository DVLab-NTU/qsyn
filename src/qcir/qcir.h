/****************************************************************************
  FileName     [ qcir.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_H
#define QCIR_H

#include <cstddef>  // for size_t
#include <string>   // for string
#include <unordered_map>

#include "phase.h"  // for Phase
#include "qcirGate.h"
#include "qcirQubit.h"

class QCir;
class ZXGraph;

struct BitInfo;

template <typename T>
class QTensor;

extern QCir* qCir;

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
    const std::vector<QCirQubit*>& getQubits() const { return _qubits; }
    const std::vector<QCirGate*>& getTopoOrderdGates() const { return _topoOrder; }
    const std::vector<QCirGate*>& getGates() const { return _qgates; }
    QCirGate* getGate(size_t gid) const;
    QCirQubit* getQubit(size_t qid) const;
    std::string getFileName() const { return _fileName; }
    const std::vector<std::string>& getProcedures() const { return _procedures; }

    void incrementZXId() { _ZXNodeId++; }
    void setId(size_t id) { _id = id; }
    void setFileName(std::string f) { _fileName = f; }
    void addProcedure(std::string = "", const std::vector<std::string>& = {});
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
    QCirGate* addGate(std::string, std::vector<size_t>, Phase, bool);
    QCirGate* addSingleRZ(size_t, Phase, bool);
    bool removeGate(size_t id);

    bool readQCirFile(std::string file);
    bool readQC(std::string qc_file);
    bool readQASM(std::string qasm_file);
    bool readQSIM(std::string qsim_file);
    bool readQUIPPER(std::string quipper_file);

    bool writeQASM(std::string qasm_output);

    bool draw(std::string const& drawer, std::string const& outputPath = "", float scale = 1.0f);

    void countGate(bool = false);

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
    const std::vector<QCirGate*>& updateTopoOrder();

    // Member functions about circuit reporting
    void printDepth();
    void printGates();
    void printCircuit();
    bool printGateInfo(size_t, bool);
    void printSummary();
    void printQubits();

private:
    void DFS(QCirGate*);
    void updateTensorPin(std::vector<BitInfo>, QTensor<double>);

    size_t _id;
    size_t _gateId;
    size_t _ZXNodeId;
    size_t _qubitId;
    bool _dirty;
    unsigned _globalDFScounter;
    QTensor<double>* _tensor;
    std::string _fileName;
    std::vector<std::string> _procedures;

    std::vector<QCirGate*> _qgates;
    std::vector<QCirQubit*> _qubits;
    std::vector<QCirGate*> _topoOrder;
    std::vector<ZXGraph*> _ZXGraphList;
    std::unordered_map<size_t, std::pair<size_t, size_t>> _qubit2pin;
};

#endif  // QCIR_MGR_H