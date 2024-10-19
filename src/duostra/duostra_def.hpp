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
    astar,
};

enum class PlacerType : std::uint8_t {
    naive,
    random,
    dfs,
};

enum class RouterType : std::uint8_t {
    shortest_path,
    duostra,
};

enum class MinMaxOptionType : std::uint8_t {
    min,
    max,
};

std::string get_scheduler_type_str(SchedulerType const& type);
std::string get_placer_type_str(PlacerType const& type);
std::string get_router_type_str(RouterType const& type);
std::string get_minmax_type_str(MinMaxOptionType const& type);

std::optional<SchedulerType> get_scheduler_type(std::string const& str);
std::optional<PlacerType> get_placer_type(std::string const& str);
std::optional<RouterType> get_router_type(std::string const& str);
std::optional<MinMaxOptionType> get_minmax_type(std::string const& str);

struct DuostraConfig {
    static SchedulerType SCHEDULER_TYPE;
    static RouterType ROUTER_TYPE;
    static PlacerType PLACER_TYPE;
    static MinMaxOptionType TIE_BREAKING_STRATEGY;  // t/f smaller logical qubit index with little priority

    // SECTION - Initialize in Greedy Scheduler
    static size_t NUM_CANDIDATES;                     // top k candidates, SIZE_MAX: all candidates
    static size_t APSP_COEFF;                         // coefficient of apsp cost
    static MinMaxOptionType AVAILABLE_TIME_STRATEGY;  // 0:min 1:max, available time of double-qubit gate is set to min or max of occupied time
    static MinMaxOptionType COST_SELECTION_STRATEGY;  // 0:min 1:max, select min or max cost from the waitlist

    // SECTION - Initialize in Search Scheduler
    static size_t SEARCH_DEPTH;                   // depth of searching region
    static bool NEVER_CACHE;                      // never cache any children unless children() is called
    static bool EXECUTE_SINGLE_QUBIT_GATES_ASAP;  // execute the single gates when they are available
};

}  // namespace qsyn::duostra
