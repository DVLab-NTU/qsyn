/****************************************************************************
  FileName     [ main.cpp ]
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

using namespace std;

//----------------------------------------------------------------------
//    Global cmd Manager
//----------------------------------------------------------------------
CommandLineInterface cli{"qsyn> "};
dvlab::utils::Logger logger;
dvlab::utils::Usage usage;

extern bool initArgParseCmd();
extern bool initCommonCmd();
extern bool initQCirCmd();
extern bool initOptimizeCmd();
extern bool initZXCmd();
extern bool initSimpCmd();
extern bool initTensorCmd();
extern bool initExtractCmd();
extern bool initDeviceCmd();
extern bool initDuostraCmd();
extern bool initGFlowCmd();
extern bool initLTCmd();
size_t verbose = 3;
size_t dmode = 0;

bool stop_requested() {
    return cli.stopRequested();
}

int main(int argc, char** argv) {
    using namespace ArgParse;

    usage.reset();

    signal(SIGINT, [](int signum) -> void { cli.sigintHandler(signum); return; });

    auto parser = ArgumentParser(argv[0]);

    parser.addArgument<string>("-file")
        .nargs(NArgsOption::ONE_OR_MORE)
        .help("specify the dofile to run, and optionally pass arguments to the dofiles");

    parser.addArgument<bool>("-help")
        .action(storeTrue)
        .help("print this help message and exit");

    parser.addArgument<bool>("-version")
        .action(storeTrue)
        .help("print the version and exit");

    std::vector<std::string> arguments{argv + 1, argv + argc};

    if (!parser.parseArgs(arguments)) {
        parser.printUsage();
        return -1;
    }

    if (parser.parsed("-help")) {
        parser.printHelp();
        return 0;
    }

    cout << "DV Lab, NTUEE, Qsyn " << QSYN_VERSION << endl;

    if (parser.parsed("-version")) {
        return 0;
    }

    if (parser.parsed("-file")) {
        auto args = parser.get<std::vector<string>>("-file");
        if (!cli.openDofile(args[0])) {
            logger.fatal("cannot open dofile!!");
            return 1;
        }

        if (!cli.saveVariables(args[0], std::ranges::subrange(arguments.begin() + 2, arguments.end()))) {
            return 1;
        }
    }

    if (
        !initArgParseCmd() ||
        !initCommonCmd() ||
        !initQCirCmd() ||
        !initOptimizeCmd() ||
        !initZXCmd() ||
        !initSimpCmd() ||
        !initTensorCmd() ||
        !initExtractCmd() ||
        !initDeviceCmd() ||
        !initDuostraCmd() ||
        !initGFlowCmd() ||
        !initLTCmd()) {
        return 1;
    }

    CmdExecResult status = CmdExecResult::DONE;

    while (status != CmdExecResult::QUIT) {  // until "quit" or command error
        status = cli.executeOneLine();
        fmt::print("\n");
    }

    return 0;
}
