/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define class Device, Topology, and Operation structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "qcir/gate_type.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/ordered_hashmap.hpp"
#include "util/ordered_hashset.hpp"
#include "util/phase.hpp"

namespace qsyn::qcir {
class QCirGate;
}

namespace qsyn::device {

class Device;
class Topology;
class PhysicalQubit;
struct DeviceInfo;

struct DeviceInfo {
    float _time;
    float _error;
};

std::ostream& operator<<(std::ostream& os, DeviceInfo const& info);

class Topology {
    struct AdjacencyPairHash {
        size_t operator()(std::pair<size_t, size_t> const& k) const {
            return (
                (std::hash<size_t>()(k.first) ^
                 (std::hash<size_t>()(k.second) << 1)) >>
                1);
        }
    };

public:
    using AdjacencyPair     = std::pair<size_t, size_t>;
    using PhysicalQubitInfo = std::unordered_map<size_t, DeviceInfo>;
    using AdjacencyMap      = std::unordered_map<AdjacencyPair, DeviceInfo, AdjacencyPairHash>;
    Topology() {}

    std::string get_name() const { return _name; }
    std::vector<qcir::GateType> const& get_gate_set() const { return _gate_set; }
    DeviceInfo const& get_adjacency_pair_info(size_t a, size_t b);
    DeviceInfo const& get_qubit_info(size_t a);
    size_t get_num_adjacencies() const { return _adjacency_info.size(); }
    void set_num_qubits(size_t n) { _num_qubit = n; }
    void set_name(std::string n) { _name = std::move(n); }
    void add_gate_type(qcir::GateType gt) { _gate_set.emplace_back(gt); }
    void add_adjacency_info(size_t a, size_t b, DeviceInfo info);
    void add_qubit_info(size_t a, DeviceInfo info);

    void print_single_edge(size_t a, size_t b) const;

private:
    std::string _name                     = "";
    size_t _num_qubit                     = 0;
    std::vector<qcir::GateType> _gate_set = {};
    PhysicalQubitInfo _qubit_info         = {};
    AdjacencyMap _adjacency_info          = {};
};

class PhysicalQubit {
public:
    using Adjacencies = dvlab::utils::ordered_hashset<QubitIdType>;
    PhysicalQubit() {}
    PhysicalQubit(QubitIdType id) : _id(id) {}

    void set_id(QubitIdType id) { _id = id; }
    void set_occupied_time(size_t t) { _occupied_time = t; }
    void set_logical_qubit(std::optional<size_t> id) { _logical_qubit = id; }
    void add_adjacency(size_t adj) { _adjacencies.emplace(adj); }

    auto get_id() const { return _id; }
    auto get_occupied_time() const { return _occupied_time; }
    auto is_adjacency(PhysicalQubit const& pq) const { return _adjacencies.contains(pq.get_id()); }
    auto const& get_adjacencies() const { return _adjacencies; }
    auto get_logical_qubit() const { return _logical_qubit; }

    // traversal
    auto get_cost() const { return _cost; }
    auto is_marked() const { return _marked; }
    auto is_taken() const { return _taken; }
    auto get_source() const { return _source; }
    auto get_predecessor() const { return _pred; }
    auto get_swap_time() const { return _swap_time; }

    // NOTE - Duostra functions
    void mark(bool source, QubitIdType pred);
    void take_route(size_t cost, size_t swap_time);
    void reset();

private:
    // NOTE - Device information
    QubitIdType _id          = max_qubit_id;
    Adjacencies _adjacencies = {};

    // NOTE - Duostra parameter
    std::optional<QubitIdType> _logical_qubit = std::nullopt;
    size_t _occupied_time                     = 0;

    bool _marked      = false;
    QubitIdType _pred = 0;
    size_t _cost      = 0;
    size_t _swap_time = 0;
    bool _source      = false;  // false:0, true:1
    bool _taken       = false;
};

class Device {
public:
    using PhysicalQubitList                  = dvlab::utils::ordered_hashmap<QubitIdType, PhysicalQubit>;
    constexpr static size_t default_max_dist = 100000;
    Device() : _topology{std::make_shared<Topology>()} {}

    std::string get_name() const { return _topology->get_name(); }
    size_t get_num_qubits() const { return _num_qubit; }
    PhysicalQubitList const& get_physical_qubit_list() const { return _qubit_list; }
    PhysicalQubit& get_physical_qubit(QubitIdType id) { return _qubit_list[id]; }
    QubitIdType get_physical_by_logical(QubitIdType id);
    std::tuple<QubitIdType, QubitIdType> get_next_swap_cost(QubitIdType source, QubitIdType target);
    bool qubit_id_exists(QubitIdType id) { return _qubit_list.contains(id); }

    void add_physical_qubit(PhysicalQubit q) { _qubit_list[q.get_id()] = std::move(q); }
    void add_adjacency(QubitIdType a, QubitIdType b);

    // NOTE - Duostra
    void apply_gate(qcir::QCirGate const& op, size_t time_begin);
    void apply_single_qubit_gate(QubitIdType physical_id);
    void apply_swap_check(QubitIdType qid0, QubitIdType qid1);
    std::vector<std::optional<size_t>> mapping() const;
    void place(std::vector<QubitIdType> const& assignment);

    // NOTE - All Pairs Shortest Path
    void calculate_path();
    void floyd_warshall();
    std::vector<PhysicalQubit> get_path(QubitIdType src, QubitIdType dest) const;

    bool read_device(std::string const& filename);

    void print_qubits(std::vector<size_t> candidates = {}) const;
    void print_edges(std::vector<size_t> candidates = {}) const;
    void print_topology() const;
    void print_predecessor() const;
    void print_distance() const;
    void print_path(QubitIdType src, QubitIdType dest) const;
    void print_mapping();
    void print_status() const;

private:
    size_t _num_qubit = 0;
    std::shared_ptr<Topology> _topology;
    PhysicalQubitList _qubit_list = {};

    // NOTE - Internal functions only used in reader
    bool _parse_gate_set(std::string const& gate_set_str);
    bool _parse_singles(std::string const& data, std::vector<float>& container);
    bool _parse_float_pairs(std::string const& data, std::vector<std::vector<float>>& containers);
    bool _parse_size_t_pairs(std::string const& data, std::vector<std::vector<size_t>>& containers);
    bool _parse_info(std::ifstream& f, std::vector<std::vector<float>>& cx_error, std::vector<std::vector<float>>& cx_delay, std::vector<float>& single_error, std::vector<float>& single_delay);

    // NOTE - Containers and helper functions for Floyd-Warshall
    int _max_dist = default_max_dist;
    std::vector<std::vector<QubitIdType>> _predecessor;
    std::vector<std::vector<int>> _distance;
    std::vector<std::vector<QubitIdType>> _adjacency_matrix;
    void _initialize_floyd_warshall();
    void _set_weight();
};

}  // namespace qsyn::device

template <>
struct fmt::formatter<qsyn::device::DeviceInfo> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(qsyn::device::DeviceInfo const& info, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "Delay: {:>7.3}    Error: {:7.3}    ", info._time, info._error);
    }
};
