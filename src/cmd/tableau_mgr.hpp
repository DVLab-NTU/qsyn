/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define class QCir manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

#include "cli/cli.hpp"
#include "tableau/tableau.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::tableau {

using TableauMgr = dvlab::utils::DataStructureManager<Tableau>;

}  // namespace qsyn::tableau

template <>
inline std::string dvlab::utils::data_structure_info_string(dvlab::utils::DataStructureManager<qsyn::tableau::Tableau> const& mgr, size_t id) {
    return fmt::format("{:<19} {}", mgr.get_filename(id).substr(0, 19),
                       fmt::join(mgr.get_procedures(id), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(dvlab::utils::DataStructureManager<qsyn::tableau::Tableau> const& mgr, size_t id) {
    return mgr.get_filename(id);
}
