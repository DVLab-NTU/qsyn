/****************************************************************************
  FileName     [ device.cpp ]
  PackageName  [ device ]
  Synopsis     [ Define class Device, Topology, and Operation functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "device.h"

#include <stdlib.h>  // for abort

#include <cassert>  // for assert
#include <fstream>  // for ifstream
#include <limits>   // for _maxDist
#include <string>   // for string

#include "qcirGate.h"    // for QCirGate
#include "textFormat.h"  // for TextFormat

using namespace std;
namespace TF = TextFormat;
extern size_t verbose;

// SECTION - Struct Info Member Functions

Info defaultInfo = {._time = 0.0, ._error = 0.0};

/**
 * @brief Print overloading
 *
 * @param os
 * @param info
 * @return ostream&
 */
ostream& operator<<(ostream& os, const Info& info) {
    os << "Delay:" << right << setw(8) << fixed << setprecision(3) << info._time << "    ";
    return os << "Error:" << right << setw(8) << fixed << setprecision(5) << info._error << "    ";
}

// SECTION - Class Topology Member Functions

/**
 * @brief Get the information of a single adjacency pair
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 * @return Info&
 */
const Info& Topology::getAdjPairInfo(size_t a, size_t b) {
    if (a > b) swap(a, b);
    return _adjInfo[make_pair(a, b)];
}

/**
 * @brief Get the information of a qubit
 *
 * @param a
 * @return const Info&
 */
const Info& Topology::getQubitInfo(size_t a) {
    return _qubitInfo[a];
}

/**
 * @brief Add adjacency information of (a,b)
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 * @param info Information of this pair
 */
void Topology::addAdjacencyInfo(size_t a, size_t b, Info info) {
    if (a > b) swap(a, b);
    _adjInfo[make_pair(a, b)] = info;
}

/**
 * @brief Add qubit information
 *
 * @param a
 * @param info
 */
void Topology::addQubitInfo(size_t a, Info info) {
    _qubitInfo[a] = info;
}

/**
 * @brief Print information of the edge (a,b)
 *
 * @param a Index of first qubit
 * @param b Index of second qubit
 */
void Topology::printSingleEdge(size_t a, size_t b) const {
    pair<size_t, size_t> query = (a < b) ? make_pair(a, b) : make_pair(b, a);
    if (_adjInfo.contains(query)) {
        const auto& adjp = _adjInfo.at(query);
        cout << "(" << right << setw(3) << a << ", " << right << setw(3) << b << ")    ";
        cout << "Delay:" << right << setw(8) << fixed << setprecision(3) << adjp._time << "    ";
        cout << "Error:" << right << setw(8) << fixed << setprecision(5) << adjp._error << endl;
    } else {
        cout << "No connection between " << a << " and " << b << "." << endl;
    }
}

// SECTION - Class PhysicalQubit Member Functions

/**
 * @brief Construct a new Physical Qubit:: Physical Qubit object
 *
 * @param id
 */
PhysicalQubit::PhysicalQubit(const size_t id) : _id(id), _adjacencies({}), _logicalQubit(ERROR_CODE), _occupiedTime(0), _marked(false), _pred(0), _cost(0), _swapTime(0), _source(false), _taken(false) {}

/**
 * @brief Construct a new Physical Qubit:: Physical Qubit object
 *
 * @param other
 */
PhysicalQubit::PhysicalQubit(const PhysicalQubit& other)
    : _id(other._id),
      _adjacencies(other._adjacencies),
      _logicalQubit(other._logicalQubit),
      _occupiedTime(other._occupiedTime),
      _marked(other._marked),
      _pred(other._pred),
      _cost(other._cost),
      _swapTime(other._swapTime),
      _source(other._source),
      _taken(other._taken) {}

/**
 * @brief Construct a new Physical Qubit:: Physical Qubit object
 *
 * @param other
 */
PhysicalQubit::PhysicalQubit(PhysicalQubit&& other)
    : _id(other._id),
      _adjacencies(move(other._adjacencies)),
      _logicalQubit(other._logicalQubit),
      _occupiedTime(other._occupiedTime),
      _marked(other._marked),
      _pred(other._pred),
      _cost(other._cost),
      _swapTime(other._swapTime),
      _source(other._source),
      _taken(other._taken) {}

