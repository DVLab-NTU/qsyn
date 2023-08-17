/****************************************************************************
  FileName     [ deviceMgr.hpp ]
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
#include "util/dataStructureManager.hpp"

template <>
inline std::string dvlab::utils::dataInfoString(Device* dev) {
    return fmt::format("{:<19} #Q: {:>4}",
                       dev->getName().substr(0, 19),
                       dev->getNQubit());
}

template <>
inline std::string dvlab::utils::dataName(Device* dev) {
    return dev->getName();
}

using DeviceMgr = dvlab::utils::DataStructureManager<Device>;
extern DeviceMgr deviceMgr;

bool deviceMgrNotEmpty();
