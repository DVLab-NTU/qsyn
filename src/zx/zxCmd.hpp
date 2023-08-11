/****************************************************************************
  FileName     [ zxCmd.h ]
  PackageName  [ zx ]
  Synopsis     [ Define zx package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"

extern ArgParse::ArgType<size_t>::ConstraintType const validZXGraphId;
