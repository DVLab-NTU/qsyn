/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define QCir translation functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir.hpp"
#include "qcir/operation.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::qcir {

using Equivalence = dvlab::utils::ordered_hashmap<Operation, std::vector<QCirGate>, OperationHash>;

extern dvlab::utils::ordered_hashmap<std::string, Equivalence> EQUIVALENCE_LIBRARY;

std::optional<QCir> translate(QCir const& qcir, std::string const& gate_set);

}  // namespace qsyn::qcir
