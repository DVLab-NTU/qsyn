/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define qsyn main function ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/chrono.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <csignal>

#include "./conversion_cmd.hpp"
#include "cli/cli.hpp"
#include "device/device_cmd.hpp"
#include "duostra/duostra_cmd.hpp"
#include "extractor/extractor_cmd.hpp"
#include "qcir/optimizer/optimizer_cmd.hpp"
#include "qcir/qcir_cmd.hpp"
#include "tensor/tensor_cmd.hpp"
#include "util/usage.hpp"
#include "util/util.hpp"
#include "zx/gflow/gflow_cmd.hpp"
#include "zx/simplifier/simp_cmd.hpp"
#include "zx/zx_cmd.hpp"

#ifndef QSYN_VERSION
#define QSYN_VERSION "[unknown version]"
#endif

//----------------------------------------------------------------------
//    Global cmd Manager
//----------------------------------------------------------------------
dvlab::utils::Usage USAGE;
size_t VERBOSE = 3;

namespace {

dvlab::CommandLineInterface cli{"qsyn> "};
qsyn::device::DeviceMgr device_mgr{"Device"};
qsyn::qcir::QCirMgr qcir_mgr{"QCir"};
qsyn::tensor::TensorMgr tensor_mgr{"Tensor"};
qsyn::zx::ZXGraphMgr zxgraph_mgr{"ZXGraph"};

}  // namespace

bool stop_requested() {
    return cli.stop_requested();
}

int main(int argc, char** argv) {
    using namespace dvlab::argparse;

    spdlog::set_pattern("[%l] %v");
    spdlog::set_level(spdlog::level::warn);

    std::string version_str = fmt::format(
        "qsyn {} - Copyright © 2022-{:%Y}, DVLab NTUEE.\n"
        "Licensed under Apache 2.0 License.",
        QSYN_VERSION, std::chrono::system_clock::now());

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto parser = ArgumentParser(argv[0], {.add_help_action = true, .add_version_action = true, .exitOnFailure = true, .version = version_str});

    parser.add_argument<std::string>("-f", "--file")
        .nargs(NArgsOption::one_or_more)
        .help("specify the dofile to run, and optionally pass arguments to the dofiles");

    parser.add_argument<bool>("-s", "--silent")
        .action(store_true)
        .help("suppress version information on start up");

    std::vector<std::string> arguments{std::next(argv), std::next(argv, argc)};

    if (!parser.parse_args(arguments)) {
        parser.print_usage();
        return -1;
    }

    if (parser.parsed("--file")) {
        auto args = parser.get<std::vector<std::string>>("--file");
        if (!cli.open_dofile(args[0])) {
            spdlog::critical("cannot open dofile!!");
            return 1;
        }

        if (!cli.add_variables_from_dofiles(args[0], std::ranges::subrange(args.begin() + 1, args.end()))) {
            return 1;
        }
    }

    if (!parser.parsed("--silent")) {
        fmt::println("{}", version_str);
    }

    if (!dvlab::add_cli_common_cmds(cli) ||
        !qsyn::device::add_device_cmds(cli, device_mgr) ||
        !qsyn::duostra::add_duostra_cmds(cli, qcir_mgr, device_mgr) ||
        !qsyn::add_conversion_cmds(cli, qcir_mgr, tensor_mgr, zxgraph_mgr) ||
        !qsyn::extractor::add_extract_cmds(cli, zxgraph_mgr, qcir_mgr) ||
        !qsyn::qcir::add_qcir_cmds(cli, qcir_mgr) ||
        !qsyn::qcir::add_qcir_optimize_cmds(cli, qcir_mgr) ||
        !qsyn::tensor::add_tensor_cmds(cli, tensor_mgr) ||
        !qsyn::zx::add_zx_cmds(cli, zxgraph_mgr) ||
        !qsyn::zx::add_zx_gflow_cmds(cli, zxgraph_mgr) ||
        !qsyn::zx::add_zx_simplifier_cmds(cli, zxgraph_mgr)) {
        return 1;
    }

    USAGE.reset();

    signal(SIGINT, [](int signum) -> void { cli.sigint_handler(signum); return; });

    auto status = dvlab::CmdExecResult::done;

    while (status != dvlab::CmdExecResult::quit) {  // until "quit" or command error
        status = cli.execute_one_line();
        fmt::print("\n");
    }

    return 0;
}
