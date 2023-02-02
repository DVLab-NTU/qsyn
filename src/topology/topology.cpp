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

    // NOTE - Coupling map

    str = "", token = "", data = "";

    while (str == "") {
        getline(topoFile, str);
        str = stripLeadingSpacesAndComments(str);
    }

    token_end = myStrGetTok(str, token, 0, ": ");
    data = str.substr(token_end + 1);

    data = stripWhitespaces(data);

    size_t lastfound = str.find_last_of("]");
    size_t firstfound = str.find_first_of("[");
    data = str.substr(firstfound + 1, lastfound - firstfound - 1);

    // TODO - Move to a function if more information would be used in future
    vector<vector<size_t>> adjlist;

    string adjstr, adjNum;

    size_t n = 0, m = 0;
    while (n < data.size()) {
        n = myStrGetTok(data, adjstr, n, '[');
        adjstr = adjstr.substr(0, adjstr.find_first_of("]"));

        m = 0;
        vector<size_t> single;
        while (m < adjstr.size()) {
            m = myStrGetTok(adjstr, adjNum, m, ',');
            adjNum = stripWhitespaces(adjNum);

            if (!myStr2Uns(adjNum, qbn) || qbn >= _nQubit) {
                cout << "Error: The number of qubit `" << adjNum << "` is not a positive integer or not in the legal range!!\n";
                return false;
            }
            single.push_back(size_t(qbn));
        }
        adjlist.push_back(single);
    }
    //////
    // TODO - Parse Information
    // NOTE - Finish parsing, store the topology

    for (size_t i = 0; i < adjlist.size(); i++) {
        for (size_t j = 0; j < adjlist[i].size(); j++) {
            if (adjlist[i][j] > i) {
                addAdjacency(i, adjlist[i][j]);
            }
        }
    }
    return true;
}

/**
 * @brief Print physical qubits and their adjacencies
 *
 * @param cand a vector of qubits to be printed
 */
void DeviceTopo::printQubits(vector<size_t> cand) {
    if (cand.empty()) {
        for (size_t i = 0; i < _nQubit; i++) {
            cout << _qubitList[i]->getId() << ": ";
            for (auto& q : _qubitList[i]->getAdjacencies()) {
                cout << q->getId() << " ";
            }
            cout << endl;
        }
    } else {
        sort(cand.begin(), cand.end());
        for (auto& p : cand) {
            cout << _qubitList[p]->getId() << ": ";
            for (auto& q : _qubitList[p]->getAdjacencies()) {
                cout << q->getId() << " ";
            }
            cout << endl;
        }
    }
}

/**
 * @brief Print device edge
 *
 */
void DeviceTopo::printEdges() {
    // TODO - Sort
    for (auto& [adjp, p] : _adjInfo) {
        cout << adjp.first << " -- " << adjp.second << ": ";
        cout << endl;
    }
}