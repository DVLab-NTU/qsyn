/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <memory>
#include <type_traits>

#include "./zx_rules_template.hpp"

extern size_t VERBOSE;
extern bool stop_requested();

class ZXGraph;

class Simplifier {
public:
    Simplifier(ZXGraph* g) : _simp_graph{g} {
        hadamard_rule_simp();
    }

    template <typename Rule>
    size_t simplify(Rule const& rule) {
        static_assert(std::is_base_of<ZXRuleTemplate<typename Rule::MatchType>, Rule>::value, "Rule must be a subclass of ZXRule");

        if (VERBOSE >= 5) std::cout << std::setw(30) << std::left << rule.name;
        if (VERBOSE >= 8) std::cout << std::endl;

        std::vector<int> match_counts;

        size_t iterations = 0;
        while (!stop_requested()) {
            std::vector<typename Rule::MatchType> matches = rule.find_matches(*_simp_graph);
            if (matches.empty()) {
                break;
            }
            match_counts.emplace_back(matches.size());
            iterations++;

            if (VERBOSE >= 8) std::cout << "\nIteration " << iterations << ":" << std::endl
                                        << ">>>" << std::endl;
            rule.apply(*_simp_graph, matches);
            if (VERBOSE >= 8) std::cout << "<<<" << std::endl;
        }

        _recipe.emplace_back(rule.name, match_counts);
        if (VERBOSE >= 8) std::cout << "=> ";
        if (VERBOSE >= 5) {
            std::cout << iterations << " iterations." << std::endl;
            for (size_t i = 0; i < match_counts.size(); i++) {
                std::cout << "  " << i + 1 << ") " << match_counts[i] << " matches" << std::endl;
            }
        }
        if (VERBOSE >= 5) std::cout << "\n";

        return iterations;
    }

    template <typename Rule>
    size_t hadamard_simplify(Rule rule) {
        static_assert(std::is_base_of<HZXRuleTemplate<typename Rule::MatchType>, Rule>::value, "Rule must be a subclass of HZXRule");

        if (VERBOSE >= 5) std::cout << std::setw(30) << std::left << rule.name;
        if (VERBOSE >= 8) std::cout << std::endl;

        std::vector<int> match_counts;

        size_t iterations = 0;
        while (!stop_requested()) {
            size_t vcount = _simp_graph->get_num_vertices();

            std::vector<typename Rule::MatchType> matches = rule.find_matches(*_simp_graph);
            if (matches.empty()) {
                break;
            }
            match_counts.emplace_back(matches.size());
            iterations++;

            if (VERBOSE >= 8) std::cout << "\nIteration " << iterations << ":" << std::endl
                                        << ">>>" << std::endl;
            rule.apply(*_simp_graph, matches);
            if (VERBOSE >= 8) std::cout << "<<<" << std::endl;
            if (_simp_graph->get_num_vertices() >= vcount) break;
        }

        _recipe.emplace_back(rule.name, match_counts);
        if (VERBOSE >= 8) std::cout << "=> ";
        if (VERBOSE >= 5) {
            std::cout << iterations << " iterations." << std::endl;
            for (size_t i = 0; i < match_counts.size(); i++) {
                std::cout << "  " << i + 1 << ") " << match_counts[i] << " matches" << std::endl;
            }
        }
        if (VERBOSE >= 5) std::cout << "\n";

        return iterations;
    }

    template <typename Rule>
    size_t scoped_simplify(Rule const& rule, ZXVertexList const& scope) {
        static_assert(std::is_base_of<ZXRuleTemplate<typename Rule::MatchType>, Rule>::value, "Rule must be a subclass of ZXRule");

        std::vector<int> match_counts;

        size_t iterations = 0;
        while (!stop_requested()) {
            std::vector<typename Rule::MatchType> matches = rule.find_matches(*_simp_graph);
            std::vector<typename Rule::MatchType> scoped_matches;
            auto is_in_scope = [&scope](ZXVertex* v) { return scope.contains(v); };
            for (auto& match : matches) {
                std::vector<ZXVertex*> match_vertices = rule.flatten_vertices(match);
                if (std::any_of(match_vertices.begin(), match_vertices.end(), is_in_scope)) {
                    scoped_matches.push_back(match);
                }
            }
            if (matches.empty()) {
                break;
            }
            match_counts.emplace_back(matches.size());
            iterations++;

            rule.apply(*_simp_graph, matches);
        }

        return iterations;
    }

    // Basic rules simplification
    int bialgebra_simp();
    int state_copy_simp();
    int phase_gadget_simp();
    int hadamard_fusion_simp();
    int hadamard_rule_simp();
    int identity_removal_simp();
    int local_complement_simp();
    int pivot_simp();
    int pivot_boundary_simp();
    int pivot_gadget_simp();
    int spider_fusion_simp();

    // action
    void to_z_graph();
    void to_x_graph();
    int interior_clifford_simp();
    int pi_clifford_simp();
    int clifford_simp();
    void full_reduce();
    void dynamic_reduce();
    void dynamic_reduce(size_t optimal_t_count);
    void symbolic_reduce();
    void partition_reduce(size_t n_partitions, size_t iterations);

    // print function
    void print_recipe();

private:
    ZXGraph* _simp_graph;
    std::vector<std::tuple<std::string, std::vector<int> > > _recipe;
};