/**
 * @brief Assignment operator overloading for PhysicalQubit
 *
 * @param other
 * @return PhysicalQubit&
 */
PhysicalQubit& PhysicalQubit::operator=(const PhysicalQubit& other) {
    _id = other._id;
    _adjacencies = other._adjacencies;
    _logicalQubit = other._logicalQubit;
    _occupiedTime = other._occupiedTime;
    _marked = other._marked;
    _pred = other._pred;
    _cost = other._cost;
    _swapTime = other._swapTime;
    _source = other._source;
    _taken = other._taken;
    return *this;
}

/**
 * @brief Assignment operator overloading for PhysicalQubit
 *
 * @param other
 * @return PhysicalQubit&
 */
PhysicalQubit& PhysicalQubit::operator=(PhysicalQubit&& other) {
    _id = other._id;
    _adjacencies = move(other._adjacencies);
    _logicalQubit = other._logicalQubit;
    _occupiedTime = other._occupiedTime;
    _marked = other._marked;
    _pred = other._pred;
    _cost = other._cost;
    _swapTime = other._swapTime;
    _source = other._source;
    _taken = other._taken;
    return *this;
}

/**
 * @brief Operator overloading
 *
 * @param os
 * @param q
 * @return ostream&
 */
ostream& operator<<(ostream& os, const PhysicalQubit& q) {
    return os << "Q" << right << setw(2) << q.getId() << ", logical: " << right << setw(2) << q.getLogicalQubit() << ", lock until " << q.getOccupiedTime();
}

/**
 * @brief Mark qubit
 *
 * @param source false: from 0, true: from 1
 * @param pred predecessor
 */
void PhysicalQubit::mark(bool source, size_t pred) {
    _marked = true;
    _source = source;
    _pred = pred;
}

/**
 * @brief Take the route
 *
 * @param cost
 * @param swapTime
 */
void PhysicalQubit::takeRoute(size_t cost, size_t swapTime) {
    _cost = cost;
    _swapTime = swapTime;
    _taken = true;
}

/**
 * @brief Reset qubit
 *
 */
void PhysicalQubit::reset() {
    _marked = false;
    _taken = false;
    _cost = _occupiedTime;
}

// SECTION - Class Device Member Functions

/**
 * @brief Construct a new Device:: Device object
 *
 * @param id
 */
Device::Device(size_t id) : _id(id) {
    _maxDist = 100000;
    _topology = make_shared<Topology>(id);
}

/**
 * @brief Get next swap cost
 *
 * @param source
 * @param target
 * @return tuple<size_t, size_t> (index of next qubit, cost)
 */
tuple<size_t, size_t> Device::getNextSwapCost(size_t source, size_t target) {
    size_t nextIdx = _predecessor[source][target];
    PhysicalQubit& qSource = getPhysicalQubit(source);
    PhysicalQubit& qNext = getPhysicalQubit(nextIdx);
    size_t cost = max(qSource.getOccupiedTime(), qNext.getOccupiedTime());

    assert(qSource.isAdjacency(qNext));
    return make_tuple(nextIdx, cost);
}

/**
 * @brief Get physical qubit id by logical id
 *
 * @param id logical
 * @return size_t
 */
size_t Device::getPhysicalbyLogical(size_t id) {
    for (auto& [_, phy] : _qubitList) {
        if (phy.getLogicalQubit() == id) {
            return phy.getId();
        }
    }
    return ERROR_CODE;
}

/**
 * @brief Add adjacency pair (a,b)
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 */
void Device::addAdjacency(size_t a, size_t b) {
    if (a > b) swap(a, b);
    if (!qubitIdExist(a)) {
        PhysicalQubit temp = PhysicalQubit(a);
        addPhyQubit(temp);
    }
    if (!qubitIdExist(b)) {
        PhysicalQubit temp = PhysicalQubit(b);
        addPhyQubit(temp);
    }
    _qubitList[a].addAdjacency(_qubitList[b].getId());
    _qubitList[b].addAdjacency(_qubitList[a].getId());

    _topology->addAdjacencyInfo(a, b, defaultInfo);
}

/**
 * @brief Apply gate to device
 *
 * @param op
 */
