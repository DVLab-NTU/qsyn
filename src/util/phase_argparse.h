/****************************************************************************
  FileName     [ phase_argparse.h ]
  PackageName  [ util ]
  Synopsis     [ Definition of the Phase class and pertinent classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_UTIL_PHASE_ARGPARSE_H
#define QSYN_UTIL_PHASE_ARGPARSE_H

#include <string>

#include "phase.h"

namespace ArgParse {

inline std::string typeString(Phase const&) { return "Phase"; }
inline bool parseFromString(Phase& phase, std::string const& token) {
    return Phase::myStr2Phase(token, phase);
}

}  // namespace ArgParse

#include "argparse.h"

#endif  // QSYN_UTIL_PHASE_ARGPARSE_H