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
    Gate(size_t id, GateType type, Phase ph,std::tuple<size_t, size_t> qs)
        : id_(id), type_(type), _phase(ph), qubits_(qs), prevs_({}), nexts_({}) {
        if (std::get<0>(qubits_) > std::get<1>(qubits_)) {
            qubits_ = std::make_tuple(std::get<1>(qubits_), std::get<0>(qubits_));
        }
    }

    Gate(const Gate& other) = delete;

    Gate(Gate&& other)
        : id_(other.id_),
          type_(other.type_),
          _phase(other._phase),
          qubits_(other.qubits_),
          prevs_(other.prevs_),
          nexts_(other.nexts_) {}

    size_t get_id() const { return id_; }
    std::tuple<size_t, size_t> get_qubits() const { return qubits_; }

    void set_type(GateType t) { type_ = t; }
    void setPhase(Phase p) { _phase = p; }
    void add_prev(size_t p) {
        if (p != ERROR_CODE) {
            prevs_.push_back(p);
        }
    }

    void add_next(size_t n) {
        if (n != ERROR_CODE) {
            nexts_.push_back(n);
        }
    }

    bool is_avail(const std::unordered_map<size_t, size_t>& executed_gates) const {
        return all_of(prevs_.begin(), prevs_.end(), [&](size_t prev) -> bool {
            return executed_gates.find(prev) != executed_gates.end();
        });
    }

    bool is_first() const { return prevs_.empty(); }
    bool is_last() const { return nexts_.empty(); }

    const std::vector<size_t>& get_prevs() const { return prevs_; }
    const std::vector<size_t>& get_nexts() const { return nexts_; }
    GateType get_type() const { return type_; }
    Phase getPhase() const { return _phase; }

private:
    size_t id_;
    GateType type_;
    Phase _phase; //For saving phase information
    std::tuple<size_t, size_t> qubits_;
    std::vector<size_t> prevs_;
    std::vector<size_t> nexts_;
};

class DependencyGraph {
public:
    DependencyGraph(size_t n, std::vector<Gate>&& gates) : num_qubits_(n), gates_(move(gates)) {}
    DependencyGraph(const DependencyGraph& other) = delete;
    DependencyGraph(DependencyGraph&& other)
        : num_qubits_(other.num_qubits_), gates_(move(other.gates_)) {}

    const std::vector<Gate>& gates() const { return gates_; }
    const Gate& get_gate(size_t idx) const { return gates_[idx]; }
    size_t get_num_qubits() const { return num_qubits_; }

private:
    size_t num_qubits_;
    std::vector<Gate> gates_;
};

class CircuitTopo {
public:
    CircuitTopo(std::shared_ptr<DependencyGraph> dep) : dep_graph_(dep), avail_gates_({}), executed_gates_({}) {
        for (size_t i = 0; i < dep_graph_->gates().size(); i++) {
            if (dep_graph_->get_gate(i).is_avail(executed_gates_)) {
                avail_gates_.push_back(i);
            }
        }
    }

    CircuitTopo(const CircuitTopo& other)
        : dep_graph_(other.dep_graph_),
          avail_gates_(other.avail_gates_),
          executed_gates_(other.executed_gates_) {}

    CircuitTopo(CircuitTopo&& other)
        : dep_graph_(std::move(other.dep_graph_)),
          avail_gates_(std::move(other.avail_gates_)),
          executed_gates_(std::move(other.executed_gates_)) {}

    ~CircuitTopo() {}

    std::unique_ptr<CircuitTopo> clone() const {
        return std::make_unique<CircuitTopo>(*this);
    }

    void update_avail_gates(size_t executed);
    size_t get_num_qubits() const { return dep_graph_->get_num_qubits(); }
    size_t get_num_gates() const { return dep_graph_->gates().size(); }
    const Gate& get_gate(size_t i) const { return dep_graph_->get_gate(i); }
    const std::vector<size_t>& get_avail_gates() const { return avail_gates_; }
    void print_gates_with_next();
    void print_gates_with_prev();
    // NOTE - If onion is implemented
    // template <bool first>
    // std::vector<size_t> get_gates() const;

    // std::vector<size_t> get_first_gates() const { return get_gates<true>(); }
    // std::vector<size_t> get_last_gates() const { return get_gates<false>(); }

    // std::unordered_map<size_t, std::vector<size_t>> gate_by_dist_to_first() const;
    // std::unordered_map<size_t, std::vector<size_t>> gate_by_dist_to_last() const;

protected:
    // data member
    std::shared_ptr<const DependencyGraph> dep_graph_;
    std::vector<size_t> avail_gates_;

    // Executed gates is a countable set.
    std::unordered_map<size_t, size_t> executed_gates_;

    // private function

    // NOTE - If onion is implemented
    // static std::unordered_map<size_t, std::vector<size_t>> gate_by_generation(
    //     const std::unordered_map<size_t, size_t>& map);
    // template <bool first>
    // std::unordered_map<size_t, size_t> dist_to() const;
    // std::unordered_map<size_t, size_t> dist_to_first() const {
    //     return dist_to<true>();
    // }
    // std::unordered_map<size_t, size_t> dist_to_last() const {
    //     return dist_to<false>();
    // }
};

#endif