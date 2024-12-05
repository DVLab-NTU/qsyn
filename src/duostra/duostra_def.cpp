/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define duostra common definitions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./duostra_def.hpp"

namespace qsyn::duostra {
/**
 * @brief Get the Scheduler Type Str object
 *
 * @return string
 */
std::string get_scheduler_type_str(SchedulerType const& type) {
    // 0:base 1:static 2:random 3:greedy 4:search
    switch (type) {
        case SchedulerType::base:
            return "base";
        case SchedulerType::naive:
            return "naive";
        case SchedulerType::random:
            return "random";
        case SchedulerType::greedy:
            return "greedy";
        case SchedulerType::search:
        default:
            return "search";
    }
}

/**
 * @brief Get the Router Type Str object
 *
 * @return string
 */
std::string get_router_type_str(RouterType const& type) {
    switch (type) {
        case RouterType::shortest_path:
            return "shortest_path";
        case RouterType::duostra:
        default:
            return "duostra";
    }
}

/**
 * @brief Get the Placer Type Str object
 *
 * @return string
 */
std::string get_placer_type_str(PlacerType const& type) {
    switch (type) {
        case PlacerType::naive:
            return "naive";
        case PlacerType::random:
            return "random";
        case PlacerType::dfs:
        default:
            return "dfs";
    }
}

/**
 * @brief Get the MinMax Type Str object
 *
 * @return string
 */
std::string get_minmax_type_str(MinMaxOptionType const& type) {
    switch (type) {
        case MinMaxOptionType::min:
            return "min";
        case MinMaxOptionType::max:
        default:
            return "max";
    }
}

/**
 * @brief Get the Scheduler object
 *
 * @param str
 * @return size_t
 */
std::optional<SchedulerType> get_scheduler_type(std::string const& str) {
    if (str == "base") return SchedulerType::base;
    if (str == "naive") return SchedulerType::naive;
    if (str == "random") return SchedulerType::random;
    if (str == "greedy") return SchedulerType::greedy;
    if (str == "search") return SchedulerType::search;

    return std::nullopt;
}

/**
 * @brief Get the Router object
 *
 * @param str
 * @return size_t
 */
std::optional<RouterType> get_router_type(std::string const& str) {
    // 0:apsp 1:duostra
    if (str == "shortest_path") return RouterType::shortest_path;
    if (str == "duostra") return RouterType::duostra;

    return std::nullopt;
}

/**
 * @brief Get the Placer object
 *
 * @param str
 * @return size_t
 */
std::optional<PlacerType> get_placer_type(std::string const& str) {
    // 0:static 1:random 2:dfs
    if (str == "naive") return PlacerType::naive;
    if (str == "random") return PlacerType::random;
    if (str == "dfs") return PlacerType::dfs;

    return std::nullopt;
}

std::optional<MinMaxOptionType> get_minmax_type(std::string const& str) {
    if (str == "min") return MinMaxOptionType::min;
    if (str == "max") return MinMaxOptionType::max;

    return std::nullopt;
}

}  // namespace qsyn::duostra
