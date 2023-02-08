/****************************************************************************
  FileName     [ cmdCommon.h ]
  PackageName  [ cmd ]
  Synopsis     [ Define classes for common commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef CMD_COMMON_H
#define CMD_COMMON_H

#include "cmdParser.h"

CmdClass(HelpCmd);
CmdClass(QuitCmd);
CmdClass(HistoryCmd);
CmdClass(DofileCmd);
CmdClass(UsageCmd);
ArgParseCmdClass(VerboseCmd);
ArgParseCmdClass(SeedCmd);
ArgParseCmdClass(ColorCmd);
#endif  // CMD_COMMON_H