void Device::applyGate(const Operation& op) {
    tuple<size_t, size_t> qubits = op.getQubits();
    PhysicalQubit& q0 = getPhysicalQubit(get<0>(qubits));
    PhysicalQubit& q1 = getPhysicalQubit(get<1>(qubits));
    size_t t = op.getOperationTime();

    switch (op.getType()) {
        case GateType::SWAP: {
            size_t temp = q0.getLogicalQubit();
            q0.setLogicalQubit(q1.getLogicalQubit());
            q1.setLogicalQubit(temp);
            q0.setOccupiedTime(t + SWAP_DELAY);
            q1.setOccupiedTime(t + SWAP_DELAY);
            break;
        }
        case GateType::CX: {
            q0.setOccupiedTime(t + DOUBLE_DELAY);
            q1.setOccupiedTime(t + DOUBLE_DELAY);
            break;
        }
        case GateType::CZ: {
            q0.setOccupiedTime(t + DOUBLE_DELAY);
            q1.setOccupiedTime(t + DOUBLE_DELAY);
            break;
        }
        default:
            assert(false);
    }
}

/**
 * @brief Apply swap, only used in checker
 *
 * @param op
 */
void Device::applySwapCheck(size_t q0Id, size_t q1Id) {
    PhysicalQubit& q0 = getPhysicalQubit(q0Id);
    PhysicalQubit& q1 = getPhysicalQubit(q1Id);
    size_t temp = q0.getLogicalQubit();
    q0.setLogicalQubit(q1.getLogicalQubit());
    q1.setLogicalQubit(temp);
    size_t t = max(q0.getOccupiedTime(), q1.getOccupiedTime());
    q0.setOccupiedTime(t + DOUBLE_DELAY);
    q1.setOccupiedTime(t + DOUBLE_DELAY);
}

/**
 * @brief Apply single-qubit gate
 *
 * @param physicalId
 */
void Device::applySingleQubitGate(size_t physicalId) {
    size_t startTime = _qubitList[physicalId].getOccupiedTime();
    _qubitList[physicalId].setOccupiedTime(startTime + SINGLE_DELAY);
    _qubitList[physicalId].reset();
}

/**
 * @brief get mapping of physical qubit
 *
 * @return vector<size_t> (index of physical qubit)
 */
vector<size_t> Device::mapping() const {
    vector<size_t> ret;
    ret.resize(_qubitList.size());
    for (const auto& [id, qubit] : _qubitList) {
        ret[id] = qubit.getLogicalQubit();
    }
    return ret;
}

/**
 * @brief Place logical qubit
 *
 * @param assign
 */
void Device::place(const vector<size_t>& assign) {
    for (size_t i = 0; i < assign.size(); ++i) {
        assert(_qubitList[assign[i]].getLogicalQubit() == ERROR_CODE);
        _qubitList[assign[i]].setLogicalQubit(i);
    }
}

/**
 * @brief Calculate Shortest Path
 *
 */
void Device::calculatePath() {
    _predecessor.clear();
    _distance.clear();
    _adjMatrix.clear();
    _adjMatrix.resize(_nQubit);
    for (size_t i = 0; i < _nQubit; i++) {
        _adjMatrix[i].resize(_nQubit, _maxDist);
        for (size_t j = 0; j < _nQubit; j++) {
            if (i == j)
                _adjMatrix[i][j] = 0;
        }
    }
    FloydWarshall();
    _adjMatrix.clear();
}

/**
 * @brief Init data for Floyd-Warshall Algorithm
 *
 */
void Device::initFloydWarshall() {
    _distance.resize(_nQubit);
    _predecessor.resize(_nQubit);

    for (size_t i = 0; i < _nQubit; i++) {
        _distance[i].resize(_nQubit);
        _predecessor[i].resize(_nQubit, size_t(-1));
        for (size_t j = 0; j < _nQubit; j++) {
            _distance[i][j] = _adjMatrix[i][j];
            if (_distance[i][j] != 0 && _distance[i][j] != _maxDist) {
                _predecessor[i][j] = _qubitList[i].getId();
            }
        }
    }
    if (verbose >= 5) {
        printPredecessor();
        printDistance();
    }
}

/**
 * @brief Set weight of edge used in Floyd-Warshall
 *
 * @param type
 */
