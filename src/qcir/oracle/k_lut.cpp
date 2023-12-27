/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ A rudimentary implementation of the Quantum-Aware Partitioning algorithm from the paper  ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/2005.00211 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./k_lut.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <map>
#include <numeric>
#include <ranges>
#include <set>
#include <tl/enumerate.hpp>
#include <tl/zip.hpp>

#include "qcir/oracle/xag.hpp"

namespace {

using namespace qsyn::qcir;
using std::views::iota;
using tl::views::enumerate;
using tl::views::zip;

template <typename T>
bool is_subset_of(const std::set<T>& a, const std::set<T>& b) {
    // return true if all members of a are also in b
    if (a.size() > b.size())
        return false;

    auto const not_found = b.end();
    for (auto const& element : a)
        if (b.find(element) == not_found)
            return false;

    return true;
}

/*
 * @brief enumerates all cuts of size up to max_cut_size for each node in the XAG *
 * @param xag
 * @param max_cut_size maximum size of a cut
 */
std::map<XAGNodeID, std::vector<XAGCut>> enumerate_cuts(XAG& xag, const size_t max_cut_size) {
    auto topological_order = xag.calculate_topological_order();
    std::map<XAGNodeID, std::vector<XAGCut>> node_id_to_cuts;
    for (auto const id : topological_order) {
        node_id_to_cuts[id] = {};
        node_id_to_cuts[id].push_back({id});
        if (xag.get_node(id)->get_type() == XAGNodeType::INPUT) {
            continue;
        }
        auto fanins = xag.get_node(id)->fanins;
        for (auto const& cut0 : node_id_to_cuts[fanins[0]]) {
            for (auto const& cut1 : node_id_to_cuts[fanins[1]]) {
                auto cut = cut0;
                cut.insert(cut1.begin(), cut1.end());

                if (cut.size() <= max_cut_size) {
                    if (!std::any_of(node_id_to_cuts[id].begin(), node_id_to_cuts[id].end(), [&cut](auto const& c) { return is_subset_of(c, cut); })) {
                        node_id_to_cuts[id].push_back(cut);
                    }
                }
            }
        }
    }

    std::vector<XAGNodeID> to_remove;
    for (auto& [id, cuts] : node_id_to_cuts) {
        std::erase(cuts, std::set<XAGNodeID>{id});
        if (cuts.size() == 0) {
            to_remove.push_back(id);
        }
    }
    for (auto const& id : to_remove) {
        node_id_to_cuts.erase(id);
    }

    return node_id_to_cuts;
}

std::vector<bool> calculate_truth_table(XAG& xag, XAGNodeID const& node_id, XAGCut const& cut) {
    auto node_ids_in_cone = get_cone_node_ids(xag, node_id, cut);
    std::vector<bool> truth_table;

    for (auto const& _minterm : iota(0UL, 1UL << cut.size())) {
        std::map<size_t, bool> intermediate_results;
        for (auto const& [i, id] : enumerate(cut)) {
            intermediate_results.insert({id.get(), (_minterm >> i) & 1});
        }

        for (auto const& id : node_ids_in_cone) {
            if (cut.contains(id)) {
                continue;
            }

            auto const node = xag.get_node(id);

            std::vector<bool> inputs;
            for (auto const& [fanin_id, inverted] : zip(node->fanins, node->inverted)) {
                inputs.push_back(inverted ^ intermediate_results[fanin_id.get()]);
            }

            bool result{};
            if (node->get_type() == XAGNodeType::XOR) {
                for (auto const& input : inputs) {
                    result ^= input;
                }
            } else if (node->get_type() == XAGNodeType::AND) {
                result = true;
                for (auto const& input : inputs) {
                    result &= input;
                }
            } else if (node->get_type() == XAGNodeType::INPUT) {
                throw std::runtime_error("cannot calculate truth table for inuput node");
            }
            intermediate_results.insert({id.get(), result});
        }

        truth_table.emplace_back(intermediate_results[node_id.get()]);
    }

    return truth_table;
}

int hadamrad_entry(const size_t& k, const size_t& i, const size_t& j) {
    static std::map<std::tuple<size_t, size_t, size_t>, int> cache;
    if (k == 2) {
        return (i == 1 && j == 1) ? -1 : 1;
    }
    if (!cache.contains({k, i, j})) {
        const size_t k_half = k / 2;
        int sign            = (i >= k_half && j >= k_half) ? -1 : 1;
        cache[{k, i, j}]    = sign * hadamrad_entry(k_half, i % k_half, j % k_half);
    }
    return cache[{k, i, j}];
}

size_t caclculate_radamacher_walsh_cost(std::vector<bool> const& truth_table) {
    size_t cost        = 0;
    size_t const k     = truth_table.size();
    std::vector<int> F = std::vector<int>(k, 0);
    for (auto const& i : iota(0UL, k)) {
        F[i] = truth_table[i] ? -1 : 1;
    }
    for (auto const& i : iota(0UL, k)) {
        int row_sum = 0;
        for (auto const& j : iota(0UL, k)) {
            row_sum += hadamrad_entry(k, i, j) * F[j];
        }
        cost += row_sum != 0 ? 1 : 0;
    }
    return cost;
}

std::map<XAGNodeID, std::vector<size_t>> calculate_cut_costs(XAG& xag, std::map<XAGNodeID, std::vector<XAGCut>> const& all_cuts) {
    std::map<XAGNodeID, std::vector<size_t>> costs;

    for (auto const& [id, cuts] : all_cuts) {
        costs[id] = {};
        for (auto const& cut : cuts) {
            auto const truth_table = calculate_truth_table(xag, id, cut);
            costs[id].emplace_back(caclculate_radamacher_walsh_cost(truth_table));
        }
    }

    for (auto const& [id, cuts] : all_cuts) {
        for (auto const& [i, cut] : enumerate(cuts)) {
            auto const& cone_node_ids = get_cone_node_ids(xag, id, cut);
            bool no_and               = true;
            size_t xor_count          = 0;
            for (auto const& cone_node_id : cone_node_ids) {
                if (xag.get_node(cone_node_id)->get_type() == XAGNodeType::AND) {
                    no_and = false;
                    break;
                } else if (xag.get_node(cone_node_id)->get_type() == XAGNodeType::XOR) {
                    xor_count++;
                }
            }
            if (no_and && xor_count >= 2) {
                costs[id][i] = 0;
            }
        }
    }

    return costs;
}

}  // namespace

