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

#include "circuitTopology.h"
#include "device.h"
#include "qcir.h"
#include "variables.h"

class AStarNode {
public:
    friend class AStarComp;
    AStarNode(size_t, size_t, bool);
    AStarNode(const AStarNode&);

    AStarNode& operator=(const AStarNode&);

    bool getSource() const { return _source; }
    size_t getId() const { return _id; }
    size_t getCost() const { return _estimatedCost; }

private:
    size_t _estimatedCost;
    size_t _id;
    bool _source;  // false q0 propagate, true q1 propagate
};

class AStarComp {
public:
    bool operator()(const AStarNode& a, const AStarNode& b) {
        return a._estimatedCost > b._estimatedCost;
    }
};

class Router {
public:
    using PriorityQueue = std::priority_queue<AStarNode, std::vector<AStarNode>, AStarComp>;
    Router(Device&&, const std::string&, bool) noexcept;
    Router(const Router&) noexcept;
    Router(Router&&) noexcept;

    std::unique_ptr<Router> clone() const;

    Device& getDevice() { return _device; }
    const Device& getDevice() const { return _device; }

    size_t getGateCost(const Gate&, bool, size_t);
    bool isExecutable(const Gate&);

    // Main Router function
    Operation executeSingle(GateType, Phase, size_t);
    std::vector<Operation> duostraRouting(GateType, size_t, Phase, std::tuple<size_t, size_t>, bool, bool);
    std::vector<Operation> apspRouting(GateType, size_t, Phase, std::tuple<size_t, size_t>, bool, bool);
    std::vector<Operation> assignGate(const Gate&);

private:
    bool _greedyType;
    bool _duostra;
    bool _orient;
    bool _APSP;
    Device _device;
    std::vector<size_t> _logical2Physical;

    void init(const std::string&);
    std::tuple<size_t, size_t> getPhysicalQubits(const Gate& gate) const;

    std::tuple<bool, size_t> touchAdjacency(PhysicalQubit& qubit, PriorityQueue& pq, bool swtch);  // return <if touch target, target id>, swtch: false q0 propagate, true q1 propagate
    std::vector<Operation> traceback([[maybe_unused]] GateType op, size_t, Phase ph, PhysicalQubit& q0, PhysicalQubit& q1, PhysicalQubit& t0, PhysicalQubit& t1, bool, bool);
};

#endif