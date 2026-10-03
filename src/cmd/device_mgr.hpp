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

namespace qsyn::device {

using DeviceMgr = dvlab::utils::DataStructureManager<Device>;

}  // namespace qsyn::device

template <>
inline std::string dvlab::utils::data_structure_info_string(dvlab::utils::DataStructureManager<qsyn::device::Device> const& mgr, size_t id) {
    auto* device = mgr.find_by_id(id);
    if (!device) return {};
    return fmt::format("{:<19} #Q: {:>4}",
                       device->get_name().substr(0, 19),  // NOLINT(cppcoreguidelines-avoid-magic-numbers)
                       device->get_num_qubits());
}

template <>
inline std::string dvlab::utils::data_structure_name(dvlab::utils::DataStructureManager<qsyn::device::Device> const& mgr, size_t id) {
    auto* device = mgr.find_by_id(id);
    if (!device) return {};
    return device->get_name();
}