namespace qsyn::qcir {

std::set<XAGNodeID> get_cone_node_ids(XAG& xag, XAGNodeID const& node_id, XAGCut const& cut) {
    std::set<XAGNodeID> cone_node_ids;
    std::vector<XAGNodeID> stack;
    stack.push_back(node_id);
    while (!stack.empty()) {
        auto const node_id = stack.back();
        stack.pop_back();
        for (auto const& fanin_id : xag.get_node(node_id)->fanins) {
            if (cut.contains(node_id) && !cut.contains(fanin_id)) {
                continue;
            }
            if (!cone_node_ids.contains(fanin_id)) {
                stack.push_back(fanin_id);
            }
        }
        cone_node_ids.insert(node_id);
    }
    return cone_node_ids;
}

std::pair<std::map<XAGNodeID, XAGCut>, std::map<XAGNodeID, size_t>> k_lut_partition(XAG& xag, const size_t max_cut_size) {
    auto id_to_cuts  = enumerate_cuts(xag, max_cut_size);
    auto id_to_costs = calculate_cut_costs(xag, id_to_cuts);

    std::map<XAGNodeID, XAGCut> optimal_cuts;
    std::map<XAGNodeID, size_t> optimal_costs;
    auto topological_order = xag.calculate_topological_order();
    for (auto const id : topological_order) {
        if (xag.get_node(id)->get_type() == XAGNodeType::INPUT) {
            optimal_cuts[id]  = {id};
            optimal_costs[id] = 0;
            continue;
        }

        optimal_costs[id] = INT_MAX;
        for (auto const& [cut, cost] : zip(id_to_cuts[id], id_to_costs[id])) {
            size_t acc_cost = cost + std::accumulate(cut.begin(), cut.end(), 0, [&optimal_costs](size_t x, XAGNodeID y) { return x + optimal_costs[y]; });
            if (acc_cost < optimal_costs[id]) {
                optimal_costs[id] = acc_cost;
                optimal_cuts[id]  = cut;
            }
        }
    }

    std::set<XAGNodeID> necessary_node_ids;
    auto input_node_ids  = std::set<XAGNodeID>(xag.inputs.begin(), xag.inputs.end());
    auto output_node_ids = std::set<XAGNodeID>(xag.outputs.begin(), xag.outputs.end());
    for (auto const id : std::views::reverse(topological_order)) {
        auto const& cut = optimal_cuts[id];
        if (input_node_ids.contains(id) || output_node_ids.contains(id)) {
            necessary_node_ids.insert(id);
        }
        if (necessary_node_ids.contains(id)) {
            necessary_node_ids.insert(cut.begin(), cut.end());
        }
    }

    for (auto const& id : topological_order) {
        if (!necessary_node_ids.contains(id)) {
            optimal_cuts.erase(id);
            optimal_costs.erase(id);
        }
    }

    return {optimal_cuts, optimal_costs};
}

void test_k_lut_partition(size_t const max_cut_size, std::istream& input) {
    XAG xag                            = from_xaag(input);
    auto [optimal_cuts, optimal_costs] = k_lut_partition(xag, max_cut_size);
    fmt::print("optimal cuts:\n");
    for (auto const& [id, cut] : optimal_cuts) {
        auto const cut_ids = cut | std::views::transform([](auto const& id) { return id.get(); });
        fmt::print("{}: {{{}}}\n", xag.get_node(id)->to_string(), fmt::join(cut_ids, ", "));
    }
    fmt::print("optimal costs:\n");
    for (auto const& [id, cost] : optimal_costs) {
        fmt::print("{}: {}\n", id.get(), cost);
    }
}

}  // namespace qsyn::qcir
