/****************************************************************************
  FileName     [ qcirCmd.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_CMD_H
#define QCIR_CMD_H

#include "argparse/argparse.h"

extern ArgParse::ArgType<size_t>::ConstraintType const validQCirId;
extern ArgParse::ArgType<size_t>::ConstraintType const validQCirGateId;
extern ArgParse::ArgType<size_t>::ConstraintType const validQCirBitId;
extern ArgParse::ArgType<size_t>::ConstraintType const validDMode;

#endif  // QCIR_CMD_H
