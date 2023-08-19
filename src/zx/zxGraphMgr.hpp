/****************************************************************************
  FileName     [ zxGraphMgr.hpp ]
  PackageName  [ zx ]
  Synopsis     [ Define ZXGraph manager ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./zxGraph.hpp"
#include "util/dataStructureManager.hpp"

template <>
inline std::string dvlab::utils::dataInfoString(ZXGraph* zx) {
    return fmt::format("{:<19} {}", zx->getFileName().substr(0, 19),
                       fmt::join(zx->getProcedures(), " ➔ "));
}

template <>
inline std::string dvlab::utils::dataName(ZXGraph* zx) {
    return zx->getFileName();
}

using ZXGraphMgr = dvlab::utils::DataStructureManager<ZXGraph>;
bool zxGraphMgrNotEmpty();  // defined in zxCmd.cpp
