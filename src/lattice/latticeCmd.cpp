/****************************************************************************
  FileName     [ latticeCmd.cpp ]
  PackageName  [ lattice ]
  Synopsis     [ Define lattice package commands ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
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
            cmdMgr->regCmd("LTS", 3, new LTCmd)

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

void LTCmd::usage(ostream &os) const {
    // os << "Usage: ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -STCOpy | -HFusion | \n"
    //                    << "-HOPF | -PIVOT | -LComp | -INTERClifford | -PIVOTGadget | -PIVOTBoundary | -CLIFford | -FReduce | -SReduce]" << endl;
    os << "Usage: LTS [ -Print ]" << endl;
}

void LTCmd::help() const {
    cout << setw(15) << left << "LTS: "
         << "(experimental) perform mapping from ZX-graph to corresponding lattice surgery" << endl;
}
