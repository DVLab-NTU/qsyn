/****************************************************************************
  FileName     [ extractorCmd.h ]
  PackageName  [ n2 ]
  Synopsis     [ Define basic extractor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef EXTRACTOR_CMD_H
#define EXTRACTOR_CMD_H

#include "cmdParser.h"

CmdClass(ExtractCmd);
CmdClass(ExtractStepCmd);
CmdClass(ExtractPrintCmd);
#endif  // EXTRACTOR_CMD_H