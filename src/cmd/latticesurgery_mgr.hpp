/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define class LatticeSurgery manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

#include "./cli/cli.hpp"
#include "latticesurgery/latticesurgery.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::latticesurgery {

using LatticeSurgeryMgr = dvlab::utils::DataStructureManager<LatticeSurgery>;

}  // namespace qsyn::latticesurgery

template <>
inline std::string dvlab::utils::data_structure_info_string(qsyn::latticesurgery::LatticeSurgery const& t) {
    return fmt::format("{:<19} {} qubits, {} gates", 
                      t.get_filename().substr(0, 19),
                      t.get_num_qubits(),
                      t.get_num_gates());
}

template <>
inline std::string dvlab::utils::data_structure_name(qsyn::latticesurgery::LatticeSurgery const& t) {
    return t.get_filename();
} 