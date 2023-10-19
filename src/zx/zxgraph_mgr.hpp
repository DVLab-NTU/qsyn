/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define ZXGraph manager ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./zxgraph.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::zx {

using ZXGraphMgr = dvlab::utils::DataStructureManager<ZXGraph>;
bool zxgraph_mgr_not_empty(ZXGraphMgr const&);  // defined in zxCmd.cpp

}  // namespace qsyn::zx

template <>
inline std::string dvlab::utils::data_structure_info_string(qsyn::zx::ZXGraph* t) {
    return fmt::format("{:<19} {}", t->get_filename().substr(0, 19),
                       fmt::join(t->get_procedures(), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(qsyn::zx::ZXGraph* t) {
    return t->get_filename();
}
