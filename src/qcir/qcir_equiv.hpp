/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define QCir equivalence checking functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/qcir.hpp"

namespace qsyn {
namespace qcir {

bool is_equivalent(QCir const& qcir1, QCir const& qcir2);

}
}  // namespace qsyn
