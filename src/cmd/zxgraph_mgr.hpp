/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define ZXGraph manager ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "util/data_structure_manager.hpp"
#include "zx/zxgraph.hpp"

namespace qsyn::zx {

using ZXGraphMgr = dvlab::utils::DataStructureManager<ZXGraph>;

}  // namespace qsyn::zx

template <>
inline std::string dvlab::utils::data_structure_info_string(qsyn::zx::ZXGraph const& t) {
    return fmt::format("{:<19} {}", t.get_filename().substr(0, 19),
                       fmt::join(t.get_procedures(), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(qsyn::zx::ZXGraph const& t) {
    return t.get_filename();
}
