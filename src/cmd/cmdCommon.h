/****************************************************************************
  FileName     [ cmdCommon.h ]
  PackageName  [ cmd ]
  Synopsis     [ Define classes for common commands ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2007-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef CMD_COMMON_H
#define CMD_COMMON_H

#include "cmdParser.h"

CmdClass(HelpCmd);
CmdClass(QuitCmd);
CmdClass(HistoryCmd);
CmdClass(DofileCmd);
CmdClass(UsageCmd);

#endif // CMD_COMMON_H
