/****************************************************************************
  FileName     [ zxCmd.h ]
  PackageName  [ graph ]
  Synopsis     [ Define basic zx package commands ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#ifndef ZX_CMD_H
#define ZX_CMD_H

#include "cmdParser.h"

CmdClass(ZXModeCmd);
CmdClass(ZXNewCmd);
CmdClass(ZXRemoveCmd);
CmdClass(ZXCHeckoutCmd);
CmdClass(ZXPrintCmd);
CmdClass(ZXCOPyCmd);
CmdClass(ZXCOMposeCmd);
CmdClass(ZXTensorCmd);
CmdClass(ZXGPrintCmd);
CmdClass(ZXGTestCmd);
CmdClass(ZXGEditCmd);
CmdClass(ZXGSimpCmd);
CmdClass(ZXGTraverseCmd);
CmdClass(ZXGTSMappingCmd);
CmdClass(ZXGReadCmd);
CmdClass(ZXGWriteCmd);
#endif // ZX_CMD_H