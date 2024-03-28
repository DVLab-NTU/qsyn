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
    "Licensed under Apache 2.0 License.",
    QSYN_VERSION, std::chrono::system_clock::now());
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

    auto const quiet = parser.get<bool>("--quiet");

    if (!parser.parsed("--no-version") && !quiet) {
        fmt::println("{}", version_str);
    }

    if (!qsyn::read_qsynrc_file(cli, parser.get<std::string>("--qsynrc-path"))) {
        return -1;
    }

    if (parser.parsed("--command")) {
        auto args = parser.get<std::vector<std::string>>("--command");

        auto cmd_stream = std::stringstream(args[0]);

        for (auto&& [i, arg] : tl::views::enumerate(std::ranges::subrange(args.begin() + 1, args.end()))) {
            cli.add_variable(std::to_string(i + 1), arg);
        }

        auto const result = cli.execute_one_line(cmd_stream, !quiet);

        if (result == dvlab::CmdExecResult::quit) {
            return dvlab::get_exit_code(cli.get_last_return_status());
        }
    }

    if (parser.parsed("--file")) {
        auto args = parser.get<std::vector<std::string>>("--file");

        auto const result = cli.source_dofile(args[0], std::ranges::subrange(args.begin() + 1, args.end()), !quiet);

        if (result == dvlab::CmdExecResult::quit) {
            return dvlab::get_exit_code(cli.get_last_return_status());
        }
    }

    return dvlab::get_exit_code(cli.start_interactive());
}
