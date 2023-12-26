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
#include "qsyn/qsyn_type.hpp"
#include "util/phase.hpp"

namespace qsyn::duostra {

class Gate {
public:
    Gate(size_t id, qcir::GateRotationCategory type, dvlab::Phase ph, std::tuple<size_t, size_t> qs);
    ~Gate()                                = default;
    Gate(Gate const& other)                = delete;
    Gate& operator=(Gate const& other)     = delete;
    Gate(Gate&& other) noexcept            = default;
    Gate& operator=(Gate&& other) noexcept = default;

    size_t get_id() const { return _id; }
    std::tuple<size_t, size_t> get_qubits() const { return _qubits; }
    std::vector<size_t> const& get_prevs() const { return _prevs; }
    std::vector<size_t> const& get_nexts() const { return _nexts; }
    qcir::GateRotationCategory get_type() const { return _type; }
    dvlab::Phase get_phase() const { return _phase; }

    void set_id(size_t id) { _id = id; }
    void set_type(qcir::GateRotationCategory t) { _type = t; }
    void set_phase(dvlab::Phase p) { _phase = p; }
    void add_prev(size_t prev_gate_id);
    void add_next(size_t next_gate_id);
    void set_prevs(std::unordered_map<size_t, size_t> const&);
    void set_nexts(std::unordered_map<size_t, size_t> const&);

    bool is_available(std::unordered_map<size_t, size_t> const&) const;
    bool is_swapped() const { return _swap; }
    bool is_first_gate() const { return _prevs.empty(); }
    bool is_last_gate() const { return _nexts.empty(); }

    bool is_swap() const { return _type == qcir::GateRotationCategory::swap; }
    bool is_cx() const { return _type == qcir::GateRotationCategory::px && _phase == dvlab::Phase(1) && std::get<1>(_qubits) != max_qubit_id; }
    bool is_cz() const { return _type == qcir::GateRotationCategory::pz && _phase == dvlab::Phase(1) && std::get<1>(_qubits) != max_qubit_id; }

private:
    size_t _id;
    qcir::GateRotationCategory _type;
    dvlab::Phase _phase;  // For saving phase information
    bool _swap = false;   // qubits is swapped for duostra
    std::tuple<QubitIdType, QubitIdType> _qubits;
    std::vector<size_t> _prevs = {};
    std::vector<size_t> _nexts = {};
};

class DependencyGraph {
public:
    DependencyGraph(size_t n, std::vector<Gate> gates) : _num_qubits(n), _gates(std::move(gates)) {}
    ~DependencyGraph()                                       = default;
    DependencyGraph(DependencyGraph const& other)            = delete;
    DependencyGraph& operator=(DependencyGraph const& other) = delete;
    DependencyGraph(DependencyGraph&& other)                 = default;
    DependencyGraph& operator=(DependencyGraph&& other)      = default;

    std::vector<Gate> const& get_gates() const { return _gates; }
    Gate const& get_gate(size_t idx) const {
        assert(idx < _gates.size());
        return _gates[idx];
    }
    size_t get_num_qubits() const { return _num_qubits; }

private:
    size_t _num_qubits;
    std::vector<Gate> _gates = {};
};

class CircuitTopology {
public:
    CircuitTopology(std::shared_ptr<DependencyGraph> const&);

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

    std::unordered_map<size_t, size_t> _executed_gates;  //  maps gate index to number of gates executed next
};

}  // namespace qsyn::duostra
