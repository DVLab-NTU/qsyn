/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define qsyn common types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <limits>
#include <vector>

namespace dvlab {
class Phase;
}

namespace qsyn {
using QubitIdType           = std::size_t;
constexpr auto max_qubit_id = std::numeric_limits<QubitIdType>::max();
constexpr auto min_qubit_id = std::numeric_limits<QubitIdType>::min();
using QubitIdList           = std::vector<QubitIdType>;
using Phase                 = dvlab::Phase;
}  // namespace qsyn
