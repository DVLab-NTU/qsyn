/****************************************************************************
  FileName     [ qcirCmd.hpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "argparse/argparse.hpp"

extern ArgParse::ArgType<size_t>::ConstraintType const validQCirId;
extern ArgParse::ArgType<size_t>::ConstraintType const validQCirGateId;
extern ArgParse::ArgType<size_t>::ConstraintType const validQCirBitId;
extern ArgParse::ArgType<size_t>::ConstraintType const validDecompositionMode;
