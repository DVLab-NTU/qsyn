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
    // TODO - Read the topology. Need to keep the order of adjacency map.
    return true;
}