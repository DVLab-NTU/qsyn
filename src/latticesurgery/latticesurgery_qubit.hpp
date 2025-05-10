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

#include "qsyn/qsyn_type.hpp"

namespace qsyn::latticesurgery {

class LatticeSurgeryQubit {
public:
    LatticeSurgeryQubit() = default;
    LatticeSurgeryQubit(QubitIdType id) : _id(id) {}
    ~LatticeSurgeryQubit() = default;

    // Basic access methods
    QubitIdType get_id() const { return _id; }
    void set_id(QubitIdType id) { _id = id; }

    // Gate connection methods
    std::optional<size_t> get_first_gate() const { return _first_gate; }
    std::optional<size_t> get_last_gate() const { return _last_gate; }
    void set_first_gate(std::optional<size_t> gate_id) { _first_gate = gate_id; }
    void set_last_gate(std::optional<size_t> gate_id) { _last_gate = gate_id; }

    bool operator==(LatticeSurgeryQubit const& rhs) const {
        return _id == rhs._id && _first_gate == rhs._first_gate && _last_gate == rhs._last_gate;
    }
    bool operator!=(LatticeSurgeryQubit const& rhs) const { return !(*this == rhs); }

private:
    QubitIdType _id;
    std::optional<size_t> _first_gate;
    std::optional<size_t> _last_gate;
};

} // namespace qsyn::latticesurgery 