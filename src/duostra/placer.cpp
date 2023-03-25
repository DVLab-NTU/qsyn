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

/**
 * @brief Get the Placer object
 *
 * @return unique_ptr<BasePlacer>
 */
unique_ptr<BasePlacer> getPlacer() {
    // 0:static 1:random 2:dfs
    if (DUOSTRA_PLACER == 0) {
        return make_unique<StaticPlacer>();
    } else if (DUOSTRA_PLACER == 1) {
        return make_unique<RandomPlacer>();
    } else if (DUOSTRA_PLACER == 2) {
        return make_unique<DFSPlacer>();
    }
    cerr << "Error: placer type not found." << endl;
    abort();
}

// SECTION - Class BasePlacer Member Functions

/**
 * @brief Place and assign logical qubit
 *
 * @param device
 */
vector<size_t> BasePlacer::placeAndAssign(Device& device) {
    auto assign = place(device);
    device.place(assign);
    return assign;
}

// SECTION - Class RandomPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
vector<size_t> RandomPlacer::place(Device& device) const {
    vector<size_t> assign;
    for (size_t i = 0; i < device.getNQubit(); ++i)
        assign.push_back(i);

    size_t seed = chrono::system_clock::now().time_since_epoch().count();
    shuffle(assign.begin(), assign.end(), default_random_engine(seed));
    return assign;
}

// SECTION - Class StaticPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
vector<size_t> StaticPlacer::place(Device& device) const {
    vector<size_t> assign;
    for (size_t i = 0; i < device.getNQubit(); ++i)
        assign.push_back(i);

    return assign;
}

// SECTION - Class DFSPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
vector<size_t> DFSPlacer::place(Device& device) const {
    vector<size_t> assign;
    vector<bool> qubitMark(device.getNQubit(), false);
    DFSDevice(0, device, assign, qubitMark);
    assert(assign.size() == device.getNQubit());
    return assign;
}

/**
 * @brief Depth-first search the device
 *
 * @param current
 * @param device
 * @param assign
 * @param qubitMark
 */
void DFSPlacer::DFSDevice(size_t current, Device& device, vector<size_t>& assign, vector<bool>& qubitMark) const {
    if (qubitMark[current]) {
        cout << current << endl;
    }
    assert(!qubitMark[current]);
    qubitMark[current] = true;
    assign.push_back(current);

    const PhysicalQubit& q = device.getPhysicalQubit(current);
    vector<size_t> adjacencyWaitlist;

    for (auto& adj : q.getAdjacencies()) {
        // already marked
        if (qubitMark[adj])
            continue;
        assert(q.getAdjacencies().size() > 0);
        // corner
        if (q.getAdjacencies().size() == 1)
            DFSDevice(adj, device, assign, qubitMark);
        else
            adjacencyWaitlist.push_back(adj);
    }

    for (size_t i = 0; i < adjacencyWaitlist.size(); ++i) {
        size_t adj = adjacencyWaitlist[i];
        if (qubitMark[adj])
            continue;
        DFSDevice(adj, device, assign, qubitMark);
    }
    return;
}
