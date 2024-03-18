/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define basic QCir functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/phase.hpp"

namespace qsyn::qcir {

using GateInfo    = std::tuple<std::string, QubitIdList, dvlab::Phase>;
using Equivalence = dvlab::utils::ordered_hashmap<std::string, std::vector<GateInfo>>;

extern dvlab::utils::ordered_hashmap<std::string, Equivalence> EQUIVALENCE_LIBRARY;

}  // namespace qsyn::qcir
