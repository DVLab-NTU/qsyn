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

namespace qsyn::qcir {

std::pair<std::map<XAGNodeID, XAGCut>, std::map<XAGNodeID, size_t>> k_lut_partition();

void test_k_lut_partition(size_t const max_cut_size, std::istream& input);

}  // namespace qsyn::qcir