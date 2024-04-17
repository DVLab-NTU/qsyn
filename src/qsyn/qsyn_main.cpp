/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define qsyn main function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <filesystem>
#include <string>
#include <tl/enumerate.hpp>

#include "./qsyn_helper.hpp"
#include "argparse/arg_parser.hpp"
#include "cli/cli.hpp"
#include "device/device_mgr.hpp"
#include "qcir/qcir_mgr.hpp"
#include "tableau/tableau_mgr.hpp"
#include "tensor/tensor_mgr.hpp"
#include "zx/zxgraph_mgr.hpp"

#ifndef QSYN_VERSION
#define QSYN_VERSION "[unknown version]"
#endif
#ifndef QSYN_BUILD_TYPE
#define QSYN_BUILD_TYPE "[unknown build type]"
#endif

namespace {
// NOLINTBEGIN(readability-identifier-naming)
dvlab::CommandLineInterface cli{"qsyn> "};
qsyn::device::DeviceMgr device_mgr{"Device"};
qsyn::qcir::QCirMgr qcir_mgr{"QCir"};
qsyn::tensor::TensorMgr tensor_mgr{"Tensor"};
qsyn::zx::ZXGraphMgr zxgraph_mgr{"ZXGraph"};
qsyn::experimental::TableauMgr tableau_mgr{"Tableau"};
// NOLINTEND(readability-identifier-naming)

std::string const version_str = fmt::format(
    "qsyn {} - Copyright © 2022-{:%Y}, DVLab NTUEE.\n"
    "Licensed under Apache 2.0 License. {} build.",
    QSYN_VERSION, std::chrono::system_clock::now(),
    QSYN_BUILD_TYPE);
}  // namespace

bool stop_requested() { return cli.stop_requested(); }

int main(int argc, char** argv) {
    using namespace dvlab::argparse;

    signal(SIGINT, [](int signum) -> void {
        cli.sigint_handler(signum);
        return;
    });

    if (!qsyn::initialize_qsyn(cli, device_mgr, qcir_mgr, tensor_mgr, zxgraph_mgr, tableau_mgr)) {
        return -1;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto parser = qsyn::get_qsyn_parser(argv[0]);

    std::vector<std::string> const arguments{std::next(argv), std::next(argv, argc)};

    if (!parser.parse_args(arguments)) {
        parser.print_usage();
        return -1;
    }

    auto const verbose = parser.parsed("--verbose") || parser.parsed("--file");

    auto const print_version = !parser.parsed("--no-version") && ((!parser.parsed("filepath") && !parser.parsed("--command")) || verbose);

    if (print_version) {
        fmt::println("{}", version_str);
    }

    if (!qsyn::read_qsynrc_file(cli, parser.get<std::string>("--qsynrc-path"))) {
        return -1;
    }

    auto const args = parser.get<std::vector<std::string>>("args");

    if (parser.parsed("--command")) {
        auto const cmds = parser.get<std::string>("--command");

        auto cmd_stream = std::stringstream(cmds);

        for (auto&& [i, arg] : tl::views::enumerate(args)) {
            cli.add_variable(std::to_string(i + 1), arg);
        }

        cli.execute_one_line(cmd_stream, verbose);
        return dvlab::get_exit_code(cli.get_last_return_status());
    }

    if (parser.parsed("filepath")) {
        auto const filepath = parser.get<std::string>("filepath");

        cli.source_dofile(filepath, args, verbose);
        if (parser.parsed("--file")) {
            spdlog::warn("The -f/--file option is deprecated and will be removed in the future.");
            spdlog::warn("To run a script file with commands printing, use the -v flag with a filepath.");
            spdlog::warn("To run a script file silently, simply supply the filepath.");
        }
        return dvlab::get_exit_code(cli.get_last_return_status());
    }

    return dvlab::get_exit_code(cli.start_interactive());
}
