/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ A rudimentary implementation of the Quantum-Aware Partitioning algorithm from the paper  ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/2005.00211 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <istream>
#include <map>
#include <utility>

#include "qcir/oracle/xag.hpp"
#include "qcir/qcir.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::qcir {

using LUTEntry = std::tuple<const XAG*, XAGNodeID const, XAGCut const>;

struct LUTEntryHash {
    size_t seed = 65537;
    size_t operator()(const LUTEntry& e) const;
};

struct LUTEntryEqual {
    bool operator()(const LUTEntry& lhs, const LUTEntry& rhs) const;
};

class LUT {
public:
    LUT(size_t const k);
    size_t get_k() const { return k; }
    QCir const& operator[](LUTEntry const& entry) const { return table.at(entry); }
    QCir& operator[](LUTEntry const& entry) { return table[entry]; }
    QCir const& at(LUTEntry const& entry) const { return table.at(entry); }

    std::map<XAGNodeID, QubitIdType> match_input(XAG const& xag, XAGNodeID const& node_id, XAGCut const& cut) const;

private:
    size_t k;
    std::unordered_map<LUTEntry, QCir, LUTEntryHash, LUTEntryEqual> table;

    void construct_lut_3();
};

std::set<XAGNodeID>
get_cone_node_ids(XAG& xag, XAGNodeID const& node_id, XAGCut const& cut);

std::pair<std::map<XAGNodeID, XAGCut>, std::map<XAGNodeID, size_t>> k_lut_partition(XAG& xag, const size_t max_cut_size);

void test_k_lut_partition(size_t const max_cut_size, std::istream& input);

}  // namespace qsyn::qcir
