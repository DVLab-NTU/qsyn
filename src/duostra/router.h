/****************************************************************************
  FileName     [ router.h ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Router structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ROUTER_H
#define ROUTER_H

#include <queue>
#include <string>
// #include <tuple>
#include "circuitTopology.h"
#include "qcir.h"
#include "topology.h"

class AStarNode {
public:
    friend class AStarComp;
    AStarNode(size_t cost, size_t id, bool swtch)
        : est_cost_(cost), id_(id), swtch_(swtch) {}
    AStarNode(const AStarNode& other)
        : est_cost_(other.est_cost_), id_(other.id_), swtch_(other.swtch_) {}

    AStarNode& operator=(const AStarNode& other) {
        est_cost_ = other.est_cost_;
        id_ = other.id_;
        swtch_ = other.swtch_;
        return *this;
    }

    bool get_swtch() const { return swtch_; }
    size_t get_id() const { return id_; }
    size_t get_cost() const { return est_cost_; }

private:
    size_t est_cost_;
    size_t id_;
    bool swtch_;  // false q0 propagate, true q1 propagate
};

class AStarComp {
public:
    bool operator()(const AStarNode& a, const AStarNode& b) {
        return a.est_cost_ > b.est_cost_;
    }
};

class Router {
public:
    using PriorityQueue = std::priority_queue<AStarNode, std::vector<AStarNode>, AStarComp>;
    Router(Device&& device,
           const std::string& typ,
           const std::string& cost,
           bool orient) noexcept;
    Router(const Router& other) noexcept;
    Router(Router&& other) noexcept;

    Device& get_device() { return device_; }
    const Device& get_device() const { return device_; }

    size_t get_gate_cost(const Gate&, bool min_max, size_t apsp_coef);
    bool is_executable(const Gate& gate);
    std::unique_ptr<Router> clone() const;

    // Main Router function
    Operation execute_single(GateType gate, size_t q);
    std::vector<Operation> duostra_routing(GateType gate, std::tuple<size_t, size_t> qs, bool orient);
    // TODO - APSP_ROUTING: After First version is finished
    std::vector<Operation> apsp_routing(GateType gate, std::tuple<size_t, size_t> qs, bool orient);
    std::vector<Operation> assign_gate(const Gate& gate);

private:
    bool greedy_type_;
    bool duostra_;
    bool orient_;
    bool apsp_;
    Device device_;
    std::vector<size_t> topo_to_dev_;

    void init(const std::string& typ, const std::string& cost);
    std::tuple<size_t, size_t> get_device_qubits_idx(const Gate& gate) const;

    std::tuple<bool, size_t> touch_adj(PhyQubit& qubit, PriorityQueue& pq, bool swtch);  // return <if touch target, target id>, swtch: false q0 propagate, true q1 propagate
    std::vector<Operation> traceback([[maybe_unused]] GateType op, PhyQubit& q0, PhyQubit& q1, PhyQubit& t0, PhyQubit& t1);
};

#endif