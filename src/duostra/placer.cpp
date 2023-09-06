/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Placer member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./placer.hpp"

#include <cassert>
#include <random>

#include "./variables.hpp"
#include "device/device.hpp"

using namespace std;

/**
 * @brief Get the Placer object
 *
 * @return unique_ptr<BasePlacer>
 */
unique_ptr<BasePlacer> get_placer() {
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
vector<size_t> BasePlacer::place_and_assign(Device& device) {
    auto assign = _place(device);
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
vector<size_t> RandomPlacer::_place(Device& device) const {
    vector<size_t> assign;
    for (size_t i = 0; i < device.get_num_qubits(); ++i)
        assign.emplace_back(i);

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
vector<size_t> StaticPlacer::_place(Device& device) const {
    vector<size_t> assign;
    for (size_t i = 0; i < device.get_num_qubits(); ++i)
        assign.emplace_back(i);

    return assign;
}

// SECTION - Class DFSPlacer Member Functions

/**
 * @brief Place logical qubit
 *
 * @param device
 * @return vector<size_t>
 */
vector<size_t> DFSPlacer::_place(Device& device) const {
    vector<size_t> assign;
    vector<bool> qubit_mark(device.get_num_qubits(), false);
    _dfs_device(0, device, assign, qubit_mark);
    assert(assign.size() == device.get_num_qubits());
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
void DFSPlacer::_dfs_device(size_t current, Device& device, vector<size_t>& assign, vector<bool>& qubit_marks) const {
    if (qubit_marks[current]) {
        cout << current << endl;
    }
    assert(!qubit_marks[current]);
    qubit_marks[current] = true;
    assign.emplace_back(current);

    PhysicalQubit const& q = device.get_physical_qubit(current);
    vector<size_t> adjacency_waitlist;

    for (auto& adj : q.get_adjacencies()) {
        // already marked
        if (qubit_marks[adj])
            continue;
        assert(q.get_adjacencies().size() > 0);
        // corner
        if (q.get_adjacencies().size() == 1)
            _dfs_device(adj, device, assign, qubit_marks);
        else
            adjacency_waitlist.emplace_back(adj);
    }

    for (size_t i = 0; i < adjacency_waitlist.size(); ++i) {
        size_t adj = adjacency_waitlist[i];
        if (qubit_marks[adj])
            continue;
        _dfs_device(adj, device, assign, qubit_marks);
    }
    return;
}
