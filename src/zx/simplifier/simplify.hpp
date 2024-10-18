/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <type_traits>

#include "./rules/zx_rules_template.hpp"

extern bool stop_requested();

namespace qsyn::zx {

class ZXGraph;

namespace simplify {

// Basic rules simplification
size_t bialgebra_simp(ZXGraph& g);
size_t state_copy_simp(ZXGraph& g);
size_t phase_gadget_simp(ZXGraph& g);
size_t hadamard_fusion_simp(ZXGraph& g);
size_t hadamard_rule_simp(ZXGraph& g);
size_t identity_removal_simp(ZXGraph& g);
size_t local_complement_simp(ZXGraph& g);
size_t pivot_simp(ZXGraph& g);
size_t pivot_boundary_simp(ZXGraph& g);
size_t pivot_gadget_simp(ZXGraph& g);
size_t spider_fusion_simp(ZXGraph& g);

// Composite simplification routines
size_t interior_clifford_simp(ZXGraph& g);
size_t pi_clifford_simp(ZXGraph& g);
size_t clifford_simp(ZXGraph& g);
void full_reduce(ZXGraph& g);
void dynamic_reduce(ZXGraph& g);
void dynamic_reduce(ZXGraph& g, size_t optimal_t_count);
void symbolic_reduce(ZXGraph& g);
void partition_reduce(ZXGraph& g, size_t n_partitions);
void causal_flow_opt(ZXGraph& g,
                     size_t max_lcomp_unfusions,
                     size_t max_pivot_unfusions);

void to_z_graph(ZXGraph& g);
void to_x_graph(ZXGraph& g);
void to_graph_like(ZXGraph& g);

void report_simplification_result(
    std::string_view rule_name, std::span<size_t> match_counts);
/**
 * @brief apply the rule on the zx graph
 *
 * @return number of iterations
 */
template <typename Rule>
requires std::is_base_of<ZXRuleTemplate<typename Rule::MatchType>, Rule>::value
size_t simplify(ZXGraph& g, Rule const& rule) {
    hadamard_rule_simp(g);

    std::vector<size_t> match_counts;

    while (!stop_requested()) {
        auto const matches = rule.find_matches(g);
        if (matches.empty()) {
            break;
        }
        match_counts.emplace_back(matches.size());

        rule.apply(g, matches);
    }

    report_simplification_result(rule.get_name(), match_counts);

    return match_counts.size();
}

/**
 * @brief apply the rule on the zx graph
 *
 * @return number of iterations
 */
template <typename Rule>
requires std::is_base_of<ZXRuleTemplate<typename Rule::MatchType>, Rule>::value
size_t hadamard_simplify(ZXGraph& g, Rule rule) {
    std::vector<size_t> match_counts;

    while (!stop_requested()) {
        auto const old_vertex_count = g.num_vertices();
        auto const matches          = rule.find_matches(g);

        if (matches.empty()) {
            break;
        }
        match_counts.emplace_back(matches.size());

        rule.apply(g, matches);
        if (g.num_vertices() >= old_vertex_count) break;
    }

    report_simplification_result(rule.get_name(), match_counts);

    return match_counts.size();
}

}  // namespace simplify

}  // namespace qsyn::zx
