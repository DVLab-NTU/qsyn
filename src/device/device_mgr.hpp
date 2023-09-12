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

#include "device/device.hpp"
#include "util/data_structure_manager.hpp"

template <>
inline std::string dvlab::utils::data_structure_info_string(Device* t) {
    return fmt::format("{:<19} #Q: {:>4}",
                       t->get_name().substr(0, 19),  // NOLINT(cppcoreguidelines-avoid-magic-numbers)
                       t->get_num_qubits());
}

template <>
inline std::string dvlab::utils::data_structure_name(Device* t) {
    return t->get_name();
}

using DeviceMgr = dvlab::utils::DataStructureManager<Device>;
extern DeviceMgr DEVICE_MGR;

bool device_mgr_not_empty();
