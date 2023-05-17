/****************************************************************************
  FileName     [ device.h ]
  PackageName  [ device ]
  Synopsis     [ Define class Device, Topology, and Operation structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef DEVICE_H
#define DEVICE_H

#include <cstddef>  // for size_t
#include <string>   // for string
#include <unordered_map>

#include "ordered_hashmap.h"
#include "ordered_hashset.h"
#include "qcirGate.h"
#include "util.h"

class Device;
class Topology;
class PhysicalQubit;
class Operation;
struct Info;

using Adjacencies = ordered_hashset<size_t>;
using PhyQubitList = ordered_hashmap<size_t, PhysicalQubit>;
using AdjacenciesInfo = std::unordered_map<std::pair<size_t, size_t>, Info>;
using QubitInfo = std::unordered_map<size_t, Info>;

namespace std {
template <>
struct hash<std::pair<size_t, size_t>> {
    size_t operator()(const std::pair<size_t, size_t>& k) const {
        return (
            (hash<size_t>()(k.first) ^
             (hash<size_t>()(k.second) << 1)) >>
            1);
    }
};
}  // namespace std

struct Info {
    float _time;
    float _error;
};

std::ostream& operator<<(std::ostream& os, const Info& info);

class Topology {
public:
    Topology(size_t id) : _id(id) {
    }
    ~Topology() {}

    size_t getId() const { return _id; }

    std::string getName() const { return _name; }
    const std::vector<GateType>& getGateSet() const { return _gateSet; }
    const Info& getAdjPairInfo(size_t, size_t);
    const Info& getQubitInfo(size_t);
    size_t getAdjSize() const { return _adjInfo.size(); }
    void setId(size_t id) { _id = id; }
    void setNQubit(size_t n) { _nQubit = n; }
    void setName(std::string n) { _name = n; }
    void addGateType(GateType gt) { _gateSet.emplace_back(gt); }
    void addAdjacencyInfo(size_t a, size_t b, Info info);
    void addQubitInfo(size_t a, Info info);

    void printSingleEdge(size_t a, size_t b) const;

private:
    size_t _id;
    std::string _name;
    size_t _nQubit;
    std::vector<GateType> _gateSet;
    QubitInfo _qubitInfo;
    AdjacenciesInfo _adjInfo;
};

class PhysicalQubit {
public:
    PhysicalQubit() {}
    PhysicalQubit(const size_t id);
    PhysicalQubit(const PhysicalQubit& other);
    PhysicalQubit(PhysicalQubit&& other);
    PhysicalQubit& operator=(const PhysicalQubit& other);
    PhysicalQubit& operator=(PhysicalQubit&& other);

    ~PhysicalQubit() {}
    void setId(size_t id) { _id = id; }
    void setOccupiedTime(size_t t) { _occupiedTime = t; }
    void setLogicalQubit(size_t id) { _logicalQubit = id; }
    void addAdjacency(size_t adj) { _adjacencies.emplace(adj); }

    size_t getId() const { return _id; }
    size_t getOccupiedTime() const { return _occupiedTime; }
    const bool isAdjacency(const PhysicalQubit& pq) const { return _adjacencies.contains(pq.getId()); }
    const Adjacencies& getAdjacencies() const { return _adjacencies; }
    size_t getLogicalQubit() const { return _logicalQubit; }

    size_t getCost() const { return _cost; }
    bool isMarked() { return _marked; }
    bool isTaken() { return _taken; }
    bool getSource() const { return _source; }
    size_t getPred() const { return _pred; }
    size_t getSwapTime() const { return _swapTime; }

    // NOTE - Duostra functions
    void mark(bool, size_t);
    void takeRoute(size_t, size_t);
    void reset();

private:
    // NOTE - Device information
    size_t _id;
    Adjacencies _adjacencies;

    // NOTE - Duostra parameter
    size_t _logicalQubit;
    size_t _occupiedTime;

    bool _marked;
    size_t _pred;
    size_t _cost;
    size_t _swapTime;
    bool _source;  // false:0, true:1
    bool _taken;
};

class Device {
public:
    Device(size_t);
    ~Device() {}

    size_t getId() const { return _id; }
    std::string getName() const { return _topology->getName(); }
    size_t getNQubit() const { return _nQubit; }
    const PhyQubitList& getPhyQubitList() const { return _qubitList; }
    PhysicalQubit& getPhysicalQubit(size_t id) { return _qubitList[id]; }
    size_t getPhysicalbyLogical(size_t id);
    std::tuple<size_t, size_t> getNextSwapCost(size_t source, size_t target);
    bool qubitIdExist(size_t id) { return _qubitList.contains(id); }

    void setId(size_t id) { _id = id; }
    void setNQubit(size_t n) { _nQubit = n; }
    void addPhyQubit(PhysicalQubit q) { _qubitList[q.getId()] = q; }
    void addAdjacency(size_t a, size_t b);

    // NOTE - Duostra
    void applyGate(const Operation&);
    void applySingleQubitGate(size_t);
    void applySwapCheck(size_t, size_t);
    std::vector<size_t> mapping() const;
    void place(const std::vector<size_t>&);

    // NOTE - All Pairs Shortest Path
    void calculatePath();
    void FloydWarshall();
    std::vector<PhysicalQubit> getPath(size_t, size_t) const;

    bool readDevice(const std::string&);

    void printQubits(std::vector<size_t> = {}) const;
    void printEdges(std::vector<size_t> = {}) const;
    void printTopology() const;
    void printPredecessor() const;
    void printDistance() const;
    void printPath(size_t, size_t) const;
    void printMapping();
    void printStatus() const;

private:
    size_t _id;
    size_t _nQubit;
    std::shared_ptr<Topology> _topology;
    PhyQubitList _qubitList;

    // NOTE - Internal functions only used in reader
    bool parseGateSet(std::string);
    bool parseSingles(std::string, std::vector<float>&);
    bool parsePairsFloat(std::string, std::vector<std::vector<float>>&);
    bool parsePairsSizeT(std::string, std::vector<std::vector<size_t>>&);
    bool parseInfo(std::ifstream& f, std::vector<std::vector<float>>&, std::vector<std::vector<float>>&, std::vector<float>&, std::vector<float>&);

    // NOTE - Containers and helper functions for Floyd-Warshall
    int _maxDist;
    std::vector<std::vector<size_t>> _predecessor;
    std::vector<std::vector<int>> _distance;
    std::vector<std::vector<int>> _adjMatrix;
    void initFloydWarshall();
    void setWeight(size_t = 0);
};

class Operation {
public:
    friend std::ostream& operator<<(std::ostream&, Operation&);
    friend std::ostream& operator<<(std::ostream&, const Operation&);

    Operation(GateType, Phase, std::tuple<size_t, size_t>, std::tuple<size_t, size_t>);
    Operation(const Operation&);

    Operation& operator=(const Operation&);

    GateType getType() const { return _oper; }
    Phase getPhase() const { return _phase; }
    size_t getCost() const { return std::get<1>(_duration); }
    size_t getOperationTime() const { return std::get<0>(_duration); }
    std::tuple<size_t, size_t> getDuration() const { return _duration; }
    std::tuple<size_t, size_t> getQubits() const { return _qubits; }

    size_t getId() const { return _id; }
    void setId(size_t id) { _id = id; }

private:
    GateType _oper;
    Phase _phase;
    std::tuple<size_t, size_t> _qubits;
    std::tuple<size_t, size_t> _duration;  // <from, to>
    size_t _id;
};

std::ostream& operator<<(std::ostream&, const Operation&);

#endif  // DEVICE_H