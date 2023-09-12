/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define qsyn common types ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

namespace dvlab {
class Phase;
}

namespace qsyn {
using QubitIdType = int;
using Phase = dvlab::Phase;
}  // namespace qsyn