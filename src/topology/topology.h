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
class Operation;
struct AdjInfo;

using AdjacencyPair = std::pair<PhyQubit*, PhyQubit*>;
using Adjacencies = ordered_hashset<PhyQubit*>;
using PhyQubitList = ordered_hashmap<size_t, PhyQubit*>;
using AdjacenciesInfo = std::unordered_map<std::pair<size_t, size_t>, AdjInfo>;

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
    size_t getLogicalQubit() const { return _logicalQubit; }
    float getError() const { return _error; }
    float getDelay() const { return _gateDelay; }
    size_t getCost() const { return _cost; }
    bool getSource() const { return _source; }
    PhyQubit* getPred() const { return _pred; }
    const Adjacencies& getAdjacencies() const { return _adjacencies; }
    size_t getOccupiedTime() const { return _occuTime; }
    size_t getSwapTime() const { return _swapTime; }
    bool isAdjacency(PhyQubit* pq) { return _adjacencies.contains(pq); }
    bool isMarked() { return _marked; }
    bool isTaken() { return _taken; }
    void setId(size_t id) { _id = id; }
    void setOccupiedTime(size_t t) { _occuTime = t; }
    void setLogicalQubit(size_t id) { _logicalQubit = id; }
    void setError(float er) { _error = er; }
    void setDelay(float dl) { _gateDelay = dl; }
    void addAdjacency(PhyQubit* adj) { _adjacencies.emplace(adj); }

    void printInfo(bool = true) const;

    // NOTE - Duostra functions
    void mark(bool, PhyQubit*);
    void takeRoute(size_t, size_t);
    void reset();

private:
    // NOTE - Device information
    size_t _id;

    float _error;
    float _gateDelay;
    Adjacencies _adjacencies;

    // NOTE - Duostra parameter
    size_t _logicalQubit;
    size_t _occuTime;

    bool _marked;
    PhyQubit* _pred;
    size_t _cost;
    size_t _swapTime;
    bool _source;  // false:0, true:1
    bool _taken;
};

class DeviceTopo {
public:
    DeviceTopo(size_t id) : _id(id) {
        _maxDist = 100000;
    }
    ~DeviceTopo() {}

    size_t getId() const { return _id; }
    size_t getNQubit() const { return _nQubit; }
    std::string getName() const { return _name; }
    const std::vector<GateType>& getGateSet() const { return _gateSet; }
    // REVIEW - Not sure why this function cannot be const funct.
    const PhyQubitList& getPhyQubitList() { return _qubitList; }
    PhyQubit* getPhysicalQubit(size_t id) { return _qubitList[id]; }
    const AdjacenciesInfo& getAdjInfo() const { return _adjInfo; }
    const AdjInfo& getAdjPairInfo(size_t, size_t);
    PhyQubit* getPhysicalByLogical(size_t id) { return _log2Phy[id]; }

    void setId(size_t id) { _id = id; }
    void setNQubit(size_t n) { _nQubit = n; }
    void setName(std::string n) { _name = n; }
    void addGateType(GateType gt) { _gateSet.emplace_back(gt); }
    void addPhyQubit(PhyQubit* q) { _qubitList[q->getId()] = q; }
    void addAdjacency(size_t a, size_t b);
    void addAdjacencyInfo(size_t a, size_t b, AdjInfo info);

    bool qubitIdExist(size_t id) { return _qubitList.contains(id); }
    bool readTopo(const std::string&);
    void calculatePath();
    // NOTE - All Pairs Shortest Path
    void FloydWarshall();
    std::vector<PhyQubit*> getPath(PhyQubit*, PhyQubit*);
    std::vector<PhyQubit*> getPath(size_t, size_t);

    // NOTE - Duostra
    void applyGate(const Operation& op);
    std::vector<size_t> mapping() const;

    void printQubits(std::vector<size_t> cand = {});
    void printEdges(std::vector<size_t> cand = {});
    void printSingleEdge(size_t a, size_t b);
    void printTopo() const;
    void printPredecessor() const;
    void printDistance() const;
    void printPath(size_t, size_t);

private:
    size_t _id;
    std::string _name;
    size_t _nQubit;
    std::vector<GateType> _gateSet;
    PhyQubitList _qubitList;
    AdjacenciesInfo _adjInfo;
    std::unordered_map<size_t, PhyQubit*> _log2Phy;

    // NOTE - Internal functions/objects only used in reader
    bool parseInfo(std::ifstream& f);
    bool parseGateSet(std::string);
    bool parseSingles(std::string, size_t);
    bool parsePairs(std::string, size_t);
    std::vector<std::vector<size_t>> _adjList;
    std::vector<std::vector<float>> _cxErr;
    std::vector<std::vector<float>> _cxDelay;
    std::vector<float> _sgErr;
    std::vector<float> _sgDelay;

    // NOTE - Containers and helper functions for Floyd-Warshall
    int _maxDist;
    std::vector<std::vector<PhyQubit*>> _predecessor;
    std::vector<std::vector<int>> _distance;
    std::vector<std::vector<int>> _adjMatrix;
    void initFloydWarshall();
    void setWeight(size_t = 0);
};

class Operation {
public:
    friend std::ostream& operator<<(std::ostream&, Operation&);
    friend std::ostream& operator<<(std::ostream&, const Operation&);

    Operation(GateType oper,
              std::tuple<size_t, size_t> qs,
              std::tuple<size_t, size_t> du)
        : oper_(oper), qubits_(qs), duration_(du) {
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
          qubits_(other.qubits_),
          duration_(other.duration_) {}

    Operation& operator=(const Operation& other) {
        oper_ = other.oper_;
        qubits_ = other.qubits_;
        duration_ = other.duration_;
        return *this;
    }

    size_t get_cost() const { return std::get<1>(duration_); }
    size_t get_op_time() const { return std::get<0>(duration_); }
    std::tuple<size_t, size_t> get_duration() const { return duration_; }
    GateType get_operator() const { return oper_; }
    // std::string get_operator_name() const { return operator_get_name(oper_); }
    std::tuple<size_t, size_t> get_qubits() const { return qubits_; }

private:
    GateType oper_;
    std::tuple<size_t, size_t> qubits_;
    std::tuple<size_t, size_t> duration_;  // <from, to>
};

#endif  // TOPOLOGY_H