void Device::setWeight(size_t type) {
    assert(_adjMatrix.size() == _nQubit);
    for (size_t i = 0; i < _nQubit; i++) {
        for (const auto& adj : _qubitList[i].getAdjacencies()) {
            _adjMatrix[i][adj] = 1;
        }
    }
}

/**
 * @brief Floyd-Warshall Algorithm. Solve All Pairs Shortest Path (APSP)
 *
 */
void Device::FloydWarshall() {
    setWeight();
    initFloydWarshall();
    for (size_t k = 0; k < _nQubit; k++) {
        if (verbose >= 5) cout << "Including vertex(" << k << "):" << endl;
        for (size_t i = 0; i < _nQubit; i++) {
            for (size_t j = 0; j < _nQubit; j++) {
                if ((_distance[i][j] > _distance[i][k] + _distance[k][j]) && (_distance[i][k] != _maxDist)) {
                    _distance[i][j] = _distance[i][k] + _distance[k][j];
                    _predecessor[i][j] = _predecessor[k][j];
                }
            }
        }
        if (verbose >= 5) {
            printPredecessor();
            printDistance();
        }
    }
}

/**
 * @brief Get shortest path from `s` to `t`
 *
 * @param s start
 * @param t terminate
 * @return vector<PhyQubit>&
 */
vector<PhysicalQubit> Device::getPath(size_t s, size_t t) const {
    vector<PhysicalQubit> path;
    path.push_back(_qubitList.at(s));
    if (s == t) return path;
    size_t newPred = _predecessor[t][s];
    path.push_back(newPred);
    while (true) {
        newPred = _predecessor[t][newPred];
        if (newPred == size_t(-1)) break;
        path.push_back(_qubitList.at(newPred));
    }
    return path;
}

/**
 * @brief Read Device
 *
 * @param filename
 * @return true
 * @return false
 */
bool Device::readDevice(const string& filename) {
    ifstream topoFile(filename);
    if (!topoFile.is_open()) {
        cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
        return false;
    }
    string str = "", token = "", data = "";

    // NOTE - Device name
    while (str == "") {
        getline(topoFile, str);
        str = stripLeadingSpacesAndComments(str);
    }
    size_t token_end = myStrGetTok(str, token, 0, ": ");
    data = str.substr(token_end + 1);

    _topology->setName(stripWhitespaces(data));

    // NOTE - Qubit num
    str = "", token = "", data = "";
    unsigned qbn;
    while (str == "") {
        getline(topoFile, str);
        str = stripLeadingSpacesAndComments(str);
    }
    token_end = myStrGetTok(str, token, 0, ": ");
    data = str.substr(token_end + 1);
    data = stripWhitespaces(data);
    if (!myStr2Uns(data, qbn)) {
        cout << "Error: The number of qubit is not a positive integer!!\n";
        return false;
    }
    _nQubit = size_t(qbn);

    // NOTE - Gate set
    str = "", token = "", data = "";
    while (str == "") {
        getline(topoFile, str);
        str = stripLeadingSpacesAndComments(str);
    }
    if (!parseGateSet(str)) return false;

    // NOTE - Coupling map
    str = "", token = "", data = "";
    while (str == "") {
        getline(topoFile, str);
        str = stripLeadingSpacesAndComments(str);
    }

    token_end = myStrGetTok(str, token, 0, ": ");
    data = str.substr(token_end + 1);
    data = stripWhitespaces(data);
    data = removeBracket(data, '[', ']');
    vector<vector<float>> cxErr, cxDelay;
    vector<vector<size_t>> adjList;
    vector<float> sgErr, sgDelay;
    if (!parsePairsSizeT(data, adjList))
        return false;

    // NOTE - Parse Information
    if (!parseInfo(topoFile, cxErr, cxDelay, sgErr, sgDelay)) return false;

    // NOTE - Finish parsing, store the topology
    for (size_t i = 0; i < adjList.size(); i++) {
        for (size_t j = 0; j < adjList[i].size(); j++) {
            if (adjList[i][j] > i) {
                addAdjacency(i, adjList[i][j]);
                _topology->addAdjacencyInfo(i, adjList[i][j], {._time = cxDelay[i][j], ._error = cxErr[i][j]});
            }
        }
    }
    assert(sgErr.size() == sgDelay.size());
    for (size_t i = 0; i < sgErr.size(); i++) {
        _topology->addQubitInfo(i, {._time = sgDelay[i], ._error = sgErr[i]});
    }
    calculatePath();
    return true;
}

