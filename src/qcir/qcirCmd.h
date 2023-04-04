/****************************************************************************
  FileName     [ qcirCmd.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_CMD_H
#define QCIR_CMD_H

#include "cmdParser.h"  // for CmdClass, CmdExec, CmdExecStatus::CMD_EXEC_ERROR
#include "qcirMgr.h"    // for QCirMgr

CmdClass(QCirAddGateCmd);

#define QC_CMD_MGR_NOT_EMPTY_OR_RETURN(str)                                                               \
    {                                                                                                     \
        if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end()) {                                 \
            cerr << "Error: QCir list is empty now. Please QCNEW/QCCRead/QCBAdd before " << str << ".\n"; \
            return CMD_EXEC_ERROR;                                                                        \
        }                                                                                                 \
    }

#define QC_CMD_QCIR_ID_EXISTS_OR_RETURN(id)                          \
    {                                                                \
        if (!(qcirMgr->isID(id))) {                                  \
            cerr << "Error: QCir " << (id) << " does not exist!!\n"; \
            return CMD_EXEC_ERROR;                                   \
        }                                                            \
    }

#define QC_CMD_ID_VALID_OR_RETURN(option, id, str)         \
    {                                                      \
        if (!myStr2Uns((option), (id))) {                  \
            cerr << "Error: invalid " << str << " ID!!\n"; \
            return errorOption(CMD_OPT_ILLEGAL, (option)); \
        }                                                  \
    }

#define QC_CMD_QCIR_ID_NOT_EXIST_OR_RETURN(id)                                                                   \
    {                                                                                                            \
        if (qcirMgr->isID(id)) {                                                                                 \
            cerr << "Error: QCir " << (id) << " already exists!! Add `-Replace` if you want to overwrite it.\n"; \
            return CMD_EXEC_ERROR;                                                                               \
        }                                                                                                        \
    }

#endif  // QCIR_CMD_H
