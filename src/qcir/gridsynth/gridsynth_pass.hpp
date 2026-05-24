/****************************************************************************
  PackageName  [ qcir/gridsynth ]
  Synopsis     [ RZ → Clifford+T decomposition via cppgridsynth ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <optional>
#include <string>

#include "qcir/qcir.hpp"

namespace qsyn::qcir {

struct GridsynthOptions {
    std::string epsilon;
    std::optional<int> dps;
    int seed = 0;
    int dloop = 10;
    int floop = 10;
    double dtimeout_ms = -1.0;
    double ftimeout_ms = -1.0;
    int verbose = 0;
};

// Replace each single-qubit RZGate with a Clifford+T sequence (GridSynth).
// Other gates are copied unchanged.  Controlled RZ (crz) is left as-is in v1.
std::optional<QCir> gridsynth_decompose(QCir const& qcir, GridsynthOptions const& opts);

}  // namespace qsyn::qcir
