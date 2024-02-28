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
#include <type_traits>

#include "./qsyn_helper.hpp"
#include "argparse/arg_parser.hpp"
#include "cli/cli.hpp"
#include "convert/conversion_cmd.hpp"
#include "device/device_cmd.hpp"
#include "duostra/duostra_cmd.hpp"
#include "extractor/extractor_cmd.hpp"
#include "qcir/qcir_cmd.hpp"
#include "tensor/tensor_cmd.hpp"
#include "util/sysdep.hpp"
#include "util/text_format.hpp"
#include "util/usage.hpp"
#include "util/util.hpp"
#include "zx/simplifier/simp_cmd.hpp"
#include "zx/zx_cmd.hpp"

#ifndef QSYN_VERSION
#define QSYN_VERSION "[unknown version]"
#endif

namespace {

dvlab::CommandLineInterface cli{"qsyn> "};
qsyn::device::DeviceMgr device_mgr{"Device"};
qsyn::qcir::QCirMgr qcir_mgr{"QCir"};
qsyn::tensor::TensorMgr tensor_mgr{"Tensor"};
qsyn::zx::ZXGraphMgr zxgraph_mgr{"ZXGraph"};

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

    if (!qsyn::initialize_qsyn(cli, device_mgr, qcir_mgr, tensor_mgr, zxgraph_mgr)) {
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
            return cli.get_last_return_code();
        }
    }

    if (parser.parsed("--file")) {
        auto args = parser.get<std::vector<std::string>>("--file");

        auto const result = cli.source_dofile(args[0], std::ranges::subrange(args.begin() + 1, args.end()), !quiet);

        if (result == dvlab::CmdExecResult::quit) {
            return cli.get_last_return_code();
        }
    }

    return cli.start_interactive();
}
