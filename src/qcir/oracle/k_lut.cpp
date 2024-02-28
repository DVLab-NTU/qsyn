/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ A rudimentary implementation of the Quantum-Aware Partitioning algorithm from the paper  ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/2005.00211 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./k_lut.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <map>
#include <numeric>
#include <ranges>
#include <set>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>
#include <tl/zip.hpp>

#include "qcir/oracle/xag.hpp"
#include "qsyn/qsyn_type.hpp"

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
        node_id_to_cuts[id] = {{id}};
        auto const& node    = xag.get_node(id);
        if (!node.is_gate()) {
            continue;
        }
        auto fanins = node.fanins;
        for (auto const& cuts0 : node_id_to_cuts[fanins[0]]) {
            for (auto const& cuts1 : node_id_to_cuts[fanins[1]]) {
                auto cuts = cuts0;
                cuts.insert(cuts1.begin(), cuts1.end());

                if (cuts.size() <= max_cut_size) {
                    if (!std::any_of(node_id_to_cuts[id].begin(), node_id_to_cuts[id].end(), [&cuts](auto const& c) { return is_subset_of(c, cuts); })) {
                        node_id_to_cuts[id].push_back(cuts);
                    }
                }
            }
        }
    }

    std::vector<XAGNodeID> to_remove;
    for (auto& [id, cuts] : node_id_to_cuts) {
        std::erase(cuts, std::set<XAGNodeID>{id});
        if (cuts.empty()) {
            to_remove.push_back(id);
        }
    }
    for (auto const& id : to_remove) {
        node_id_to_cuts.erase(id);
    }

    return node_id_to_cuts;
}

