/****************************************************************************
  PackageName  [ gflow ]
  Synopsis     [ Define gflow package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./gflow_cmd.hpp"

#include <spdlog/spdlog.h>

#include "cli/cli.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "zx/flow/gflow.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::zx {

Command zxgraph_gflow_cmd(ZXGraphMgr const& zxgraph_mgr) {
    return {"gflow",
            [](ArgumentParser& parser) {
                parser.description("calculate and print the generalized flow of a ZXGraph");

                auto mutex = parser.add_mutually_exclusive_group().required(false);

                mutex.add_argument<bool>("-a", "--all")
                    .action(store_true)
                    .help("print both GFlow levels and correction sets");
                mutex.add_argument<bool>("-l", "--levels")
                    .action(store_true)
                    .help("print GFlow levels");
                mutex.add_argument<bool>("-c", "--corrections")
                    .action(store_true)
                    .help("print the correction set to each ZXVertex");
                mutex.add_argument<bool>("-s", "--summary")
                    .action(store_true)
                    .help("print basic information on the ZXGraph's GFlow");

                parser.add_argument<bool>("--only-xy-plane")
                    .action(store_true)
                    .help("Only allowing XY plane measurements");

                parser.add_argument<bool>("--independent-set")
                    .action(store_true)
                    .help("force each GFlow level to be an independent set");
            },
            [&](ArgumentParser const& parser) {
                if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return CmdExecResult::error;
                GFlow gflow(zxgraph_mgr.get());

                gflow.do_extended_gflow(!parser.get<bool>("--only-xy-plane"));
                gflow.do_independent_layers(parser.get<bool>("--independent-set"));

                gflow.calculate();

                if (parser.parsed("--all")) {
                    gflow.print();
                } else if (parser.parsed("--levels")) {
                    gflow.print_levels();
                } else if (parser.parsed("--corrections")) {
                    gflow.print_x_correction_sets();
                }

                gflow.print_summary();
                if (!gflow.is_valid()) gflow.print_failed_vertices();

                return CmdExecResult::done;
            }};
}

}  // namespace qsyn::zx
