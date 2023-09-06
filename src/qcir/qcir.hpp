/****************************************************************************
  FileName     [ qcir.hpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "qcir/qcirGate.hpp"
#include "qcir/qcirQubit.hpp"

class QCir;
class Phase;

struct BitInfo;

class QCir {
public:
    QCir() {}
    ~QCir() {}

    QCir(QCir const& other) {
        namespace views = std::ranges::views;
        other.updateTopoOrder();
        this->addQubit(other._qubits.size());

        for (size_t i = 0; i < _qubits.size(); i++) {
            _qubits[i]->setId(other._qubits[i]->getId());
        }

        for (auto& gate : other._topoOrder) {
            auto bit_range = gate->getQubits() |
                             views::transform([](BitInfo const& qb) { return qb._qubit; });
            auto newGate = this->addGate(
                gate->getTypeStr(), {bit_range.begin(), bit_range.end()},
                gate->getPhase(), true);

            newGate->setId(gate->getId());
        }

        this->setNextGateId(1 + std::ranges::max(
                                    other._topoOrder | views::transform(
                                                           [](QCirGate* g) { return g->getId(); })));
        this->setNextQubitId(1 + std::ranges::max(
                                     other._qubits | views::transform(
                                                         [](QCirQubit* qb) { return qb->getId(); })));
        this->setFileName(other._fileName);
        this->addProcedures(other._procedures);
    }

    QCir(QCir&& other) noexcept = default;

    QCir& operator=(QCir copy) {
        copy.swap(*this);
        return *this;
    }

    void swap(QCir& other) noexcept {
        std::swap(_gateId, other._gateId);
        std::swap(_qubitId, other._qubitId);
        std::swap(_dirty, other._dirty);
        std::swap(_globalDFScounter, other._globalDFScounter);
        std::swap(_fileName, other._fileName);
        std::swap(_procedures, other._procedures);
        std::swap(_qgates, other._qgates);
        std::swap(_qubits, other._qubits);
        std::swap(_topoOrder, other._topoOrder);
    }

    friend void swap(QCir& a, QCir& b) noexcept {
        a.swap(b);
    }

    // Access functions
    size_t getNQubit() const { return _qubits.size(); }
    int getDepth();
    const std::vector<QCirQubit*>& getQubits() const { return _qubits; }
    const std::vector<QCirGate*>& getTopoOrderedGates() const { return _topoOrder; }
    const std::vector<QCirGate*>& getGates() const { return _qgates; }
    QCirGate* getGate(size_t gid) const;
    QCirQubit* getQubit(size_t qid) const;
    std::string getFileName() const { return _fileName; }
    const std::vector<std::string>& getProcedures() const { return _procedures; }

    void setFileName(std::string f) { _fileName = f; }
    void addProcedures(std::vector<std::string> const& ps) { _procedures.insert(_procedures.end(), ps.begin(), ps.end()); }
    void addProcedure(std::string const& p) { _procedures.emplace_back(p); }
    // For Copy
    void setNextGateId(size_t id) { _gateId = id; }
    void setNextQubitId(size_t id) { _qubitId = id; }
    //
    void reset();
    QCir* compose(QCir const& target);
    QCir* tensorProduct(QCir const& target);
    // Member functions about circuit construction
    QCirQubit* addSingleQubit();
    QCirQubit* insertSingleQubit(size_t);
    void addQubit(size_t num);
    bool removeQubit(size_t q);
    QCirGate* addGate(std::string gateType, std::vector<size_t> bits, Phase, bool);
    QCirGate* addSingleRZ(size_t, Phase, bool);
    bool removeGate(size_t id);

    bool readQCirFile(std::string file);
    bool readQC(std::string qc_file);
    bool readQASM(std::string qasm_file);
    bool readQSIM(std::string qsim_file);
    bool readQUIPPER(std::string quipper_file);

    bool writeQASM(std::string qasm_output);

    bool draw(std::string const& drawer, std::string const& outputPath = "", float scale = 1.0f);

    std::vector<int> countGate(bool detail = false, bool print = true);

    void updateGateTime() const;
    void printZXTopoOrder();

    // DFS functions
    template <typename F>
    void topoTraverse(F lambda) const {
        if (_dirty) {
            updateTopoOrder();
            _dirty = false;
        }
        for_each(_topoOrder.begin(), _topoOrder.end(), lambda);
    }

    bool printTopoOrder();

    // pass a function F (public functions) into for_each
    // lambdaFn such as mappingToZX / updateGateTime
    const std::vector<QCirGate*>& updateTopoOrder() const;

    // Member functions about circuit reporting
    void printDepth();
    void printGates();
    void printCircuit();
    bool printGateInfo(size_t, bool);
    void printSummary();
    void printQubits();
    void printCirInfo();

private:
    void DFS(QCirGate*) const;

    size_t _gateId = 0;
    size_t _qubitId = 0;
    bool mutable _dirty = true;
    unsigned mutable _globalDFScounter = 0;
    std::string _fileName;
    std::vector<std::string> _procedures;

    std::vector<QCirGate*> _qgates;
    std::vector<QCirQubit*> _qubits;
    std::vector<QCirGate*> mutable _topoOrder;
};
