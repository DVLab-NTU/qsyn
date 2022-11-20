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
CmdClass(QCirWriteCmd);
CmdClass(QCirAddGateCmd);
CmdClass(QCirAddQubitCmd);
CmdClass(QCirDeleteGateCmd);
CmdClass(QCirDeleteQubitCmd);
CmdClass(QCirTestCmd);
CmdClass(QCir2ZXCmd);
CmdClass(QCir2TSCmd);
CmdClass(QCirGatePrintCmd);

#define QC_CMD_MGR_NOT_EMPTY_OR_RETURN(str) {\
if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end()) {\
    cerr << "Error: QCir list is empty now. Please QCNEW/QCCRead/QCBAdd before " << str << ".\n";\
    return CMD_EXEC_ERROR;\
}}

#endif // QCIR_CMD_H
