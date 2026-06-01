#include <spdlog/spdlog.h>

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "qcir/gridsynth/gridsynth_pass.hpp"
#include "qcir/qcir.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::qcir {

Command qcir_gridsynth_cmd(QCirMgr& qcir_mgr) {
    return {"gridsynth",
            [&](ArgumentParser& parser) {
                parser.description(
                    "Decompose RZ gates into Clifford+T sequences (GridSynth)");
                parser.add_argument<std::string>("-e", "--epsilon")
                    .required(true)
                    .help("maximum synthesis error (e.g. 1e-10)");
                parser.add_argument<int>("--dps")
                    .help("MPFR decimal places (default: derived from epsilon)");
                parser.add_argument<int>("--seed")
                    .default_value(0)
                    .help("random seed for GridSynth");
                parser.add_argument<int>("--dloop")
                    .default_value(10)
                    .help("diophantine loop limit");
                parser.add_argument<int>("--floop")
                    .default_value(10)
                    .help("factor loop limit");
                parser.add_argument<double>("--dtimeout")
                    .default_value(-1.0)
                    .help("diophantine timeout in ms (<0: none)");
                parser.add_argument<double>("--ftimeout")
                    .default_value(-1.0)
                    .help("factor timeout in ms (<0: none)");
                parser.add_argument<int>("--verbose")
                    .default_value(0)
                    .help("GridSynth verbosity");
            },
            [&](ArgumentParser const& parser) -> CmdExecResult {
                if (!qcir_mgr.get()) {
                    spdlog::error("No circuit is loaded!!");
                    return CmdExecResult::error;
                }

                GridsynthOptions opts;
                opts.epsilon     = parser.get<std::string>("--epsilon");
                opts.seed        = parser.get<int>("--seed");
                opts.dloop       = parser.get<int>("--dloop");
                opts.floop       = parser.get<int>("--floop");
                opts.dtimeout_ms = parser.get<double>("--dtimeout");
                opts.ftimeout_ms = parser.get<double>("--ftimeout");
                opts.verbose     = parser.get<int>("--verbose");
                if (parser.parsed("--dps")) {
                    opts.dps = parser.get<int>("--dps");
                }

                auto result = gridsynth_decompose(*qcir_mgr.get(), opts);
                if (!result.has_value()) {
                    return CmdExecResult::error;
                }
                *qcir_mgr.get() = std::move(*result);
                return CmdExecResult::done;
            }};
}

}  // namespace qsyn::qcir
