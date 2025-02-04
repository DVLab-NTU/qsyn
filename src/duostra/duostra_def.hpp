/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define duostra common definitions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "qcir/qcir_gate.hpp"

namespace qsyn::duostra {

using GateIdToTime = std::vector<std::pair<size_t, size_t>>;
using GateInfo     = std::pair<qcir::QCirGate, std::pair<size_t, size_t>>;

enum class SchedulerType : std::uint8_t {
    base,
    naive,
    random,
    greedy,
    search,
};

enum class PlacerType : std::uint8_t {
    naive,
    random,
    dfs,
    qmdla
};

enum class RouterType : std::uint8_t {
    shortest_path,
    duostra,
};

enum class MinMaxOptionType : std::uint8_t {
    min,
    max,
};

enum class AlgorithmType : std::uint8_t {
    duostra,
    subcircuit,
};

std::string get_scheduler_type_str(SchedulerType const& type);
std::string get_placer_type_str(PlacerType const& type);
std::string get_router_type_str(RouterType const& type);
std::string get_minmax_type_str(MinMaxOptionType const& type);
std::string get_algorithm_type_str(AlgorithmType const& type);

std::optional<SchedulerType> get_scheduler_type(std::string const& str);
std::optional<PlacerType> get_placer_type(std::string const& str);
std::optional<RouterType> get_router_type(std::string const& str);
std::optional<MinMaxOptionType> get_minmax_type(std::string const& str);
std::optional<AlgorithmType> get_algorithm_type(std::string const& str);

struct DuostraConfig {
    SchedulerType scheduler_type;
    RouterType router_type;
    PlacerType placer_type;
    MinMaxOptionType tie_breaking_strategy;  // t/f smaller logical qubit index with little priority
    AlgorithmType algorithm_type; // which strategy is using: duostra or layout_synthesis

    // SECTION - Initialize in Greedy Scheduler
    size_t num_candidates;                     // top k candidates, SIZE_MAX: all candidates
    size_t apsp_coeff;                         // coefficient of apsp cost
    MinMaxOptionType available_time_strategy;  // 0:min 1:max, available time of double-qubit gate is set to min or max of occupied time
    MinMaxOptionType cost_selection_strategy;  // 0:min 1:max, select min or max cost from the waitlist

    // SECTION - Initialize in Search Scheduler
    size_t search_depth;                   // depth of searching region
    bool never_cache;                      // never cache any children unless children() is called
    bool execute_single_qubit_gates_asap;  // execute the single gates when they are available
};

}  // namespace qsyn::duostra
