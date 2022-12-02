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

CmdClass(QCirCheckoutCmd);
CmdClass(QCirResetCmd);
CmdClass(QCirDeleteCmd);
CmdClass(QCirNewCmd);
CmdClass(QCirCopyCmd);
CmdClass(QCirComposeCmd);
CmdClass(QCirTensorCmd);
CmdClass(QCPrintCmd);

CmdClass(QCirReadCmd);
CmdClass(QCirPrintCmd);
CmdClass(QCirWriteCmd);
CmdClass(QCirAddGateCmd);
CmdClass(QCirAddQubitCmd);
CmdClass(QCirDeleteGateCmd);
CmdClass(QCirDeleteQubitCmd);
CmdClass(QCirAddMultipleCmd);
CmdClass(QCir2ZXCmd);
CmdClass(QCir2TSCmd);
CmdClass(QCirGatePrintCmd);

CmdClass(QCirTestCmd);

#define QC_CMD_MGR_NOT_EMPTY_OR_RETURN(str) {\
if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end()) {\
    cerr << "Error: QCir list is empty now. Please QCNEW/QCCRead/QCBAdd before " << str << ".\n";\
    return CMD_EXEC_ERROR;\
}}

#define QC_CMD_QCIR_ID_EXISTED_OR_RETURN(id) {\
if (!(qcirMgr->isID(id))) {\
    cerr << "Error: Graph " << (id) << " is not existed!!\n"; \
    return CMD_EXEC_ERROR;\
}}

#define QC_CMD_ID_VALID_OR_RETURN(option, id, str) {\
if (!myStr2Uns((option), (id))) { \
    cerr << "Error: invalid " << str << " ID!!\n"; \
    return errorOption(CMD_OPT_ILLEGAL, (option)); \
}}

#endif // QCIR_CMD_H
