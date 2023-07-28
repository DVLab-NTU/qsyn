/****************************************************************************
  FileName     [ qcirCmd.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_CMD_H
#define QCIR_CMD_H

#include "argparse.h"

extern ArgParse::ArgType<size_t>::ConstraintType const validQCirId;
extern ArgParse::ArgType<size_t>::ConstraintType const validQCirGateId;
extern ArgParse::ArgType<size_t>::ConstraintType const validQCirBitId;
extern ArgParse::ArgType<size_t>::ConstraintType const validDMode;

bool qcirMgrNotEmpty(std::string const& command);

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

#endif  // QCIR_CMD_H
