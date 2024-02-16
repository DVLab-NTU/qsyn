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
#include "./stabilizer_tableau.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::experimental {

using TableauMgr = dvlab::utils::DataStructureManager<Tableau>;

}  // namespace qsyn::experimental

template <>
inline std::string dvlab::utils::data_structure_info_string(qsyn::experimental::Tableau const& t) {
    return fmt::format("{:<19} {}", t.get_filename().substr(0, 19),
                       fmt::join(t.get_procedures(), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(qsyn::experimental::Tableau const& t) {
    return t.get_filename();
}
