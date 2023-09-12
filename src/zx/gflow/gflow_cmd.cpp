/****************************************************************************
  PackageName  [ gflow ]
  Synopsis     [ Define gflow package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./gflow_cmd.hpp"

#include <cstddef>
#include <string>

#include "./gflow.hpp"
#include "cli/cli.hpp"
#include "zx/zx_cmd.hpp"
#include "zx/zxgraph.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::zx {

dvlab::Command zxgraph_gflow_cmd();

Command zxgraph_gflow_cmd(ZXGraphMgr const& zxgraph_mgr) {
    return {"zxggflow",
            [](ArgumentParser& parser) {
                parser.description("calculate and print the generalized flow of a ZXGraph");

                auto mutex = parser.add_mutually_exclusive_group().required(false);

                mutex.add_argument<bool>("-all")
                    .action(store_true)
                    .help("print both GFlow levels and correction sets");
                mutex.add_argument<bool>("-levels")
                    .action(store_true)
                    .help("print GFlow levels");
                mutex.add_argument<bool>("-corrections")
                    .action(store_true)
                    .help("print the correction set to each ZXVertex");
                mutex.add_argument<bool>("-summary")
                    .action(store_true)
                    .help("print basic information on the ZXGraph's GFlow");

                parser.add_argument<bool>("-extended")
                    .action(store_true)
                    .help("calculate the extended GFlow, i.e., allowing XY, YZ, XZ plane measurements");

                parser.add_argument<bool>("-independent-set")
                    .action(store_true)
                    .help("force each GFlow level to be an independent set");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                GFlow gflow(zxgraph_mgr.get());

                gflow.do_extended_gflow(parser.get<bool>("-extended"));
                gflow.do_independent_layers(parser.get<bool>("-independent-set"));

                gflow.calculate();

                if (parser.parsed("-all")) {
                    gflow.print();
                } else if (parser.parsed("-levels")) {
                    gflow.print_levels();
                } else if (parser.parsed("-corrections")) {
                    gflow.print_x_correction_sets();
                }

                gflow.print_summary();
                if (!gflow.is_valid()) gflow.print_failed_vertices();

                return CmdExecResult::done;
            }};
}

bool add_zx_gflow_cmds(dvlab::CommandLineInterface& cli, ZXGraphMgr& zxgraph_mgr) {
    if (!cli.add_command(zxgraph_gflow_cmd(zxgraph_mgr))) {
        std::cerr << "Registering \"gflow\" commands fails... exiting" << std::endl;
        return false;
    }
    return true;
}

}  // namespace qsyn::zx