/**
 * @brief Parse gate set
 *
 * @param str
 * @return true
 * @return false
 */
bool Device::parseGateSet(string str) {
    string token = "", data = "", gt;
    size_t token_end = myStrGetTok(str, token, 0, ": ");
    data = str.substr(token_end + 1);
    data = stripWhitespaces(data);
    data = removeBracket(data, '{', '}');
    size_t m = 0;
    while (m < data.size()) {
        m = myStrGetTok(data, gt, m, ',');
        gt = stripWhitespaces(gt);
        for_each(gt.begin(), gt.end(), [](char& c) { c = ::tolower(c); });
        if (!str2GateType.contains(gt)) {
            cout << "Error: unsupported gate type " << gt << "!!" << endl;
            return false;
        }
        _topology->addGateType(str2GateType[gt]);
    }
    return true;
}

/**
 * @brief Parse device information including SGERROR, SGTIME, CNOTERROR, and CNOTTIME
 *
 * @param f
 * @param cxErr
 * @param cxDelay
 * @param sgErr
 * @param sgDelay
 * @return true
 * @return false
 */
bool Device::parseInfo(std::ifstream& f, vector<vector<float>>& cxErr, vector<vector<float>>& cxDelay, vector<float>& sgErr, vector<float>& sgDelay) {
    string str = "", token = "", data = "";
    while (true) {
        while (str == "") {
            if (f.eof()) break;
            getline(f, str);
            str = stripLeadingSpacesAndComments(str);
        }
        size_t token_end = myStrGetTok(str, token, 0, ": ");
        data = str.substr(token_end + 1);

        data = stripWhitespaces(data);
        if (token == "SGERROR") {
            if (!parseSingles(data, sgErr)) return false;
        } else if (token == "SGTIME") {
            if (!parseSingles(data, sgDelay)) return false;
        } else if (token == "CNOTERROR") {
            if (!parsePairsFloat(data, cxErr)) return false;
        } else if (token == "CNOTTIME") {
            if (!parsePairsFloat(data, cxDelay)) return false;
        }
        if (f.eof()) {
            break;
        }
        getline(f, str);
        str = stripLeadingSpacesAndComments(str);
    }

    return true;
}

/**
 * @brief Parse device qubits information
 *
 * @param data
 * @param container
 * @return true
 * @return false
 */
bool Device::parseSingles(string data, vector<float>& container) {
    string str, num;
    float fl;
    size_t m = 0;

    str = removeBracket(data, '[', ']');

    vector<float> singleFl;
    while (m < str.size()) {
        m = myStrGetTok(str, num, m, ',');
        num = stripWhitespaces(num);
        if (!myStr2Float(num, fl)) {
            cout << "Error: The number `" << num << "` is not a float!!\n";
            return false;
        }
        container.emplace_back(fl);
    }
    return true;
}

/**
 * @brief Parse device edges information with type is float
 *
 * @param data
 * @param container
 * @return true
 * @return false
 */
bool Device::parsePairsFloat(string data, vector<vector<float>>& container) {
    string str, num;
    float fl;
    size_t n = 0, m = 0;
    while (n < data.size()) {
        n = myStrGetTok(data, str, n, '[');
        str = str.substr(0, str.find_first_of("]"));
        m = 0;
        vector<float> singleFl;
        while (m < str.size()) {
            m = myStrGetTok(str, num, m, ',');
            num = stripWhitespaces(num);
            if (!myStr2Float(num, fl)) {
                cout << "Error: The number `" << num << "` is not a float!!\n";
                return false;
            }
            singleFl.push_back(fl);
        }
        container.push_back(singleFl);
    }
    return true;
}

/**
 * @brief Parse device edges information with type is size_t
 *
 * @param data
 * @param container
 * @return true
 * @return false
 */
