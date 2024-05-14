/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ A rudimentary implementation of the Quantum-Aware Partitioning algorithm from the paper  ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/2005.00211 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <spdlog/spdlog.h>

#include <istream>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/kitty.hpp>
#include <kitty/print.hpp>
#include <unordered_map>
#include <utility>

#include "qcir/oracle/xag.hpp"
#include "qcir/qcir.hpp"

namespace qsyn::qcir {

class LUT {
public:
    LUT(size_t k);
    size_t get_k() const { return _k; }
    QCir const& operator[](kitty::dynamic_truth_table const& entry) const {
        return _table.at(entry);
    }
    QCir& operator[](kitty::dynamic_truth_table const& entry) { return _table[entry]; }
    void for_each(std::function<void(kitty::dynamic_truth_table const&, QCir const&)> const& fn) const {
        for (auto const& [entry, qcir] : _table) {
            fn(entry, qcir);
        }
    }

private:
    size_t _k;
    std::unordered_map<kitty::dynamic_truth_table, QCir, kitty::hash<kitty::dynamic_truth_table>> _table;

    void _construct_lut_1();
    void _construct_lut_2();
    void _construct_lut_3();
};

std::pair<std::map<XAGNodeID, XAGCut>, std::map<XAGNodeID, size_t>> k_lut_partition(XAG& xag, size_t max_cut_size);

void test_k_lut_partition(size_t max_cut_size, std::istream& input);

}  // namespace qsyn::qcir
