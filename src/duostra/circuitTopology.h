/****************************************************************************
  FileName     [ circuitTopology.h ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef CIRCUIT_TOPOLOGY_H
#define CIRCUIT_TOPOLOGY_H

#include <cassert>
#include <climits>
#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "qcir.h"

class Gate {
public:
    Gate(size_t id, GateType type, Phase ph, std::tuple<size_t, size_t> qs);
    Gate(const Gate& other) = delete;
    Gate(Gate&& other);

    size_t getId() const { return _id; }
    std::tuple<size_t, size_t> getQubits() const { return _qubits; }

    void setType(GateType t) { _type = t; }
    void setPhase(Phase p) { _phase = p; }
    void addPrev(size_t);
    void addNext(size_t);

    bool isAvailable(const std::unordered_map<size_t, size_t>&) const;
    bool isSwapped() const { return _swap; }
    bool isFirstGate() const { return _prevs.empty(); }
    bool isLastGate() const { return _nexts.empty(); }

    const std::vector<size_t>& getPrevs() const { return _prevs; }
    const std::vector<size_t>& getNexts() const { return _nexts; }
    GateType getType() const { return _type; }
    Phase getPhase() const { return _phase; }

private:
    size_t _id;
    GateType _type;
    Phase _phase;  // For saving phase information
    bool _swap;    // qubits is swapped for duostra
    std::tuple<size_t, size_t> _qubits;
    std::vector<size_t> _prevs;
    std::vector<size_t> _nexts;
};

class DependencyGraph {
public:
    DependencyGraph(size_t n, std::vector<Gate>&& gates) : _nQubits(n), _gates(move(gates)) {}
    DependencyGraph(const DependencyGraph& other) = delete;
    DependencyGraph(DependencyGraph&& other)
        : _nQubits(other._nQubits), _gates(move(other._gates)) {}

    const std::vector<Gate>& gates() const { return _gates; }
    const Gate& getGate(size_t idx) const { return _gates[idx]; }
    size_t getNumQubits() const { return _nQubits; }

private:
    size_t _nQubits;
    std::vector<Gate> _gates;
};

class CircuitTopo {
public:
    CircuitTopo(std::shared_ptr<DependencyGraph>);
    CircuitTopo(const CircuitTopo& other);
    CircuitTopo(CircuitTopo&& other);
    ~CircuitTopo() {}

    std::unique_ptr<CircuitTopo> clone() const;

    void updateAvailableGates(size_t executed);
    size_t getNumQubits() const { return _dependencyGraph->getNumQubits(); }
    size_t getNumGates() const { return _dependencyGraph->gates().size(); }
    const Gate& getGate(size_t i) const { return _dependencyGraph->getGate(i); }
    const std::vector<size_t>& getAvailableGates() const { return _availableGates; }
    void printGatesWithNexts();
    void printGatesWithPrevs();

protected:
    std::shared_ptr<const DependencyGraph> _dependencyGraph;
    std::vector<size_t> _availableGates;

    // NOTE - Executed gates is a countable set. <Gate Index, #next executed>
    std::unordered_map<size_t, size_t> _executedGates;
};

#endif