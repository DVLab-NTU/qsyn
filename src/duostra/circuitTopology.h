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

class CircuitTopo {
public:
    CircuitTopo(QCir* circuit) : dep_graph_(circuit), avail_gates_({}), executed_gates_({}) {
        initial_avail_gates();
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
    void initial_avail_gates();
    void update_avail_gates(size_t executed);
    size_t get_num_qubits() const { return dep_graph_->getNQubit(); }
    size_t get_num_gates() const { return dep_graph_->getTopoOrderdGates().size(); }
    QCirGate* get_gate(size_t i) const { return dep_graph_->getGate(i); }
    const std::vector<size_t>& get_avail_gates() const { return avail_gates_; }

    // NOTE - If onion is implemented
    // template <bool first>
    // std::vector<size_t> get_gates() const;

    // std::vector<size_t> get_first_gates() const { return get_gates<true>(); }
    // std::vector<size_t> get_last_gates() const { return get_gates<false>(); }

    // std::unordered_map<size_t, std::vector<size_t>> gate_by_dist_to_first() const;
    // std::unordered_map<size_t, std::vector<size_t>> gate_by_dist_to_last() const;

protected:
    // data member
    QCir* dep_graph_;
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