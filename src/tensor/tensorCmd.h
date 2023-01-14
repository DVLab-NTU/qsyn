/****************************************************************************
  FileName     [ tensorCmd.h ]
  PackageName  [ tensor ]
  Synopsis     [ Define tensor commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef TENSOR_CMD_H
#define TENSOR_CMD_H

#include "cmdParser.h"

CmdClass(TSResetCmd);
CmdClass(TSPrintCmd);
CmdClass(TSEquivalenceCmd);
CmdClass(TSAdjointCmd);

#endif  // TENSOR_CMD_H