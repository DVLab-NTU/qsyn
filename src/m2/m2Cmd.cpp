/****************************************************************************
  FileName     [ m2Cmd.cpp ]
  PackageName  [ m2 ]
  Synopsis     [ Define basic m2 package commands ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "m2Cmd.h"

#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

#include "m2.h"
#include "zxGraph.h"
#include "zxGraphMgr.h"

using namespace std;
extern size_t verbose;
extern int effLimit;
extern ZXGraphMgr *zxGraphMgr;

bool initM2Cmd() {
    if (!(cmdMgr->regCmd("M2GAUE", 6, new M2GaussEliCmd) && cmdMgr->regCmd("M2TEST", 6, new M2TestCmd))) {
        cerr << "Registering \"m2\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

CmdExecStatus
M2GaussEliCmd::exec(const string &option) {
    unordered_map<size_t, ZXVertex *> outputList = zxGraphMgr->getGraph()->getOutputList();
    vector<ZXVertex *> front;
    ZXVertexList frontier;
    for (auto [q, v] : outputList) {
        front.push_back(v->getFirstNeighbor().first);
    }
    sort(front.begin(), front.end(), [](const ZXVertex *a, const ZXVertex *b) {
        return a->getQubit() < b->getQubit();
    });
    for (auto v : front) {
        frontier.emplace(v);
    }
    ZXVertexList nebsOfFrontier;
    for (auto v : frontier) {
        vector<ZXVertex *> cands = v->getCopiedNeighbors();
        for (auto [c, _] : v->getNeighbors()) {
            if (c->getType() == VertexType::BOUNDARY) continue;
            if (find(nebsOfFrontier.begin(), nebsOfFrontier.end(), c) == nebsOfFrontier.end()) {
                nebsOfFrontier.insert(c);
            }
        }
    }
    string token;
    M2 m2;
    m2.fromZXVertices(frontier, nebsOfFrontier);
    m2.printMatrix();
    cout << "Is Idendity? " << m2.gaussianElim(true) << endl;
    m2.printMatrix();
    m2.printTrack();
    return CMD_EXEC_DONE;
}

void M2GaussEliCmd::usage(ostream &os) const {
    os << "Usage: M2GAUE" << endl;
}

void M2GaussEliCmd::help() const {
    cout << setw(15) << left << "M2GAUE: "
         << "perform Gaussian elimination" << endl;
}

CmdExecStatus
M2TestCmd::exec(const string &option) {
    string token;
    M2 m2;
    m2.defaultInit();
    m2.printMatrix();
    cout << "Is Idendity? " << m2.gaussianElim(true) << endl;
    m2.printMatrix();
    m2.printTrack();
    return CMD_EXEC_DONE;
}

void M2TestCmd::usage(ostream &os) const {
    os << "Usage: M2TEST" << endl;
}

void M2TestCmd::help() const {
    cout << setw(15) << left << "M2TEST: "
         << "test funct." << endl;
}