/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Define pp package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <string>

#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "argparse/argument.hpp"
#include "cli/cli.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_cmd.hpp"
#include "qcir/qcir_mgr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/util.hpp"


using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;
using qsyn::qcir::QCirMgr;

namespace qsyn::pp{
dvlab::Command qcir_phase_polynomial_cmd(QCirMgr& qcir_mgr) {
    return {"phase_polynomial",
            [](ArgumentParser& parser) {
                parser.description("perform phase polynomial optimizer");

                // a=-1: unlimited ancilla
                parser.add_argument<int>("-a", "--ancilla")
                    .nargs(NArgsOption::optional)
                    .default_value(-1)
                    .help("the number of ancilla to be added (default=-1)");

                parser.add_argument<std::string>("-resyn", "--resynthesis")
                    .constraint(choices_allow_prefix({"C", "G"}))
                    .default_value("G")
                    .help("the resynethesis method chosen(C/G). If not specified, the default method i\ns gaussian elimination(G).");
            },
            [&](ArgumentParser const& parser) {
                if (qcir_mgr.empty()) {
                    spdlog::info("QCir list is empty now. Create a new one.");
                    qcir_mgr.add(qcir_mgr.get_next_id());
                }

                // TODO
                fmt::println("phase-polynomial {}", parser.get<std::string>("--resynthesis"));

                return CmdExecResult::done;
            }};
}

Command pp_cmd(QCirMgr& qcir_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(qcir_mgr);
    cmd.add_subcommand(qcir_phase_polynomial_cmd(qcir_mgr));
    return cmd;
}

bool add_pp_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr) {
    if (!cli.add_command(pp_cmd(qcir_mgr))) {
        spdlog::error("Registering \"pp\" commands fails... exiting");
        return false;
    }
    return true;
}


} // namespace qsyn::pp


