#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "argparse/arg_parser.hpp"
#include "cli/cli.hpp"
#include "cmd/zxgraph_mgr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::zx {

Command zxgraph_test_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"test",
            [](ArgumentParser& parser) {
                parser.description("test ZXGraph structures and functions");

                auto mutex = parser.add_mutually_exclusive_group()
                                 .required(true);

                mutex.add_argument<bool>("-g", "--graph-like")
                    .action(store_true)
                    .help("check if the ZXGraph is graph-like");
                mutex.add_argument<bool>("-i", "--identity")
                    .action(store_true)
                    .help("check if the ZXGraph is equivalent to identity");
            },
            [&](ArgumentParser const& parser) {
                if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) {
                    return CmdExecResult::error;
                }
                if (parser.parsed("--graph-like")) {
                    if (is_graph_like(*zxgraph_mgr.get())) {
                        fmt::println("The graph is graph-like!");
                    } else {
                        fmt::println("The graph is not graph-like!");
                    }
                } else if (parser.parsed("--identity")) {
                    if (zxgraph_mgr.get()->is_identity()) {
                        fmt::println("The graph is an identity!");
                    } else {
                        fmt::println("The graph is not an identity!");
                    }
                }
                return CmdExecResult::done;
            }};
}

}  // namespace qsyn::zx
