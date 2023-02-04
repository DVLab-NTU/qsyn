/****************************************************************************
  FileName     [ topology.cpp ]
  PackageName  [ topology ]
  Synopsis     [ Define class Topology functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "topology.h"

#include <stdlib.h>      // for abort

#include <cassert>       // for assert
#include <fstream>       // for ifstream
#include <string>        // for string

#include "qcirGate.h"    // for QCirGate
#include "textFormat.h"  // for TextFormat

using namespace std;
namespace TF = TextFormat;
extern size_t verbose;

AdjInfo defaultInfo = {._cnotTime = 0.0, ._error = 0.0};

/**
 * @brief Print information of physical qubit
 *
 * @param detail If true, print delay and error
 */
void PhyQubit::printInfo(bool detail) const {
    cout << "ID:" << right << setw(4) << _id << "    ";
    if (detail) {
        cout << "Delay:" << right << setw(8) << _gateDelay << "    ";
        cout << "Error:" << right << setw(8) << _error << "    ";
    }
    cout << "Adjs:";
    for (auto& q : _adjacencies) {
        cout << right << setw(3) << q->getId() << " ";
    }
    cout << endl;
}

/**
 * @brief Get the information of a single pair
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 * @return AdjInfo&
 */
const AdjInfo& DeviceTopo::getAdjPairInfo(size_t a, size_t b) {
    if (a > b) swap(a, b);
    return _adjInfo[make_pair(a, b)];
}

/**
 * @brief Add adjacency pair (a,b)
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 */
void DeviceTopo::addAdjacency(size_t a, size_t b) {
    if (a > b) swap(a, b);
    if (!qubitIdExist(a)) {
        PhyQubit* temp = new PhyQubit(a);
        addPhyQubit(temp);
    }
    if (!qubitIdExist(b)) {
        PhyQubit* temp = new PhyQubit(b);
        addPhyQubit(temp);
    }
    PhyQubit* qa = _qubitList[a];
    PhyQubit* qb = _qubitList[b];
    qa->addAdjacency(qb);
    qb->addAdjacency(qa);

    addAdjacencyInfo(a, b, defaultInfo);
}

/**
 * @brief Add adjacency information of (a,b)
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 * @param info Information of this pair
 */
void DeviceTopo::addAdjacencyInfo(size_t a, size_t b, AdjInfo info) {
    if (a > b) swap(a, b);
    _adjInfo[make_pair(a, b)] = info;
}

/**
 * @brief Read Device Topology
 *
 * @param filename
 * @return true
 * @return false
 */
bool DeviceTopo::readTopo(const string& filename) {
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

    _name = stripWhitespaces(data);

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
    // TODO - Store gate set
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

    if (!parsePairs(data, 0)) return false;
    // NOTE - Parse Information
    if (!parseInfo(topoFile)) return false;
    // NOTE - Finish parsing, store the topology
    for (size_t i = 0; i < _adjList.size(); i++) {
        for (size_t j = 0; j < _adjList[i].size(); j++) {
            if (_adjList[i][j] > i) {
                addAdjacency(i, _adjList[i][j]);
                addAdjacencyInfo(i, _adjList[i][j], {._cnotTime = _cxDelay[i][j], ._error = _cxErr[i][j]});
            }
        }
        if (i < _sgDelay.size()) _qubitList[i]->setDelay(_sgDelay[i]);
        if (i < _sgErr.size()) _qubitList[i]->setError(_sgErr[i]);
    }
    return true;
}

bool DeviceTopo::parseGateSet(string str) {
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
        _gateSet.push_back(str2GateType[gt]);
    }
    return true;
}

/**
 * @brief Parse device information including SGERROR, SGTIME, CNOTERROR, and CNOTTIME
 *
 * @param f
 * @return true
 * @return false
 */
bool DeviceTopo::parseInfo(std::ifstream& f) {
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
            if (!parseSingles(data, 0)) return false;
        } else if (token == "SGTIME") {
            if (!parseSingles(data, 1)) return false;
        } else if (token == "CNOTERROR") {
            if (!parsePairs(data, 1)) return false;
        } else if (token == "CNOTTIME") {
            if (!parsePairs(data, 2)) return false;
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
 * @param data raw information string, i.e. [0.1, ...]
 * @param which 0: SGERROR, 1:SGTIME
 * @return true
 * @return false
 */
bool DeviceTopo::parseSingles(string data, size_t which) {
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
        if (which == 0)
            _sgErr.push_back(fl);
        else if (which == 1)
            _sgDelay.push_back(fl);
    }
    return true;
}

