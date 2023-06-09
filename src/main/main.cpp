/****************************************************************************
  FileName     [ main.cpp ]
  PackageName  [ main ]
  Synopsis     [ Define main() ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <stdlib.h>  // for exit

#include <csignal>
#include <cstddef>
#include <fstream>
#include <iostream>

#include "cmdParser.h"  // for CmdExecStatus, CmdExecStatus::CMD_EXEC_DONE
#include "myUsage.h"    // for MyUsage
#include "util.h"       // for myUsage

using namespace std;

//----------------------------------------------------------------------
//    Global cmd Manager
//----------------------------------------------------------------------
CmdParser* cmdMgr = new CmdParser("qsyn> ");

extern bool initArgParserCmd();
extern bool initCommonCmd();
extern bool initQCirCmd();
extern bool initOptimizeCmd();
extern bool initZXCmd();
extern bool initSimpCmd();
extern bool initTensorCmd();
extern bool initExtractCmd();
extern bool initDeviceCmd();
extern bool initDuostraCmd();
extern bool initM2Cmd();
extern bool initGFlowCmd();
extern bool initLTCmd();
size_t verbose = 3;
size_t colorLevel = 1;

extern MyUsage myUsage;

static void
usage() {
    cout << "Usage: ./qsyn [ -File < doFile > ]" << endl;
}

static void
myexit() {
    usage();
    exit(-1);
}

int main(int argc, char** argv) {
    myUsage.reset();

    signal(SIGINT, [](int signum) -> void { cmdMgr->sigintHandler(signum); return; });

    if (argc == 3) {  // -file <doFile>
        if (myStrNCmp("-File", argv[1], 2) == 0) {
            if (!cmdMgr->openDofile(argv[2])) {
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

    cout << "DV Lab, NTUEE, Qsyn 0.4.2" << endl;

    if (
        // !initArgParserCmd() ||
        !initCommonCmd() ||
        !initQCirCmd() ||
        !initOptimizeCmd() ||
        !initZXCmd() ||
        !initSimpCmd() ||
        !initTensorCmd() ||
        // !initM2Cmd() ||
        !initExtractCmd() ||
        !initDeviceCmd() ||
        !initDuostraCmd() ||
        !initGFlowCmd() ||
        !initLTCmd()) {
        return 1;
    }

    CmdExecStatus status = CMD_EXEC_DONE;

    while (status != CMD_EXEC_QUIT) {  // until "quit" or command error
        status = cmdMgr->execOneCmd();
        cout << endl;  // a blank line between each command
    }

    return 0;
}