std::vector<bool> calculate_truth_table(XAG& xag, XAGNodeID const& node_id, XAGCut const& cut) {
    auto node_ids_in_cone = xag.get_cone_node_ids(node_id, cut) | std::views::reverse | tl::to<std::vector>();
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

            auto const& node = xag.get_node(id);
            if (!node.is_gate()) {
                continue;
            }

            std::vector<bool> inputs;
            for (auto const& [fanin_id, inverted] : zip(node.fanins, node.fanin_inverted)) {
                inputs.push_back(inverted ^ intermediate_results[fanin_id.get()]);
            }

            bool result{};
            if (node.is_xor()) {
                result = false;
                for (auto const& input : inputs) {
                    result ^= input;
                }
            } else if (node.is_and()) {
                result = true;
                for (auto const& input : inputs) {
                    result &= input;
                }
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
            fmt::print("{{{}}}: {}\n",
                       fmt::join(cut | std::views::transform([](auto const& id) { return id.get(); }), ", "),
                       fmt::join(truth_table | std::views::transform([](bool b) { return b ? "1" : "0"; }), ", "));
            costs[id].emplace_back(caclculate_radamacher_walsh_cost(truth_table));
        }
    }

    for (auto const& [id, cuts] : all_cuts) {
        for (auto const& [i, cut] : enumerate(cuts)) {
            auto const& cone_node_ids = xag.get_cone_node_ids(id, cut);
            bool no_and               = true;
            size_t xor_count          = 0;
            for (auto const& cone_node_id : cone_node_ids) {
                if (xag.get_node(cone_node_id).is_and()) {
                    no_and = false;
                    break;
                } else if (xag.get_node(cone_node_id).is_xor()) {
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

// input: 0, 1, 2
// output: 3
QCir build_qcir_3(
    XAGNodeType const& top_type,
    std::pair<bool, bool> const& top_inverted,
    XAGNodeType const& bottom_type,
    std::pair<bool, bool> const& bottom_inverted) {
    auto qcir                                   = QCir(4);
    auto [top_inverted_1, top_inverted_2]       = top_inverted;
    auto [bottom_inverted_1, bottom_inverted_2] = bottom_inverted;

    if (bottom_inverted_1) {
        qcir.add_gate("x", {0}, {}, true);
    }
    if (bottom_inverted_2) {
        qcir.add_gate("x", {1}, {}, true);
    }
    if (top_inverted_2) {
        qcir.add_gate("x", {2}, {}, true);
    }

    if (top_type == XAGNodeType::XOR && bottom_type == XAGNodeType::XOR) {
        qcir.add_gate("cx", {0, 3}, {}, true);
        qcir.add_gate("cx", {1, 3}, {}, true);
        qcir.add_gate("cx", {2, 3}, {}, true);
        if (top_inverted_1) {
            qcir.add_gate("x", {3}, {}, true);
        }
    } else if (top_type == XAGNodeType::XOR && bottom_type == XAGNodeType::AND) {
        qcir.add_gate("ccx", {0, 1, 3}, {}, true);
        qcir.add_gate("cx", {2, 3}, {}, true);
        if (top_inverted_1) {
            qcir.add_gate("x", {3}, {}, true);
        }
    } else if (top_type == XAGNodeType::AND && bottom_type == XAGNodeType::XOR) {
        qcir.add_gate("cx", {0, 1}, {}, true);
        if (top_inverted_1) {
            qcir.add_gate("x", {3}, {}, true);
        }
        qcir.add_gate("ccx", {1, 2, 3}, {}, true);
        if (top_inverted_1) {
            qcir.add_gate("x", {3}, {}, true);
        }
        qcir.add_gate("cx", {0, 1}, {}, true);
    } else if (top_type == XAGNodeType::AND && bottom_type == XAGNodeType::AND) {
        // from qiskit
        //
        //      ┌────────┐
        // q_0: ┤ P(π/8) ├────■──────────────────■────────────────────■──────────────────────────────■───────────────────────────────────────────────────■─────────────────────────────────────────────────────────────■───────
        //      ├────────┤  ┌─┴─┐   ┌─────────┐┌─┴─┐                  │                              │                                                   │                                                             │
        // q_1: ┤ P(π/8) ├──┤ X ├───┤ P(-π/8) ├┤ X ├──■───────────────┼──────────────■───────────────┼────────────────────■──────────────────────────────┼──────────────────────────────■──────────────────────────────┼───────
        //      ├────────┤  └───┘   └─────────┘└───┘┌─┴─┐┌─────────┐┌─┴─┐┌────────┐┌─┴─┐┌─────────┐┌─┴─┐                  │                              │                              │                              │
        // q_2: ┤ P(π/8) ├──────────────────────────┤ X ├┤ P(-π/8) ├┤ X ├┤ P(π/8) ├┤ X ├┤ P(-π/8) ├┤ X ├──■───────────────┼──────────────■───────────────┼──────────────■───────────────┼──────────────■───────────────┼───────
        //      └─┬───┬──┘┌────────┐                └───┘└─────────┘└───┘└────────┘└───┘└─────────┘└───┘┌─┴─┐┌─────────┐┌─┴─┐┌────────┐┌─┴─┐┌─────────┐┌─┴─┐┌────────┐┌─┴─┐┌─────────┐┌─┴─┐┌────────┐┌─┴─┐┌─────────┐┌─┴─┐┌───┐
        // q_3: ──┤ H ├───┤ P(π/8) ├────────────────────────────────────────────────────────────────────┤ X ├┤ P(-π/8) ├┤ X ├┤ P(π/8) ├┤ X ├┤ P(-π/8) ├┤ X ├┤ P(π/8) ├┤ X ├┤ P(-π/8) ├┤ X ├┤ P(π/8) ├┤ X ├┤ P(-π/8) ├┤ X ├┤ H ├
        //        └───┘   └────────┘                                                                    └───┘└─────────┘└───┘└────────┘└───┘└─────────┘└───┘└────────┘└───┘└─────────┘└───┘└────────┘└───┘└─────────┘└───┘└───┘
        qcir.add_gate("t", {0}, {}, true);
        qcir.add_gate("t", {1}, {}, true);
        qcir.add_gate("t", {2}, {}, true);
        qcir.add_gate("h", {3}, {}, true);

        qcir.add_gate("cx", {0, 1}, {}, true);
        qcir.add_gate("t", {3}, {}, true);

        qcir.add_gate("tdg", {1}, {}, true);
        qcir.add_gate("cx", {0, 1}, {}, true);

        // ================================
        qcir.add_gate("cx", {1, 2}, {}, true);
        qcir.add_gate("tdg", {2}, {}, true);
        qcir.add_gate("cx", {0, 2}, {}, true);

        qcir.add_gate("t", {2}, {}, true);

        qcir.add_gate("cx", {1, 2}, {}, true);
        qcir.add_gate("tdg", {2}, {}, true);
        qcir.add_gate("cx", {0, 2}, {}, true);

        // ================================
        qcir.add_gate("cx", {2, 3}, {}, true);
        qcir.add_gate("tdg", {3}, {}, true);
        qcir.add_gate("cx", {1, 3}, {}, true);
        qcir.add_gate("t", {3}, {}, true);
        qcir.add_gate("cx", {2, 3}, {}, true);
        qcir.add_gate("tdg", {3}, {}, true);
        qcir.add_gate("cx", {0, 3}, {}, true);

        qcir.add_gate("t", {3}, {}, true);

        qcir.add_gate("cx", {2, 3}, {}, true);
        qcir.add_gate("tdg", {3}, {}, true);
        qcir.add_gate("cx", {1, 3}, {}, true);
        qcir.add_gate("t", {3}, {}, true);
        qcir.add_gate("cx", {2, 3}, {}, true);
        qcir.add_gate("tdg", {3}, {}, true);
        qcir.add_gate("cx", {0, 3}, {}, true);

        // ================================
        qcir.add_gate("h", {3}, {}, true);

        // !(a & b) & c = (a & b & c) ^ c
        if (top_inverted_1) {
            qcir.add_gate("cx", {2, 3}, {}, true);
        }
    }

    if (bottom_inverted_1) {
        qcir.add_gate("x", {0}, {}, true);
    }
    if (bottom_inverted_2) {
        qcir.add_gate("x", {1}, {}, true);
    }
    if (top_inverted_2) {
        qcir.add_gate("x", {2}, {}, true);
    }

    return qcir;
}

// (4)
//  ├───┐
// (3)  └───┐
//  ├───┐   │
// (0) (1) (2)
std::map<XAGNodeID, qsyn::QubitIdType> match_input_3(XAG const& xag, XAGNodeID const& node_id, XAGCut const& cut) {
    auto input_to_qcir = std::map<XAGNodeID, qsyn::QubitIdType>{};

    auto top_node    = xag.get_node(node_id);
    auto top_fanin_0 = xag.get_node(top_node.fanins[0]);
    auto top_fanin_1 = xag.get_node(top_node.fanins[1]);

    input_to_qcir[top_node.get_id()] = 3;

    XAGNode bottom_node{};
    if (cut.contains(top_fanin_0.get_id())) {
        bottom_node                         = top_fanin_1;
        input_to_qcir[top_fanin_0.get_id()] = 2;
    } else {
        bottom_node                         = top_fanin_0;
        input_to_qcir[top_fanin_1.get_id()] = 2;
    }

    if (!bottom_node.fanin_inverted[0] && bottom_node.fanin_inverted[1]) {
        input_to_qcir[bottom_node.fanins[0]] = 1;
        input_to_qcir[bottom_node.fanins[1]] = 0;
    } else {
        input_to_qcir[bottom_node.fanins[0]] = 0;
        input_to_qcir[bottom_node.fanins[1]] = 1;
    }

    return input_to_qcir;
}

}  // namespace

namespace qsyn::qcir {

std::pair<std::map<XAGNodeID, XAGCut>, std::map<XAGNodeID, size_t>> k_lut_partition(XAG& xag, const size_t max_cut_size) {
    auto id_to_cuts  = enumerate_cuts(xag, max_cut_size);
    auto id_to_costs = calculate_cut_costs(xag, id_to_cuts);

    std::map<XAGNodeID, XAGCut> optimal_cuts;
    std::map<XAGNodeID, size_t> optimal_costs;
    auto topological_order = xag.calculate_topological_order();
    for (auto const id : topological_order) {
        if (xag.get_node(id).is_input()) {
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
        fmt::print("{}: {{{}}}\n", xag.get_node(id).to_string(), fmt::join(cut_ids, ", "));
    }
    fmt::print("optimal costs:\n");
    for (auto const& [id, cost] : optimal_costs) {
        fmt::print("{}: {}\n", id.get(), cost);
    }
}

size_t LUTEntryHash::operator()(const LUTEntry& entry) const {
    auto const& [xag, node_id, cut] = entry;
    auto node                       = xag->get_node(node_id);
    auto node_type_hash             = std::hash<XAGNodeType>{};
    auto hash_node                  = [&node_type_hash, &xag = xag](XAGNodeID const& node_id) {
        return node_type_hash(xag->get_node(node_id).get_type());
    };

    size_t hash = hash_node(node_id);

    if (cut.contains(node_id)) {
        return hash;
    }

    for (auto const& [id, inverted] : zip(node.fanins, node.fanin_inverted)) {
        if (inverted) {
            hash ^= (~(operator()({xag, id, cut}) << 1)) * seed;
        } else {
            hash ^= operator()({xag, id, cut}) * seed;
        }
    }

    return hash;
}

// only use when the hash is equal
// in which case the LUTEntry should be equal
bool LUTEntryEqual::operator()(const LUTEntry& /*lhs*/, const LUTEntry& /*rhs*/) const {
    return true;
}

// (4)
//  ├───┐
// (3)  └───┐
//  ├───┐   │
// (0) (1) (2)
void LUT::construct_lut_3() {
    auto input_nodes = std::vector<XAGNode>{
        XAGNode{XAGNodeID{0}, {}, {}, XAGNodeType::INPUT},
        XAGNode{XAGNodeID{1}, {}, {}, XAGNodeType::INPUT},
        XAGNode{XAGNodeID{2}, {}, {}, XAGNodeType::INPUT},
    };

    for (size_t const& top_i : iota(0, 8)) {
        bool top_inverted_1 = (top_i & 1) == 1;
        bool top_inverted_2 = (top_i & 2) == 2;
        auto top_type       = top_i & 4 ? XAGNodeType::XOR : XAGNodeType::AND;

        auto top_node = XAGNode{XAGNodeID{4},
                                {XAGNodeID{3}, XAGNodeID{2}},
                                {top_inverted_1, top_inverted_2},
                                top_type};

        for (size_t const& bottom_i : iota(0, 8)) {
            bool bottom_inverted_1 = (bottom_i & 1) == 1;
            bool bottom_inverted_2 = (bottom_i & 2) == 2;
            // these two are equivalent
            if (bottom_inverted_1 != bottom_inverted_2) {
                bottom_inverted_1 = true;
                bottom_inverted_2 = false;
            }
            auto bottom_type = bottom_i & 4 ? XAGNodeType::XOR : XAGNodeType::AND;

            auto bottom_node = XAGNode{
                XAGNodeID{3},
                {XAGNodeID{0}, XAGNodeID{1}},
                {bottom_inverted_1, bottom_inverted_2},
                bottom_type};

            auto xag = XAG(
                {
                    input_nodes[0],
                    input_nodes[1],
                    input_nodes[2],
                    bottom_node,
                    top_node,
                },
                {XAGNodeID(0), XAGNodeID(1), XAGNodeID(2)},
                {top_node.get_id()}, {false});

            auto qcir = build_qcir_3(top_type,
                                     {top_inverted_1, top_inverted_2},
                                     bottom_type,
                                     {bottom_inverted_1, bottom_inverted_2});

            table[{
                &xag,
                top_node.get_id(),
                {XAGNodeID(0),
                 XAGNodeID(1),
                 XAGNodeID(2)}}] = qcir;
        };
    }
}

LUT::LUT(size_t const k) : k(k) {
    switch (k) {
        case 3: {
            construct_lut_3();
            break;
        }
        default: {
            throw std::runtime_error(fmt::format("k-LUT partitioning not implemented for k = {}", k));
        }
    }
}

std::map<XAGNodeID, QubitIdType> LUT::match_input(XAG const& xag, XAGNodeID const& node_id, XAGCut const& cut) const {
    switch (k) {
        case 3: {
            return match_input_3(xag, node_id, cut);
        }
        default: {
            throw std::runtime_error(fmt::format("k-LUT partitioning not implemented for k = {}", k));
        }
    }
}

}  // namespace qsyn::qcir
