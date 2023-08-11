/****************************************************************************
  FileName     [ zxGraphMgr.h ]
  PackageName  [ zx ]
  Synopsis     [ Define ZXGraph manager ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./zxGraph.hpp"
#include "util/dataStructureManager.hpp"

using ZXGraphMgr = dvlab_utils::DataStructureManager<ZXGraph>;
bool zxGraphMgrNotEmpty(std::string const& command);  // defined in zxCmd.cpp
