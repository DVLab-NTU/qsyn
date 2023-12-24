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
#include "pp.hpp"
#include "pp_partition.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_cmd.hpp"
#include "qcir/qcir_mgr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/util.hpp"


using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;
using qsyn::qcir::QCirMgr;

namespace qsyn::pp {
dvlab::Command phase_polynomial_cmd(QCirMgr& qcir_mgr) {
    return {"phase_poly",
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

                // TODO and move to other place
                fmt::println("phase-polynomial {}", parser.get<std::string>("--resynthesis"));
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;

                Phase_Polynomial pp;
                pp.calculate_pp(*qcir_mgr.get());

                pp.print_wires(spdlog::level::level_enum::off);
                pp.print_polynomial(spdlog::level::level_enum::off);

                Partitioning partitioning(pp.get_pp_terms(), pp.get_data_qubit_num(), 0);
                Partitions partitions = partitioning.greedy_partitioning(pp.get_h_map(), pp.get_data_qubit_num());

                for_each(partitions.begin(), partitions.end(), [&](Partition p){
                    // std::cout<< "======= partition " << c << "======" << endl;
                    p.print_matrix();
                });

                return CmdExecResult::done;
            }};
}

// Note: Not sure what the props of pp_cmd should be right now...
Command pp_cmd(QCirMgr& qcir_mgr) {
    // auto cmd = dvlab::utils::mgr_root_cmd(qcir_mgr);
    // auto cmd = Command{"phase_poly",
    //                    [](ArgumentParser& parser) {
    //                        parser.description("Optimize Qcir with phasepolynomial");
    //                        parser.add_subparsers().required(true);
    //                    },
    //                    [&](ArgumentParser const& /* unused */) {
    //                        return CmdExecResult::error;
    //                    }};
    auto cmd = phase_polynomial_cmd(qcir_mgr);
    // cmd.add_subcommand(phase_polynomial_cmd(qcir_mgr));
    return cmd;
}

bool add_pp_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr) {
    if (!cli.add_command(pp_cmd(qcir_mgr))) {
        spdlog::error("Registering \"pp\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn::pp