bool Device::parsePairsSizeT(string data, vector<vector<size_t>>& container) {
    string str, num;
    unsigned qbn;
    size_t n = 0, m = 0;
    while (n < data.size()) {
        n = myStrGetTok(data, str, n, '[');
        str = str.substr(0, str.find_first_of("]"));
        m = 0;
        vector<size_t> single;
        while (m < str.size()) {
            m = myStrGetTok(str, num, m, ',');
            num = stripWhitespaces(num);
            if (!myStr2Uns(num, qbn) || qbn >= _nQubit) {
                cout << "Error: The number of qubit `" << num << "` is not a positive integer or not in the legal range!!\n";
                return false;
            }
            single.push_back(size_t(qbn));
        }
        container.push_back(single);
    }
    return true;
}

/**
 * @brief Print physical qubits and their adjacencies
 *
 * @param cand a vector of qubits to be printed
 */
void Device::printQubits(vector<size_t> cand) const {
    for (auto& c : cand) {
        if (c >= _nQubit) {
            cout << "Error: the maximum qubit id is " << _nQubit - 1 << "!!" << endl;
            return;
        }
    }
    cout << endl;
    vector<PhysicalQubit> qubits;
    qubits.resize(_nQubit);
    for (const auto& [idx, info] : _qubitList) {
        qubits[idx] = info;
    }
    if (cand.empty()) {
        for (size_t i = 0; i < qubits.size(); i++) {
            cout << "ID:" << right << setw(4) << i << "    ";
            cout << _topology->getQubitInfo(i) << "Adjs:";
            for (auto& q : qubits[i].getAdjacencies()) {
                cout << right << setw(3) << q << " ";
            }
            cout << endl;
        }
        cout << "Total #Qubits: " << _nQubit << endl;
    } else {
        sort(cand.begin(), cand.end());
        for (auto& p : cand) {
            cout << "ID:" << right << setw(4) << p << "    ";
            cout << _topology->getQubitInfo(p) << "Adjs:";
            for (auto& q : qubits[p].getAdjacencies()) {
                cout << right << setw(3) << q << " ";
            }
            cout << endl;
        }
    }
}

/**
 * @brief Print device edge
 *
 * @param cand Empty: print all. Single element [a]: print edges connecting to a. Two elements [a,b]: print edge (a,b).
 */
void Device::printEdges(vector<size_t> cand) const {
    for (auto& c : cand) {
        if (c >= _nQubit) {
            cout << "Error: the maximum qubit id is " << _nQubit - 1 << "!!" << endl;
            return;
        }
    }
    cout << endl;
    vector<PhysicalQubit> qubits;
    qubits.resize(_nQubit);
    for (const auto& [idx, info] : _qubitList) {
        qubits[idx] = info;
    }
    if (cand.size() == 0) {
        size_t cnt = 0;
        for (size_t i = 0; i < _nQubit; i++) {
            for (auto& q : qubits[i].getAdjacencies()) {
                if (i < q) {
                    cnt++;
                    _topology->printSingleEdge(i, q);
                }
            }
        }
        assert(cnt == _topology->getAdjSize());
        cout << "Total #Edges: " << cnt << endl;
    } else if (cand.size() == 1) {
        for (auto& q : qubits[cand[0]].getAdjacencies()) {
            _topology->printSingleEdge(cand[0], q);
        }
        cout << "Total #Edges: " << qubits[cand[0]].getAdjacencies().size() << endl;
    } else if (cand.size() == 2) {
        _topology->printSingleEdge(cand[0], cand[1]);
    }
}

/**
 * @brief Print information of Topology
 *
 */
void Device::printTopology() const {
    cout << "Topology " << right << setw(2) << _id << ": " << getName() << "( "
         << _qubitList.size() << " qubits, "
         << _topology->getAdjSize() << " edges )\n";

    cout << "Gate Set   : ";
    for (size_t i = 0; i < _topology->getGateSet().size(); i++) {
        cout << gateType2Str[_topology->getGateSet()[i]];
        if (i != _topology->getGateSet().size() - 1) cout << ", ";
    }
    cout << endl;
}

/**
 * @brief Print Predecessor
 *
 */
void Device::printPredecessor() const {
    cout << "Predecessor Matrix:" << endl;
    for (size_t i = 0; i < _nQubit; i++) {
        for (size_t j = 0; j < _nQubit; j++) {
            if (_predecessor[i][j] == size_t(-1)) {
                cout << setw(5) << "/";
            } else
                cout << setw(5) << _predecessor[i][j];
        }
        cout << endl;
    }
}

/**
 * @brief Print Distance
 *
 */
