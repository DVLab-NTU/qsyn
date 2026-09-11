/****************************************************************************
  PackageName  [ qcir/gridsynth ]
  Synopsis     [ Thin bridge to cppgridsynth (no GMP headers here) ]
****************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace qsyn::qcir::gridsynth_detail {

enum class SynthGateKind { H,
                           T,
                           S,
                           X,
                           W };

struct SynthGate {
    SynthGateKind kind;
};

struct GridsynthRequest {
    std::string theta_numer;
    std::string theta_denom;
    std::string epsilon;
    std::optional<int> dps;
    int seed           = 0;
    int dloop          = 10;
    int floop          = 10;
    double dtimeout_ms = -1.0;
    double ftimeout_ms = -1.0;
    int verbose        = 0;
};

std::optional<std::vector<SynthGate>>
synthesize_rz(GridsynthRequest const& req);

}  // namespace qsyn::qcir::gridsynth_detail
