/****************************************************************************
  FileName     [ placer.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Placer member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "placer.h"

#include <random>

using namespace std;

unique_ptr<BasePlacer> get(const string& typ) {
    if (typ == "static") {
        return make_unique<StaticPlacer>();
    } else if (typ == "random") {
        return make_unique<RandomPlacer>();
    } else if (typ == "dfs") {
        return make_unique<DFSPlacer>();
    }
    cerr << typ << " is not a placer type." << endl;
    abort();
}

vector<size_t> RandomPlacer::place(DeviceTopo* device) const {
    std::vector<size_t> assign;
    for (size_t i = 0; i < device->getNQubit(); ++i) {
        assign.push_back(i);
    }
    size_t seed = std::chrono::system_clock::now().time_since_epoch().count();

    shuffle(assign.begin(), assign.end(), std::default_random_engine(seed));
    return assign;
}

vector<size_t> StaticPlacer::place(DeviceTopo* device) const {
    std::vector<size_t> assign;
    for (size_t i = 0; i < device->getNQubit(); ++i) {
        assign.push_back(i);
    }
    return assign;
}

vector<size_t> DFSPlacer::place(DeviceTopo* device) const {
    vector<size_t> assign;
    vector<bool> qubit_mark(device->getNQubit(), false);
    dfs_device(0, device, assign, qubit_mark);
    assert(assign.size() == device->getNQubit());
    return assign;
}

void DFSPlacer::dfs_device(size_t current, DeviceTopo* device, vector<size_t>& assign, vector<bool>& qubit_mark) const {
    if (qubit_mark[current]) {
        cout << current << endl;
    }
    assert(!qubit_mark[current]);
    qubit_mark[current] = true;
    assign.push_back(current);

    PhyQubit* q = device->getPhysicalQubit(current);
    vector<size_t> adj_waitlist;

    for (auto& adjQ : q->getAdjacencies()) {
        size_t adj = adjQ->getId();
        // already marked
        if (qubit_mark[adj])
            continue;
        assert(adjQ->getAdjacencies().size() > 0);

        // corner
        if (adjQ->getAdjacencies().size() == 1) {
            dfs_device(adj, device, assign, qubit_mark);
        } else {
            adj_waitlist.push_back(adj);
        }
    }

    for (size_t i = 0; i < adj_waitlist.size(); ++i) {
        size_t adj = adj_waitlist[i];
        if (qubit_mark[adj]) {
            continue;
        }
        dfs_device(adj, device, assign, qubit_mark);
    }
    return;
}
