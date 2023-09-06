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
#include <string>
#include <unordered_map>

#include "qcir/gate_type.hpp"
#include "util/ordered_hashmap.hpp"
#include "util/ordered_hashset.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

class Device;
class Topology;
class PhysicalQubit;
class Operation;
struct DeviceInfo;

struct DeviceInfo {
    float _time;
    float _error;
};

template <>
struct fmt::formatter<DeviceInfo> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(DeviceInfo const& info, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "Delay: {:>7.3}    Error: {:7.3}    ", info._time, info._error);
    }
};

std::ostream& operator<<(std::ostream& os, DeviceInfo const& info);

class Topology {
public:
    using AdjacencyPair = std::pair<size_t, size_t>;
    using PhysicalQubitInfo = std::unordered_map<size_t, DeviceInfo>;
    struct AdjacencyPairHash {
        size_t operator()(std::pair<size_t, size_t> const& k) const {
            return (
                (std::hash<size_t>()(k.first) ^
                 (std::hash<size_t>()(k.second) << 1)) >>
                1);
        }
    };
    using AdjacencyMap = std::unordered_map<AdjacencyPair, DeviceInfo, AdjacencyPairHash>;
    Topology() {}

    std::string get_name() const { return _name; }
    std::vector<GateType> const& get_gate_set() const { return _gate_set; }
    DeviceInfo const& get_adjacency_pair_info(size_t a, size_t b);
    DeviceInfo const& get_qubit_info(size_t a);
    size_t get_num_adjacencies() const { return _adjacency_info.size(); }
    void set_num_qubits(size_t n) { _num_qubit = n; }
    void set_name(std::string n) { _name = n; }
    void add_gate_type(GateType gt) { _gate_set.emplace_back(gt); }
    void add_adjacency_info(size_t a, size_t b, DeviceInfo info);
    void add_qubit_info(size_t a, DeviceInfo info);

    void print_single_edge(size_t a, size_t b) const;

private:
    std::string _name;
    size_t _num_qubit = 0;
    std::vector<GateType> _gate_set;
    PhysicalQubitInfo _qubit_info;
    AdjacencyMap _adjacency_info;
};

class PhysicalQubit {
public:
    using Adjacencies = ordered_hashset<size_t>;
    PhysicalQubit() {}
    PhysicalQubit(const size_t id) : _id(id) {}

    void set_id(size_t id) { _id = id; }
    void set_occupied_time(size_t t) { _occupied_time = t; }
    void set_logical_qubit(size_t id) { _logical_qubit = id; }
    void add_adjacency(size_t adj) { _adjacencies.emplace(adj); }

    size_t get_id() const { return _id; }
    size_t get_occupied_time() const { return _occupied_time; }
    bool is_adjacency(PhysicalQubit const& pq) const { return _adjacencies.contains(pq.get_id()); }
    Adjacencies const& get_adjacencies() const { return _adjacencies; }
    size_t get_logical_qubit() const { return _logical_qubit; }

    // traversal
    size_t get_cost() const { return _cost; }
    bool is_marked() { return _marked; }
    bool is_taken() { return _taken; }
    bool get_source() const { return _source; }
    size_t get_predecessor() const { return _pred; }
    size_t get_swap_time() const { return _swap_time; }

    // NOTE - Duostra functions
    void mark(bool source, size_t pred);
    void take_route(size_t cost, size_t swap_time);
    void reset();

private:
    // NOTE - Device information
    size_t _id = SIZE_MAX;
    Adjacencies _adjacencies;

    // NOTE - Duostra parameter
    size_t _logical_qubit = SIZE_MAX;
    size_t _occupied_time = 0;

    bool _marked = false;
    size_t _pred = 0;
    size_t _cost = 0;
    size_t _swap_time = 0;
    bool _source = false;  // false:0, true:1
    bool _taken = false;
};

class Device {
public:
    using PhysicalQubitList = ordered_hashmap<size_t, PhysicalQubit>;
    constexpr static size_t max_dist = 100000;
    Device() : _max_dist{max_dist}, _topology{std::make_shared<Topology>()} {}

    std::string get_name() const { return _topology->get_name(); }
    size_t get_num_qubits() const { return _num_qubit; }
    PhysicalQubitList const& get_physical_qubit_list() const { return _qubit_list; }
    PhysicalQubit& get_physical_qubit(size_t id) { return _qubit_list[id]; }
    size_t get_physical_by_logical(size_t id);
    std::tuple<size_t, size_t> get_next_swap_cost(size_t source, size_t target);
    bool qubit_id_exists(size_t id) { return _qubit_list.contains(id); }

    void set_num_qubits(size_t n) { _num_qubit = n; }
    void add_physical_qubit(PhysicalQubit q) { _qubit_list[q.get_id()] = q; }
    void add_adjacency(size_t a, size_t b);

    // NOTE - Duostra
    void apply_gate(Operation const& op);
    void apply_single_qubit_gate(size_t physical_id);
    void apply_swap_check(size_t qid0, size_t qid1);
    std::vector<size_t> mapping() const;
    void place(std::vector<size_t> const& assignment);

    // NOTE - All Pairs Shortest Path
    void calculate_path();
    void floyd_warshall();
    std::vector<PhysicalQubit> get_path(size_t, size_t) const;

    bool read_device(std::string const&);

    void print_qubits(std::vector<size_t> candidates = {}) const;
    void print_edges(std::vector<size_t> candidates = {}) const;
    void print_topology() const;
    void print_predecessor() const;
    void print_distance() const;
    void print_path(size_t source, size_t destination) const;
    void print_mapping();
    void print_status() const;

private:
    size_t _num_qubit = 0;
    std::shared_ptr<Topology> _topology;
    PhysicalQubitList _qubit_list;

    // NOTE - Internal functions only used in reader
    bool _parse_gate_set(std::string gate_set_str);
    bool _parse_singles(std::string data, std::vector<float>& container);
    bool _parse_float_pairs(std::string data, std::vector<std::vector<float>>& containers);
    bool _parse_size_t_pairs(std::string data, std::vector<std::vector<size_t>>& containers);
    bool _parse_info(std::ifstream& f, std::vector<std::vector<float>>& cx_error, std::vector<std::vector<float>>& cx_delay, std::vector<float>& single_error, std::vector<float>& single_delay);

    // NOTE - Containers and helper functions for Floyd-Warshall
    int _max_dist;
    std::vector<std::vector<size_t>> _predecessor;
    std::vector<std::vector<int>> _distance;
    std::vector<std::vector<int>> _adjacency_matrix;
    void _initialize_floyd_warshall();
    void _set_weight();
};

class Operation {
public:
    friend std::ostream& operator<<(std::ostream&, Operation&);
    friend std::ostream& operator<<(std::ostream&, Operation const&);

    Operation(GateType, Phase, std::tuple<size_t, size_t>, std::tuple<size_t, size_t>);

    GateType get_type() const { return _oper; }
    Phase get_phase() const { return _phase; }
    size_t get_cost() const { return std::get<1>(_duration); }
    size_t get_operation_time() const { return std::get<0>(_duration); }
    std::tuple<size_t, size_t> get_duration() const { return _duration; }
    std::tuple<size_t, size_t> get_qubits() const { return _qubits; }

    size_t get_id() const { return _id; }
    void set_id(size_t id) { _id = id; }

private:
    GateType _oper;
    Phase _phase;
    std::tuple<size_t, size_t> _qubits;
    std::tuple<size_t, size_t> _duration;  // <from, to>
    size_t _id;
};
