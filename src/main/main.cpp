/****************************************************************************
  PackageName  [ main ]
  Synopsis     [ Define main() ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <csignal>

#include "argparse/argparse.hpp"
#include "cli/cli.hpp"
#include "util/logger.hpp"
#include "util/usage.hpp"
#include "util/util.hpp"

#ifndef QSYN_VERSION
#define QSYN_VERSION "[unknown version]"
#endif

//----------------------------------------------------------------------
//    Global cmd Manager
//----------------------------------------------------------------------
CommandLineInterface CLI{"qsyn> "};
dvlab::Logger LOGGER;
dvlab::utils::Usage USAGE;
size_t VERBOSE = 3;

extern bool add_argparse_cmds();
extern bool add_cli_common_cmds();
extern bool add_qcir_cmds();
extern bool add_qcir_optimize_cmds();
extern bool add_zx_cmds();
extern bool add_zx_simplifier_cmds();
extern bool add_tensor_cmds();
extern bool add_extract_cmds();
extern bool add_device_cmds();
extern bool add_duostra_cmds();
extern bool add_zx_gflow_cmds();

bool stop_requested() {
    return CLI.stop_requested();
}

int main(int argc, char** argv) {
    using namespace argparse;

    USAGE.reset();

    signal(SIGINT, [](int signum) -> void { CLI.sigint_handler(signum); return; });
    constexpr auto version_str = "DV Lab, NTUEE, Qsyn " QSYN_VERSION;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto parser = ArgumentParser(argv[0], {.add_help_action = true, .add_version_action = true, .exitOnFailure = true, .version = version_str});

    parser.add_argument<std::string>("-file")
        .nargs(NArgsOption::one_or_more)
        .help("specify the dofile to run, and optionally pass arguments to the dofiles");

    std::vector<std::string> arguments{std::next(argv), std::next(argv, argc)};

    if (!parser.parse_args(arguments)) {
        parser.print_usage();
        return -1;
    }

    if (parser.parsed("-file")) {
        auto args = parser.get<std::vector<std::string>>("-file");
        if (!CLI.open_dofile(args[0])) {
            LOGGER.fatal("cannot open dofile!!");
            return 1;
        }

        if (!CLI.add_variables_from_dofiles(args[0], std::ranges::subrange(arguments.begin() + 2, arguments.end()))) {
            return 1;
        }
    }

    fmt::println("{}", version_str);

    if (
        !add_argparse_cmds() ||
        !add_cli_common_cmds() ||
        !add_qcir_cmds() ||
        !add_qcir_optimize_cmds() ||
        !add_zx_cmds() ||
        !add_zx_simplifier_cmds() ||
        !add_tensor_cmds() ||
        !add_extract_cmds() ||
        !add_device_cmds() ||
        !add_duostra_cmds() ||
        !add_zx_gflow_cmds()) {
        return 1;
    }

    CmdExecResult status = CmdExecResult::done;

    while (status != CmdExecResult::quit) {  // until "quit" or command error
        status = CLI.execute_one_line();
        fmt::print("\n");
    }

    return 0;
}
