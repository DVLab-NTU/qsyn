/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define class Device manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "cli/cli.hpp"
#include "device/device.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::device {

using DeviceMgr = dvlab::utils::DataStructureManager<Device>;

bool device_mgr_not_empty(DeviceMgr const& device_mgr);

}  // namespace qsyn::device

template <>
inline std::string dvlab::utils::data_structure_info_string(qsyn::device::Device* t) {
    return fmt::format("{:<19} #Q: {:>4}",
                       t->get_name().substr(0, 19),  // NOLINT(cppcoreguidelines-avoid-magic-numbers)
                       t->get_num_qubits());
}

template <>
inline std::string dvlab::utils::data_structure_name(qsyn::device::Device* t) {
    return t->get_name();
}
