/****************************************************************************
  FileName     [ main.cpp ]
  PackageName  [ main ]
  Synopsis     [ Define main() ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "argparse/argparse.h"
#include "cli/cli.h"
#include "jthread/jthread.hpp"
#include "util/myUsage.h"
#include "util/util.h"

#ifndef QSYN_VERSION
#define QSYN_VERSION "0.5.1"
#endif

using namespace std;

//----------------------------------------------------------------------
//    Global cmd Manager
//----------------------------------------------------------------------
CommandLineInterface cli{"qsyn> "};

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
size_t colorLevel = 1;
size_t dmode = 0;

extern MyUsage myUsage;

int main(int argc, char** argv) {
    using namespace ArgParse;
    myUsage.reset();

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

    if (parser["-help"].isParsed()) {
        parser.printHelp();
        return 0;
    }

    cout << "DV Lab, NTUEE, Qsyn " << QSYN_VERSION << endl;

    if (parser["-version"].isParsed()) {
        return 0;
    }

    if (parser["-file"].isParsed()) {
        auto args = parser.get<std::vector<string>>("-file");

        if (!cli.openDofile(args[0])) {
            cerr << "Error: cannot open dofile!!" << endl;
            return 1;
        }

        for (auto& arg : ranges::subrange(args.begin() + 1, args.end())) {
            cli.addArgument(arg);
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
        cout << endl;  // a blank line between each command
    }

    return 0;
}
