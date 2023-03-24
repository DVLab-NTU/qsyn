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

class Device;
class Topology;
class PhyQubit;
class Operation;
struct Info;

using AdjacencyPair = std::pair<PhyQubit*, PhyQubit*>;
using Adjacencies = ordered_hashset<size_t>;
using PhyQubitList = ordered_hashmap<size_t, PhyQubit>;
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

class PhyQubit {
public:
    PhyQubit() {}
    PhyQubit(const size_t id);
    PhyQubit(const PhyQubit& other);
    PhyQubit(PhyQubit&& other);
    PhyQubit& operator=(const PhyQubit& other);
    PhyQubit& operator=(PhyQubit&& other);

    ~PhyQubit() {}
    void setId(size_t id) { _id = id; }
    void setOccupiedTime(size_t t) { _occuTime = t; }
    void setLogicalQubit(size_t id) { _logicalQubit = id; }
    void addAdjacency(size_t adj) { _adjacencies.emplace(adj); }

    size_t getId() const { return _id; }
    size_t getOccupiedTime() const { return _occuTime; }
    const bool isAdjacency(const PhyQubit& pq) const { return _adjacencies.contains(pq.getId()); }
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
    size_t _occuTime;

    bool _marked;
    size_t _pred;
    size_t _cost;
    size_t _swapTime;
    bool _source;  // false:0, true:1
    bool _taken;
};

class Device {
public:
    Device(size_t id) : _id(id) {
        _maxDist = 100000;
        _topology = std::make_shared<Topology>(id);
    }
    ~Device() {}

    size_t getId() const { return _id; }
    std::string getName() const { return _topology->getName(); }
    size_t getNQubit() const { return _nQubit; }
    // REVIEW - Not sure why this function cannot be const funct.
    const PhyQubitList& getPhyQubitList() { return _qubitList; }
    PhyQubit& getPhysicalQubit(size_t id) { return _qubitList[id]; }
    std::tuple<size_t, size_t> nextSwapCost(size_t source, size_t target);

    void setId(size_t id) { _id = id; }
    void setNQubit(size_t n) { _nQubit = n; }
    void addPhyQubit(PhyQubit q) { _qubitList[q.getId()] = q; }
    void addAdjacency(size_t a, size_t b);

    bool qubitIdExist(size_t id) { return _qubitList.contains(id); }
    bool readTopo(const std::string&);
    void calculatePath();
    // NOTE - All Pairs Shortest Path
    void FloydWarshall();
    std::vector<PhyQubit> getPath(size_t, size_t) const;

    // NOTE - Duostra
    void applyGate(const Operation& op);
    std::vector<size_t> mapping() const;
    void place(std::vector<size_t>& assign);

    void printQubits(std::vector<size_t> cand = {}) const;
    void printEdges(std::vector<size_t> cand = {}) const;
    void printTopo() const;
    void printPredecessor() const;
    void printDistance() const;
    void printPath(size_t, size_t) const;
    void printMapping() const;

private:
    size_t _id;
    size_t _nQubit;
    std::shared_ptr<Topology> _topology;
    PhyQubitList _qubitList;

    // NOTE - Internal functions/objects only used in reader
    bool parseInfo(std::ifstream& f, std::vector<std::vector<float>>&, std::vector<std::vector<float>>&, std::vector<float>&, std::vector<float>&);
    bool parseGateSet(std::string);
    bool parseSingles(std::string, std::vector<float>&);
    bool parsePairsFloat(std::string, std::vector<std::vector<float>>&);
    bool parsePairsSizeT(std::string, std::vector<std::vector<size_t>>&);

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

    Operation(GateType oper, Phase ph,
              std::tuple<size_t, size_t> qs,
              std::tuple<size_t, size_t> du)
        : oper_(oper), _phase(ph), qubits_(qs), duration_(du) {
        // sort qs
        size_t a = std::get<0>(qs);
        size_t b = std::get<1>(qs);
        assert(a != b);
        if (a > b) {
            qubits_ = std::make_tuple(b, a);
        }
    }
    Operation(const Operation& other)
        : oper_(other.oper_),
          _phase(other._phase),
          qubits_(other.qubits_),
          duration_(other.duration_) {}

    Operation& operator=(const Operation& other) {
        oper_ = other.oper_;
        _phase = other._phase;
        qubits_ = other.qubits_;
        duration_ = other.duration_;
        return *this;
    }

    void setPhase(Phase ph) { _phase = ph; }
    Phase getPhase() const { return _phase; }
    size_t get_cost() const { return std::get<1>(duration_); }
    size_t get_op_time() const { return std::get<0>(duration_); }
    std::tuple<size_t, size_t> get_duration() const { return duration_; }
    GateType get_operator() const { return oper_; }
    // std::string get_operator_name() const { return operator_get_name(oper_); }
    std::tuple<size_t, size_t> get_qubits() const { return qubits_; }

private:
    GateType oper_;
    Phase _phase;
    std::tuple<size_t, size_t> qubits_;
    std::tuple<size_t, size_t> duration_;  // <from, to>
};

#endif  // TOPOLOGY_H