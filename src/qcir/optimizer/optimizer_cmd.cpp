/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <string>

#include "../qcir.hpp"
#include "../qcir_cmd.hpp"
#include "../qcir_mgr.hpp"
#include "./optimizer.hpp"
#include "cli/cli.hpp"
#include "util/util.hpp"

using namespace std;
using namespace argparse;
extern QCirMgr QCIR_MGR;
extern bool stop_requested();

Command qcir_optimize_cmd();

bool add_qcir_optimize_cmds() {
    if (!(CLI.add_command(qcir_optimize_cmd()))) {
        LOGGER.fatal("Registering \"optimize\" commands fails... exiting");
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    Optimize
//----------------------------------------------------------------------
Command qcir_optimize_cmd() {
    return {"qccoptimize",
            [](ArgumentParser& parser) {
                parser.description("optimize QCir");

                parser.add_argument<bool>("-physical")
                    .default_value(false)
                    .action(store_true)
                    .help("optimize physical circuit, i.e preserve the swap path");
                parser.add_argument<bool>("-copy")
                    .default_value(false)
                    .action(store_true)
                    .help("copy a circuit to perform optimization");
                parser.add_argument<bool>("-statistics")
                    .default_value(false)
                    .action(store_true)
                    .help("count the number of rules operated in optimizer.");
                parser.add_argument<bool>("-trivial")
                    .default_value(false)
                    .action(store_true)
                    .help("Use the trivial optimization.");
            },
            [](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty()) return CmdExecResult::error;
                Optimizer optimizer;
                std::optional<QCir> result;
                std::string procedure_str{};
                if (parser.get<bool>("-trivial")) {
                    result = optimizer.trivial_optimization(*QCIR_MGR.get());
                    procedure_str = "Trivial Optimize";
                } else {
                    result = optimizer.basic_optimization(*QCIR_MGR.get(), {.doSwap = !parser.get<bool>("-physical"),
                                                                            .separateCorrection = false,
                                                                            .maxIter = 1000,
                                                                            .printStatistics = parser.get<bool>("-statistics")});
                    procedure_str = "Optimize";
                }
                if (result == std::nullopt) {
                    LOGGER.error("Fail to optimize circuit.");
                    return CmdExecResult::error;
                }

                if (parser.get<bool>("-copy")) {
                    QCIR_MGR.add(QCIR_MGR.get_next_id(), std::make_unique<QCir>(std::move(*result)));
                } else {
                    QCIR_MGR.set(std::make_unique<QCir>(std::move(*result)));
                }

                if (stop_requested()) {
                    procedure_str += "[INT]";
                }

                QCIR_MGR.get()->add_procedure(procedure_str);

                return CmdExecResult::done;
            }};
}