/**
 * @brief Parse device edges information
 *
 * @param data raw information string, i.e. [[1, 3, ...], [0, ... ,], ...]
 * @param which 0:COUPLINGMAP, 1:CNOTERROR, 2:CNOTTIME
 * @return true
 * @return false
 */
bool DeviceTopo::parsePairs(string data, size_t which) {
    string str, num;
    unsigned qbn;
    float fl;
    size_t n = 0, m = 0;
    while (n < data.size()) {
        n = myStrGetTok(data, str, n, '[');
        str = str.substr(0, str.find_first_of("]"));

        m = 0;
        vector<size_t> single;
        vector<float> singleFl;
        while (m < str.size()) {
            m = myStrGetTok(str, num, m, ',');
            num = stripWhitespaces(num);
            if (which == 0) {
                if (!myStr2Uns(num, qbn) || qbn >= _nQubit) {
                    cout << "Error: The number of qubit `" << num << "` is not a positive integer or not in the legal range!!\n";
                    return false;
                }
                single.push_back(size_t(qbn));
            } else {
                if (!myStr2Float(num, fl)) {
                    cout << "Error: The number `" << num << "` is not a float!!\n";
                    return false;
                }

                singleFl.push_back(fl);
            }
        }
        if (which == 0)
            _adjList.push_back(single);
        else if (which == 1)
            _cxErr.push_back(singleFl);
        else if (which == 2)
            _cxDelay.push_back(singleFl);
    }
    return true;
}

/**
 * @brief Print physical qubits and their adjacencies
 *
 * @param cand a vector of qubits to be printed
 */
void DeviceTopo::printQubits(vector<size_t> cand) {
    cout << endl;
    if (cand.empty()) {
        for (size_t i = 0; i < _nQubit; i++) {
            _qubitList[i]->printInfo(true);
        }
        cout << "Total #Qubits: " << _nQubit << endl;
    } else {
        sort(cand.begin(), cand.end());
        for (auto& p : cand) {
            _qubitList[p]->printInfo(true);
        }
    }
}

/**
 * @brief Print device edge
 *
 * @param cand Empty: print all. Single element [a]: print edges connecting to a. Two elements [a,b]: print edge (a,b).
 */
void DeviceTopo::printEdges(vector<size_t> cand) {
    cout << endl;
    if (cand.size() == 0) {
        size_t cnt = 0;
        for (size_t i = 0; i < _nQubit; i++) {
            for (auto& q : _qubitList[i]->getAdjacencies()) {
                if (i < q->getId()) {
                    cnt++;
                    printSingleEdge(i, q->getId());
                }
            }
        }
        assert(cnt == _adjInfo.size());
        cout << "Total #Edges: " << _adjInfo.size() << endl;
    } else if (cand.size() == 1) {
        for (auto& q : _qubitList[cand[0]]->getAdjacencies()) {
            printSingleEdge(cand[0], q->getId());
        }
        cout << "Total #Edges: " << _qubitList[cand[0]]->getAdjacencies().size() << endl;
    } else if (cand.size() == 2) {
        printSingleEdge(cand[0], cand[1]);
    }
}

/**
 * @brief Print information of the edge (a,b)
 *
 * @param a Id of first qubit
 * @param b Id of second qubit
 */
void DeviceTopo::printSingleEdge(size_t a, size_t b) {
    auto& adjp = _adjInfo[make_pair(a, b)];
    cout << "(" << right << setw(3) << _qubitList[a]->getId() << ", " << right << setw(3) << _qubitList[b]->getId() << ")    ";
    cout << "Delay:" << right << setw(8) << adjp._cnotTime << "    ";
    cout << "Error:" << right << setw(8) << adjp._error << endl;
}

/**
 * @brief Print information of Device Topology
 *
 */
void DeviceTopo::printTopo() const {
    cout << "Topology " << right << setw(2) << _id << ": " << _name << "( "
         << _qubitList.size() << " qubits, "
         << _adjInfo.size() << " edges )\n";

    cout << "Gate Set   : ";
    for (size_t i = 0; i < _gateSet.size(); i++) {
        cout << gateType2Str[_gateSet[i]];
        if (i != _gateSet.size() - 1) cout << ", ";
    }
    cout << endl;
}