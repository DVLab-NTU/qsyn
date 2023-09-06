/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Router structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <queue>
#include <string>

#include "device/device.hpp"

class Gate;

class AStarNode {
public:
    friend class AStarComp;
    AStarNode(size_t, size_t, bool);
    AStarNode(AStarNode const&);

    AStarNode& operator=(AStarNode const&);

    bool get_source() const { return _source; }
    size_t get_id() const { return _id; }
    size_t get_cost() const { return _estimated_cost; }

private:
    size_t _estimated_cost;
    size_t _id;
    bool _source;  // false q0 propagate, true q1 propagate
};

class AStarComp {
public:
    bool operator()(AStarNode const& a, AStarNode const& b) {
        return a._estimated_cost > b._estimated_cost;
    }
};

class Router {
public:
    using PriorityQueue = std::priority_queue<AStarNode, std::vector<AStarNode>, AStarComp>;
    Router(Device&&, std::string const&, bool) noexcept;
    Router(Router const&) noexcept;
    Router(Router&&) noexcept;

    std::unique_ptr<Router> clone() const;

    Device& get_device() { return _device; }
    Device const& get_device() const { return _device; }

    size_t get_gate_cost(Gate const&, bool, size_t);
    bool is_executable(Gate const&);

    // Main Router function
    Operation execute_single(GateType, Phase, size_t);
    std::vector<Operation> duostra_routing(GateType, size_t, Phase, std::tuple<size_t, size_t>, bool, bool);
    std::vector<Operation> apsp_routing(GateType, size_t, Phase, std::tuple<size_t, size_t>, bool, bool);
    std::vector<Operation> assign_gate(Gate const&);

private:
    bool _greedy_type;
    bool _duostra;
    bool _orient;
    bool _apsp;
    Device _device;
    std::vector<size_t> _logical_to_physical;

    void _initialize(std::string const&);
    std::tuple<size_t, size_t> _get_physical_qubits(Gate const& gate) const;

    std::tuple<bool, size_t> _touch_adjacency(PhysicalQubit& qubit, PriorityQueue& pq, bool source);  // return <if touch target, target id>, swtch: false q0 propagate, true q1 propagate
    std::vector<Operation> _traceback([[maybe_unused]] GateType gt, size_t, Phase ph, PhysicalQubit& q0, PhysicalQubit& q1, PhysicalQubit& t0, PhysicalQubit& t1, bool swap_ids, bool swapped);
};
