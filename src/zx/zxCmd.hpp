/****************************************************************************
  FileName     [ zxCmd.h ]
  PackageName  [ graph ]
  Synopsis     [ Define zx package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_CMD_H
#define ZX_CMD_H

#include "cli/cli.hpp"

extern ArgParse::ArgType<size_t>::ConstraintType const validZXGraphId;

#endif  // ZX_CMD_H