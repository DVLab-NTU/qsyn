/****************************************************************************
  FileName     [ topologyMgr.cpp ]
  PackageName  [ topology ]
  Synopsis     [ Define class QCir manager member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "topologyMgr.h"

#include <cstddef>  // for size_t
#include <iostream>

using namespace std;

DeviceTopoMgr* deviceTopoMgr = 0;
extern size_t verbose;
