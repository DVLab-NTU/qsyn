/****************************************************************************
  FileName     [ latticeCmd.cpp ]
  PackageName  [ lattice ]
  Synopsis     [ Define lattice package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "latticeCmd.h"

#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>
#include <string>  // for string

#include "lattice.h"     // for LTContainer
#include "zxGraphMgr.h"  // for ZXGraphMgr, zxGraphMgr

using namespace std;
extern size_t verbose;

bool initLTCmd() {
    if (!(
            cmdMgr->regCmd("LTS", 3, make_unique<LTCmd>())

                )) {
        cerr << "Registering \"lts\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------------------------------------------
//    LT [ -p ]
//------------------------------------------------------------------------------------------------------------------
CmdExecStatus
LTCmd::exec(const string &option) {
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()) {
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXPrint." << endl;
        return CMD_EXEC_ERROR;
    }

    LTContainer lt(0, 0);
    lt.generateLTC(zxGraphMgr->getGraph());
    if (token.empty())
        lt.printLTC();

    else
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);

    return CMD_EXEC_DONE;
}

void LTCmd::usage() const {
    cout << "Usage: LTS [ -Print ]" << endl;
}

void LTCmd::summary() const {
    cout << setw(15) << left << "LTS: "
         << "(experimental) perform mapping from ZX-graph to corresponding lattice surgery" << endl;
}
