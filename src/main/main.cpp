/****************************************************************************
  FileName     [ main.cpp ]
  PackageName  [ main ]
  Synopsis     [ Define main() ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2007-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/

#include <ctime>

#include "cmdParser.h"
#include "util.h"
using namespace std;

//----------------------------------------------------------------------
//    Global cmd Manager
//----------------------------------------------------------------------
CmdParser* cmdMgr = new CmdParser("qsyn> ");

extern bool initCommonCmd();
extern bool initQCirCmd();
extern bool initZXCmd();
extern bool initSimpCmd();
extern bool initTensorCmd();
extern bool initExtractCmd();
extern bool initM2Cmd();
extern bool initGFlowCmd();
extern bool initLTCmd();
size_t verbose = 3;
size_t colorLevel = 1;

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

    ifstream dof;

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

    if (
        !initCommonCmd() ||
        !initQCirCmd() ||
        !initZXCmd() ||
        !initSimpCmd() ||
        !initTensorCmd() ||
        !initM2Cmd() ||
        !initExtractCmd() ||
        !initGFlowCmd() ||
        !initLTCmd()) {
        return 1;
    }

    CmdExecStatus status = CMD_EXEC_DONE;
    // time_t result = time(nullptr);
    cout << "DV Lab, NTUEE, Qsyn 0.3.0" << endl;
    // cerr << "DV Lab, NTUEE, Qsyn 0.3.0, compiled " << ctime(&result);
    while (status != CMD_EXEC_QUIT) {  // until "quit" or command error
        status = cmdMgr->execOneCmd();
        cout << endl;  // a blank line between each command
    }

    return 0;
}
