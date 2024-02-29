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
#include <cstdint>
#include <kitty/bit_operations.hpp>
#include <kitty/dynamic_truth_table.hpp>
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

size_t caclculate_radamacher_walsh_cost(kitty::dynamic_truth_table const& truth_table) {
    size_t cost        = 0;
    size_t const k     = 1 << truth_table.num_vars();
    std::vector<int> F = std::vector<int>(k, 0);
    for (auto const& i : iota(0UL, k)) {
        F[i] = kitty::get_bit(truth_table, i) ? -1 : 1;
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
            auto const truth_table = xag.calculate_truth_table(id, cut);
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

std::map<XAGNodeID, qsyn::QubitIdType> match_input_2(XAG const& xag, XAGNodeID const& node_id, XAGCut const& /* cut */) {
    auto input_to_qcir = std::map<XAGNodeID, qsyn::QubitIdType>{};
    auto node          = xag.get_node(node_id);
    if (node.fanin_inverted[1] && !node.fanin_inverted[0]) {
        input_to_qcir[node.fanins[0]] = 1;
        input_to_qcir[node.fanins[1]] = 0;
    } else {
        input_to_qcir[node.fanins[0]] = 0;
        input_to_qcir[node.fanins[1]] = 1;
    }
    return input_to_qcir;
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

void add_gates_2(
    uint8_t const i,
    std::function<void(QubitIdType)> x,
    std::function<void(QubitIdType, QubitIdType)> cx,
    std::function<void(QubitIdType, QubitIdType, QubitIdType)> ccx) {
    switch (i) {
        case 0b0000:
            break;
        case 0b0001:
            x(0), x(1), cx(0, 1), x(0), x(1);
            break;
        case 0b0010:
            x(0), ccx(0, 1, 2), x(0);
            break;
        case 0b0011:
            x(0), cx(0, 2), x(0);
            break;
        case 0b0100:
            x(1), ccx(0, 1, 2), x(1);
            break;
        case 0b0101:
            x(1), cx(1, 2), x(1);
            break;
        case 0b0110:
            cx(0, 2), cx(1, 2);
            break;
        case 0b0111:
            ccx(0, 1, 2), x(2);
            break;
        case 0b1000:
            ccx(0, 1, 2);
            break;
        case 0b1001:
            cx(0, 2), cx(1, 2), x(2);
            break;
        case 0b1010:
            cx(1, 2);
            break;
        case 0b1011:
            x(1), ccx(0, 1, 2), x(1), x(2);
            break;
        case 0b1100:
            cx(0, 2);
            break;
        case 0b1101:
            x(0), ccx(0, 1, 2), x(0), x(2);
            break;
        case 0b1110:
            ccx(0, 1, 2), x(2);
            break;
        case 0b1111:
            x(2);
            break;
        default: {
            throw std::runtime_error(fmt::format("unexpected truth table: {}", i));
        };
    }
}

void LUT::construct_lut_2() {
    for (uint64_t const i : iota(0, 1 << 4)) {
        auto tt = kitty::dynamic_truth_table(2);
        kitty::create_from_words(tt, &i, &i + 1);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto qcir = QCir(3);

        auto x   = [&qcir](auto const& target) { qcir.add_gate("x", {target}, {}, true); };
        auto cx  = [&qcir](auto const& control, auto const& target) { qcir.add_gate("cx", {control, target}, {}, true); };
        auto ccx = [&qcir](auto const& c1, auto const& c2, auto const& target) { qcir.add_gate("ccx", {c1, c2, target}, {}, true); };

        add_gates_2(i, x, cx, ccx);
    }
}

void LUT::construct_lut_3() {
    for (uint64_t const i : iota(0, 1 << 8)) {
        auto tt = kitty::dynamic_truth_table(3);
        kitty::create_from_words(tt, &i, &i + 1);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto qcir = QCir(4);

        uint8_t i_0 = i & 0b1111;
        uint8_t i_1 = i >> 4;

        auto x   = [&qcir](auto const& target) { qcir.add_gate("cx", {0, target + 1}, {}, true); };
        auto cx  = [&qcir](auto const& control, auto const& target) { qcir.add_gate("ccx", {0, control + 1, target + 1}, {}, true); };
        auto ccx = [&qcir](auto const& c1, auto const& c2, auto const& target) { qcir.add_gate("cccx", {0, c1 + 1, c2 + 1, target + 1}, {}, true); };

        // (a & b) ^ (!a & c)
        qcir.add_gate("x", {0}, {}, true);
        add_gates_2(i_0, x, cx, ccx);
        qcir.add_gate("x", {0}, {}, true);
        add_gates_2(i_1, x, cx, ccx);

        table[tt] = qcir;
    }
}

LUT::LUT(size_t const k) : k(k) {
    switch (k) {
        case 2: {
            construct_lut_2();
            break;
        }
        case 3: {
            construct_lut_2();
            construct_lut_3();
            break;
        }
        default: {
            throw std::runtime_error(fmt::format("k-LUT partitioning not implemented for k = {}", k));
        }
    }

    for (auto const& [entry, qcir] : table) {
        qcir.print_qcir();
    }
}

std::map<XAGNodeID, QubitIdType> LUT::match_input(XAG const& xag, XAGNodeID const& node_id, XAGCut const& cut) const {
    switch (cut.size()) {
        case 2: {
            return match_input_2(xag, node_id, cut);
        }
        case 3: {
            return match_input_3(xag, node_id, cut);
        }
        default: {
            throw std::runtime_error(fmt::format("k-LUT partitioning not implemented for k = {}", k));
        }
    }
}

}  // namespace qsyn::qcir
