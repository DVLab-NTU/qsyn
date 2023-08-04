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

#include "cli.h"
#include "jthread.hpp"
#include "myUsage.h"
#include "util.h"

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

static void
usage() {
    cout << "Usage: ./qsyn [-File < doFile > [arguments...]]" << endl;
}

static void
myexit() {
    usage();
    exit(-1);
}

int main(int argc, char** argv) {
    myUsage.reset();

    signal(SIGINT, [](int signum) -> void { cli.sigintHandler(signum); return; });

    if (argc >= 3) {  // -file <doFile>
        for (int i = 3; i < argc; ++i) {
            cli.addArgument(argv[i]);
        }

        if (myStrNCmp("-File", argv[1], 2) == 0) {
            if (!cli.openDofile(argv[2])) {
                cerr << "Error: cannot open file \"" << argv[2] << "\"!!\n";
                myexit();
            }
        } else {
            cerr << "Error: unknown argument \"" << argv[1] << "\"!!\n";
            myexit();
        }
    } else if (argc != 1) {
        cerr << "Error: illegal number of argument (" << argc << ")!!\n";
        myexit();
    }

    cout << "DV Lab, NTUEE, Qsyn " << QSYN_VERSION << endl;

    if (
        // !initArgParseCmd() ||
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
