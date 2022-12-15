/****************************************************************************
  FileName     [ gFlowCmd.cpp ]
  PackageName  [ gflow ]
  Synopsis     [ Define gflow package commands ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#include "gFlowCmd.h"
#include "gFlow.h"

#include "zxGraph.h"
#include "zxGraphMgr.h"
#include <iomanip>

using namespace std;

extern ZXGraphMgr *zxGraphMgr;
extern size_t verbose;

bool initGFlowCmd() {
    if (!cmdMgr->regCmd("ZXGGFlow", 5, new ZXGGFlowCmd)) {
        cerr << "Registering \"gflow\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZXGGFlow [-All | -Level]
//----------------------------------------------------------------------
CmdExecStatus
ZXGGFlowCmd::exec(const string &option) {  
    string token;  
    if (!lexSingleOption(option, token, true)) return CMD_EXEC_ERROR;

    if (zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()) {
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXGGFlow." << endl;
        return CMD_EXEC_ERROR;
    }
    GFlow gflow(zxGraphMgr->getGraph());

    gflow.calculate();

    if (token.empty() || myStrNCmp("-All", token, 2) == 0) {
        gflow.print();
    } else if (myStrNCmp("-Level", token, 2) == 0) {
        gflow.printLevels();
    } else {
        return errorOption(CMD_OPT_ILLEGAL, token);
    }
    
    return CMD_EXEC_DONE;
}

void ZXGGFlowCmd::usage(ostream &os) const {
    os << "Usage: ZXGGFlow [-All | -Level]" << endl;
}

void ZXGGFlowCmd::help() const {
    cout << setw(15) << left << "ZXGGFlow: "
         << "calculate the generalized flow of current ZX-graph.\n";
}