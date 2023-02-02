/****************************************************************************
  FileName     [ topology.h ]
  PackageName  [ topology ]
  Synopsis     [ Define class DeviceTopo structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include <cstddef>  // for size_t
#include <string>   // for string
#include <unordered_map>

#include "ordered_hashmap.h"
#include "ordered_hashset.h"
#include "qcirGate.h"
#include "util.h"

class DeviceTopo;

class PhyQubit;
struct AdjInfo;

using AdjacencyPair = std::pair<PhyQubit*, PhyQubit*>;
using Adjacencies = ordered_hashset<PhyQubit*>;
using AdjacenciesInfo = std::unordered_map<AdjacencyPair, AdjInfo>;

namespace std {
template <>
struct hash<AdjacencyPair> {
    size_t operator()(const AdjacencyPair& k) const {
        return (
            (hash<PhyQubit*>()(k.first) ^
             (hash<PhyQubit*>()(k.second) << 1)) >>
            1);
    }
};
}  // namespace std

struct AdjInfo {
    float _cnotTime;
    float _error;
};

class PhyQubit {
public:
    PhyQubit(size_t id) : _id(id) {
    }
    ~PhyQubit() {}

    size_t getId() const { return _id; }
    float getError() const { return _error; }
    float getDelay() const { return _gateDelay; }
    const Adjacencies& getAdjacencies() const { return _adjacencies; }

    void setId(size_t id) { _id = id; }
    void setError(float er) { _error = er; }
    void setDelay(float dl) { _gateDelay = dl; }
    void addAdjacency(PhyQubit* adj) { _adjacencies.emplace(adj); }

private:
    // NOTE - Device information
    size_t _id;
    float _error;
    float _gateDelay;
    Adjacencies _adjacencies;
    // NOTE - Duostra parameter
    float _occuTime;
};

class DeviceTopo {
public:
    DeviceTopo(size_t id) : _id(id) {
    }
    ~DeviceTopo() {}

    size_t getId() const { return _id; }
    size_t getNQubit() const { return _nQubit; }
    std::string getName() const { return _name; }
    const std::vector<GateType>& getGateSet() const { return _gateSet; }
    const AdjacenciesInfo& getAdjInfo() const { return _adjInfo; }

    void setId(size_t id) { _id = id; }
    void setNQubit(size_t n) { _nQubit = n; }
    void setName(std::string n) { _name = n; }
    void addGateType(GateType gt) { _gateSet.emplace_back(gt); }
    void addAdjacencyInfo(AdjacencyPair adjp);

    bool readTopo();

private:
    size_t _id;
    std::string _name;
    size_t _nQubit;
    std::vector<GateType> _gateSet;
    AdjacenciesInfo _adjInfo;
};

#endif  // TOPOLOGY_H