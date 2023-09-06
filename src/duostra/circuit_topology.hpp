/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cassert>
#include <climits>
#include <cmath>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "qcir/qcir_gate.hpp"
#include "util/phase.hpp"

class Gate {
public:
    Gate(size_t id, GateType type, Phase ph, std::tuple<size_t, size_t> qs);
    Gate(Gate const& other) = delete;
    Gate(Gate&& other);

    size_t get_id() const { return _id; }
    std::tuple<size_t, size_t> get_qubits() const { return _qubits; }
    std::vector<size_t> const& get_prevs() const { return _prevs; }
    std::vector<size_t> const& get_nexts() const { return _nexts; }
    GateType get_type() const { return _type; }
    Phase get_phase() const { return _phase; }

    void set_type(GateType t) { _type = t; }
    void set_phase(Phase p) { _phase = p; }
    void add_prev(size_t);
    void add_next(size_t);

    bool is_available(std::unordered_map<size_t, size_t> const&) const;
    bool is_swapped() const { return _swap; }
    bool is_first_gate() const { return _prevs.empty(); }
    bool is_last_gate() const { return _nexts.empty(); }

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
    DependencyGraph(size_t n, std::vector<Gate>&& gates) : _num_qubits(n), _gates(std::move(gates)) {}
    DependencyGraph(DependencyGraph const& other) = delete;
    DependencyGraph(DependencyGraph&& other)
        : _num_qubits(other._num_qubits), _gates(std::move(other._gates)) {}

    std::vector<Gate> const& get_gates() const { return _gates; }
    Gate const& get_gate(size_t idx) const { return _gates[idx]; }
    size_t get_num_qubits() const { return _num_qubits; }

private:
    size_t _num_qubits;
    std::vector<Gate> _gates;
};

class CircuitTopology {
public:
    CircuitTopology(std::shared_ptr<DependencyGraph>);
    CircuitTopology(CircuitTopology const& other);
    CircuitTopology(CircuitTopology&& other);
    ~CircuitTopology() {}

    std::unique_ptr<CircuitTopology> clone() const;

    void update_available_gates(size_t executed);
    size_t get_num_qubits() const { return _dependency_graph->get_num_qubits(); }
    size_t get_num_gates() const { return _dependency_graph->get_gates().size(); }
    Gate const& get_gate(size_t i) const { return _dependency_graph->get_gate(i); }
    std::vector<size_t> const& get_available_gates() const { return _available_gates; }
    void print_gates_with_nexts();
    void print_gates_with_prevs();

protected:
    std::shared_ptr<DependencyGraph const> _dependency_graph;
    std::vector<size_t> _available_gates;

    // NOTE - Executed gates is a countable set. <Gate Index, #next executed>
    // REVIEW - what does this mean? everything is a countable set in programming
    std::unordered_map<size_t, size_t> _executed_gates;
};