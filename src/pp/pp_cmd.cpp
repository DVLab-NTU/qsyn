/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Define pp package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>  // to delete
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

dvlab::Command phase_polynomial_t_depth_cmd(QCirMgr& qcir_mgr) {
    return {"calcTDepth",
            [](ArgumentParser& parser) {
                parser.description("Report t-depth and t-count");
            },
            [&](ArgumentParser const& /*parser*/) {
                if (qcir_mgr.empty()) {
                    spdlog::info("QCir list is empty now. Create a new one.");
                    qcir_mgr.add(qcir_mgr.get_next_id());
                    return CmdExecResult::error;
                }
                Phase_Polynomial pp;
                pp.count_t_depth(*qcir_mgr.get());
                return CmdExecResult::done;
            }};
}

dvlab::Command phase_polynomial_print_cmd(QCirMgr& qcir_mgr) {
    return {"print",
            [](ArgumentParser& parser) {
                parser.description("Print Phase-Polynonmials");
            },
            [&](ArgumentParser const& /*parser*/) {
                if (qcir_mgr.empty()) {
                    spdlog::info("QCir list is empty now. Create a new one.");
                    qcir_mgr.add(qcir_mgr.get_next_id());
                    return CmdExecResult::error;
                }
                Phase_Polynomial pp;
                pp.calculate_pp(*qcir_mgr.get());
                pp.print_phase_poly();
                return CmdExecResult::done;
            }};
}

dvlab::Command phase_polynomial_cmd(QCirMgr& qcir_mgr) {
    return {"phase_poly",
            [](ArgumentParser& parser) {
                parser.description("perform phase polynomial optimizer");

                // a=-1: unlimited ancilla
                parser.add_argument<int>("-a", "--ancilla")
                    .nargs(NArgsOption::optional)
                    .default_value(0)
                    .help("the number of ancilla to be added (default=0)");

                parser.add_argument<std::string>("-par", "--partition")
                    .constraint(choices_allow_prefix({"G", "M"}))
                    .default_value("G")
                    .help("the partition method chosen(G/M). If not specified, the default method i\ns greedy partition(G).");

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
                auto const ancilla = parser.get<int>("--ancilla");
                // fmt::println("Calculating phase-polynomial {}", parser.get<std::string>("--resynthesis"));
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;

                Phase_Polynomial pp;
                pp.calculate_pp(*qcir_mgr.get());

                pp.print_phase_poly();

                Partitioning partitioning(pp.get_pp_terms(), pp.get_data_qubit_num(), ancilla);
                Partitions temp;  // todo: rewrite the dirty code ==
                size_t rank = pp.get_data_qubit_num();
                for (size_t i = 0; i <= pp.get_h_map().size(); i++) {
                    dvlab::BooleanMatrix initial_wires;
                    dvlab::BooleanMatrix terminal_wires = (i != pp.get_h_map().size()) ? pp.get_h_map()[i].first : pp.get_wires();

                    if (i == 0) {
                        for (size_t j = 0; j < pp.get_wires().num_rows(); j++) {
                            dvlab::BooleanMatrix::Row r(pp.get_wires().num_cols());
                            initial_wires.push_row(r);
                            if (j < pp.get_data_qubit_num()) initial_wires[j][j] = 1;
                        }

                    } else
                        initial_wires = pp.get_h_map()[i - 1].second;

                    Partitions partitions = partitioning.greedy_partitioning_routine(temp, initial_wires, rank);

                    pp.gaussian_resynthesis(partitions, initial_wires, terminal_wires);

                    if (i != pp.get_h_map().size()) pp.add_H_gate(i);
                }

                qcir::QCir result = pp.get_result();
                pp.count_t_depth(result);
                qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<qcir::QCir>(result));

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
    cmd.add_subcommand(phase_polynomial_print_cmd(qcir_mgr));
    cmd.add_subcommand(phase_polynomial_t_depth_cmd(qcir_mgr));

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
