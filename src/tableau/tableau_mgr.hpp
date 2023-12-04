/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define class QCir manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

#include "./cli/cli.hpp"
#include "./tableau.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::experimental {

using TableauMgr = dvlab::utils::DataStructureManager<StabilizerTableau>;

}  // namespace qsyn::experimental

template <>
inline std::string dvlab::utils::data_structure_info_string(qsyn::experimental::StabilizerTableau const& t) {
    return fmt::format("#qubit: {}", t.n_qubits());
}

template <>
inline std::string dvlab::utils::data_structure_name(qsyn::experimental::StabilizerTableau const& /*t*/) {
    return "tableau";
}
