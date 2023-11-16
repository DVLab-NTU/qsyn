/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define qsyn main function ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <string>
#include <tl/enumerate.hpp>
#include <type_traits>

#include "./conversion_cmd.hpp"
#include "argparse/arg_parser.hpp"
#include "cli/cli.hpp"
#include "device/device_cmd.hpp"
#include "duostra/duostra_cmd.hpp"
#include "extractor/extractor_cmd.hpp"
#include "qcir/optimizer/optimizer_cmd.hpp"
#include "qcir/qcir_cmd.hpp"
#include "tensor/tensor_cmd.hpp"
#include "util/text_format.hpp"
#include "util/usage.hpp"
#include "util/util.hpp"
#include "zx/simplifier/simp_cmd.hpp"
#include "zx/zx_cmd.hpp"

#ifndef QSYN_VERSION
#define QSYN_VERSION "[unknown version]"
#endif

//----------------------------------------------------------------------
//    Global cmd Manager
//----------------------------------------------------------------------

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

bool initialize_qsyn() {
    spdlog::set_pattern("%L%v");
    spdlog::set_level(spdlog::level::warn);
    signal(SIGINT, [](int signum) -> void {
        cli.sigint_handler(signum);
        return;
    });
    if (!dvlab::add_cli_common_cmds(cli) ||
        !qsyn::device::add_device_cmds(cli, device_mgr) ||
        !qsyn::duostra::add_duostra_cmds(cli, qcir_mgr, device_mgr) ||
        !qsyn::add_conversion_cmds(cli, qcir_mgr, tensor_mgr, zxgraph_mgr) ||
        !qsyn::extractor::add_extract_cmds(cli, zxgraph_mgr, qcir_mgr) ||
        !qsyn::qcir::add_qcir_cmds(cli, qcir_mgr) ||
        !qsyn::tensor::add_tensor_cmds(cli, tensor_mgr) ||
        !qsyn::zx::add_zx_cmds(cli, zxgraph_mgr)) {
        return false;
    }
    dvlab::utils::Usage::reset();
    return true;
}

dvlab::argparse::ArgumentParser get_qsyn_parser(std::string_view const prog_name) {
    using namespace dvlab::argparse;

    auto parser = ArgumentParser(std::string{prog_name}, {.add_help_action    = true,
                                                          .add_version_action = true,
                                                          .exit_on_failure    = true,
                                                          .version            = QSYN_VERSION});
    auto mutex  = parser.add_mutually_exclusive_group();

    mutex.add_argument<std::string>("-c", "--command")
        .nargs(NArgsOption::one_or_more)
        .usage(fmt::format("{} {}{}{}",
                           dvlab::fmt_ext::styled_if_ansi_supported("cmd", fmt::emphasis::bold),
                           dvlab::fmt_ext::styled_if_ansi_supported("[", fmt::fg(fmt::terminal_color::yellow)),
                           dvlab::fmt_ext::styled_if_ansi_supported("arg", fmt::emphasis::bold),
                           dvlab::fmt_ext::styled_if_ansi_supported("]", fmt::fg(fmt::terminal_color::yellow))))
        .help("specify the command to run, and optionally pass arguments to the dofiles");

    mutex.add_argument<std::string>("-f", "--file")
        .nargs(NArgsOption::one_or_more)
        .usage(fmt::format("{} {}{}{}",
                           dvlab::fmt_ext::styled_if_ansi_supported("filepath", fmt::emphasis::bold),
                           dvlab::fmt_ext::styled_if_ansi_supported("[", fmt::fg(fmt::terminal_color::yellow)),
                           dvlab::fmt_ext::styled_if_ansi_supported("arg", fmt::emphasis::bold),
                           dvlab::fmt_ext::styled_if_ansi_supported("]", fmt::fg(fmt::terminal_color::yellow))))
        .help("specify the dofile to run, and optionally pass arguments to the dofiles");

    parser.add_argument<bool>("-q", "--quiet")
        .action(store_true)
        .help("suppress echo of commands when supplying commands from `-c` or `-f` flags. This argument does not affect the interactive mode");

    parser.add_argument<bool>("--no-version")
        .action(store_true)
        .help("suppress version information on start up");

    return parser;
}

int main(int argc, char** argv) {
    using namespace dvlab::argparse;

    if (!initialize_qsyn()) {
        return -1;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto parser = get_qsyn_parser(argv[0]);

    std::vector<std::string> const arguments{std::next(argv), std::next(argv, argc)};

    if (!parser.parse_args(arguments)) {
        parser.print_usage();
        return -1;
    }

    if (!parser.parsed("--no-version")) {
        fmt::println("{}", version_str);
    }

    auto const quiet = parser.get<bool>("--quiet");

    if (parser.parsed("--command")) {
        auto args = parser.get<std::vector<std::string>>("--command");

        auto cmd_stream = std::stringstream(args[0]);

        for (auto&& [i, arg] : tl::views::enumerate(std::ranges::subrange(args.begin() + 1, args.end()))) {
            cli.add_variable(std::to_string(i + 1), arg);
        }

        auto const result = cli.execute_one_line(cmd_stream, !quiet);

        if (result == dvlab::CmdExecResult::quit) {
            return 0;
        }
    }

    if (parser.parsed("--file")) {
        auto args = parser.get<std::vector<std::string>>("--file");

        auto const result = cli.source_dofile(args[0], std::ranges::subrange(args.begin() + 1, args.end()), !quiet);

        if (result == dvlab::CmdExecResult::quit) {
            return 0;
        }
    }

    auto const result = cli.start_interactive();

    return static_cast<std::underlying_type_t<dvlab::CmdExecResult>>(result);
}
