/****************************************************************************
  FileName     [ qcirCmd.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define basic qcir package commands ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_CMD_H
#define QCIR_CMD_H

#include "cmdParser.h"

CmdClass(QCirReadCmd);
CmdClass(QCirPrintCmd);
CmdClass(QCirAddGateCmd);
CmdClass(QCirAddQubitCmd);
CmdClass(QCirDeleteGateCmd);
CmdClass(QCirDeleteQubitCmd);
CmdClass(QCirTestCmd);
CmdClass(QCirZXMappingCmd);
CmdClass(QCirTSMappingCmd);
CmdClass(QCirGatePrintCmd);

#endif // QCIR_CMD_H
