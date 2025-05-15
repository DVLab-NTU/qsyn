/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define class LatticeSurgeryQubit structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <spdlog/spdlog.h>
#include <cstddef>
#include <string>
#include <vector>
#include <tl/to.hpp>
#include <vector>


#include "latticesurgery/latticesurgery_gate.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::latticesurgery {

enum PatchDirection{
    top,
    down,
    left,
    right
};

class LatticeSurgeryQubit {
public:
    LatticeSurgeryQubit() : _id(0), _logical_id(0) { _connections.resize(4, false); }
    LatticeSurgeryQubit(QubitIdType id) : _id(id), _logical_id(id) { _connections. resize(4,false);}  // Initially, each patch is its own logical qubit
    ~LatticeSurgeryQubit() = default;

    // Basic access methods
    QubitIdType get_id() const { return _id; }
    void set_id(QubitIdType id) { _id = id; }

    // Logical qubit methods
    QubitIdType get_logical_id() const { return _logical_id; }
    void set_logical_id(QubitIdType id) { _logical_id = id; }

    // Gate connection methods
    std::optional<size_t> get_first_gate() const { return _first_gate; }
    std::optional<size_t> get_last_gate() const { return _last_gate; }
    void set_first_gate(std::optional<size_t> gate_id) { _first_gate = gate_id; }
    void set_last_gate(std::optional<size_t> gate_id) { _last_gate = gate_id; }

    // Depth
    void set_depth(size_t t){ _depth = t; }
    size_t get_depth() const { return _depth; }

    // Occupied
    bool occupied() const { return _occupied; }
    void set_occupied(bool occupied) { _occupied = occupied; };


    // Orientation
    bool get_orientation() const { return _orientation; }
    void rotate() { _orientation=!_orientation; }
    MeasureType get_lr_type() const { // false: <-> z | x, true: | z <-> x
        if(_orientation) return MeasureType::x;
        else return MeasureType::z; 
    }; 
    MeasureType get_td_type() const { // false: <-> z | x, true: | z <-> x
        if(_orientation) return MeasureType::z;
        else return MeasureType::x;
    }

    // Patch Connections
    void set_connection(PatchDirection d, bool connect){ _connections[d] = connect; }
    bool get_connection(PatchDirection d) const { return _connections[d]; }

    bool operator==(LatticeSurgeryQubit const& rhs) const {
        return _id == rhs._id && 
               _logical_id == rhs._logical_id && 
               _first_gate == rhs._first_gate && 
               _last_gate == rhs._last_gate;
    }
    bool operator!=(LatticeSurgeryQubit const& rhs) const { return !(*this == rhs); }

private:
    QubitIdType _id;           // Physical patch ID
    QubitIdType _logical_id;   // Logical qubit ID that this patch belongs to
    std::optional<size_t> _first_gate;
    std::optional<size_t> _last_gate;
    size_t _depth = 0;
    std::vector<bool> _connections;
    bool _orientation = false; // false: <-> z | x, true: | z <-> x
    bool _occupied = false;
    

};

} // namespace qsyn::latticesurgery 