/****************************************************************************
  FileName     [ phase_argparse.h ]
  PackageName  [ util ]
  Synopsis     [ Definition of the Phase class and pertinent classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <string>

#include "util/phase.hpp"

namespace ArgParse {

inline std::string typeString(Phase const&) { return "Phase"; }
inline bool parseFromString(Phase& phase, std::string const& token) {
    return Phase::myStr2Phase(token, phase);
}

}  // namespace ArgParse

#include "argparse/argparse.hpp"

