/****************************************************************************
  FileName     [ zxGraphMgr.h ]
  PackageName  [ graph ]
  Synopsis     [ Define ZXGraph manager ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_GRAPH_MGR_H
#define ZX_GRAPH_MGR_H

#include "./zxGraph.hpp"
#include "util/dataStructureManager.hpp"

using ZXGraphMgr = dvlab_utils::DataStructureManager<ZXGraph>;
bool zxGraphMgrNotEmpty(std::string const& command);  // defined in zxCmd.cpp

#endif  // ZX_GRAPH_MGR_H