void Device::printDistance() const {
    cout << "Distance Matrix:" << endl;
    for (size_t i = 0; i < _nQubit; i++) {
        for (size_t j = 0; j < _nQubit; j++) {
            if (_distance[i][j] == _maxDist) {
                cout << setw(5) << "X";
            } else
                cout << setw(5) << _distance[i][j];
        }
        cout << endl;
    }
}

/**
 * @brief Print shortest path from `s` to `t`
 *
 * @param s start
 * @param t terminate
 */
void Device::printPath(size_t s, size_t t) const {
    cout << endl;
    for (auto& c : {s, t}) {
        if (c >= _nQubit) {
            cout << "Error: the maximum qubit id is " << _nQubit - 1 << "!!" << endl;
            return;
        }
    }
    const vector<PhysicalQubit>& path = getPath(s, t);
    if (path.front().getId() != s && path.back().getId() != t)
        cout << "No path between " << s << "and " << t;
    else {
        cout << "Path from " << s << " to " << t << ":" << endl;
        size_t cnt = 1;
        for (auto& v : path) {
            cout << setw(4) << v.getId() << " ";
            if (cnt % 10 == 0) cout << endl;
            cnt++;
        }
    }
}

/**
 * @brief Print Mapping (Physical : Logical)
 *
 */
void Device::printMapping() {
    cout << "----------Mapping---------" << endl;
    for (size_t i = 0; i < _nQubit; i++) {
        cout << left << setw(5) << i << " : " << getPhysicalQubit(i).getLogicalQubit() << endl;
    }
}

/**
 * @brief Print device status
 *
 */
void Device::printStatus() const {
    cout << endl;
    cout << "Device Status:" << endl;
    vector<PhysicalQubit> qubits;
    qubits.resize(_nQubit);
    for (const auto& [idx, info] : _qubitList) {
        qubits[idx] = info;
    }
    for (size_t i = 0; i < qubits.size(); ++i) {
        cout << qubits[i] << "\n";
    }
    cout << "\n";
}

// SECTION - Class Operation Member Functions

/**
 * @brief Print overloading
 *
 * @param os
 * @param op
 * @return ostream&
 */
ostream& operator<<(ostream& os, Operation& op) {
    os << left;
    size_t from = get<0>(op._duration);
    size_t to = get<1>(op._duration);
    os << setw(20) << "Operation: " + gateType2Str[op._oper];
    os << "Q" << get<0>(op._qubits);
    if (get<1>(op._qubits) != ERROR_CODE) os << " Q" << get<1>(op._qubits);
    os << "    from: " << left << setw(10) << from << "to: " << to;
    return os;
}

/**
 * @brief Print overloading
 *
 * @param os
 * @param op
 * @return ostream&
 */
ostream& operator<<(ostream& os, const Operation& op) {
    os << left;
    size_t from = get<0>(op._duration);
    size_t to = get<1>(op._duration);
    os << setw(20) << "Operation: " + gateType2Str[op._oper];
    os << "Q" << get<0>(op._qubits) << " Q" << get<1>(op._qubits)
       << "    from: " << left << setw(10) << from << "to: " << to;
    return os;
}

/**
 * @brief Construct a new Operation:: Operation object
 *
 * @param oper
 * @param ph
 * @param qs
 * @param du
 */
Operation::Operation(GateType oper, Phase ph, tuple<size_t, size_t> qs, tuple<size_t, size_t> du)
    : _oper(oper), _phase(ph), _qubits(qs), _duration(du), _id(ERROR_CODE) {
    // sort qs
    size_t a = get<0>(qs);
    size_t b = get<1>(qs);
    assert(a != b);
    // if (a > b)
    //     _qubits = make_tuple(b, a);
}

/**
 * @brief Construct a new Operation:: Operation object
 *
 * @param other
 */
Operation::Operation(const Operation& other)
    : _oper(other._oper),
      _phase(other._phase),
      _qubits(other._qubits),
      _duration(other._duration),
      _id(other._id) {}

/**
 * @brief Assignment operator overloading for Operation
 *
 * @param other
 * @return Operation&
 */
Operation& Operation::operator=(const Operation& other) {
    _oper = other._oper;
    _phase = other._phase;
    _qubits = other._qubits;
    _duration = other._duration;
    _id = other._id;
    return *this;
}