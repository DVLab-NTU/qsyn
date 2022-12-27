/****************************************************************************
  FileName     [ gFlowCmd.cpp ]
  PackageName  [ gflow ]
  Synopsis     [ Define gflow package commands ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "gFlowCmd.h"

#include <iomanip>

#include "gFlow.h"
#include "zxGraph.h"
#include "zxGraphMgr.h"

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
//    ZXGGFlow [-All | -Summary | -Levels | -CorrectionSets]
//----------------------------------------------------------------------
CmdExecStatus
ZXGGFlowCmd::exec(const string &option) {
    enum class GFLOW_PRINT_MODE {
        ALL,
        LEVELS,
        CORRECTION_SETS,
        SUMMARY,
        ERROR
    };
    string token;
    if (!lexSingleOption(option, token, true)) return CMD_EXEC_ERROR;

    if (zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()) {
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXGGFlow." << endl;
        return CMD_EXEC_ERROR;
    }
    GFlow gflow(zxGraphMgr->getGraph());

    GFLOW_PRINT_MODE mode = GFLOW_PRINT_MODE::ERROR;
    if (myStrNCmp("-All", token, 2) == 0) {
        mode = GFLOW_PRINT_MODE::ALL;
    } else if (myStrNCmp("-Levels", token, 2) == 0) {
        mode = GFLOW_PRINT_MODE::LEVELS;
    } else if (myStrNCmp("-Correctionsets", token, 2) == 0) {
        mode = GFLOW_PRINT_MODE::CORRECTION_SETS;
    } else if (token.empty() || myStrNCmp("-Summary", token, 2) == 0) {
        mode = GFLOW_PRINT_MODE::SUMMARY;
    } else {
        return errorOption(CMD_OPT_ILLEGAL, token);
    }
    gflow.calculate();

    switch (mode) {
        case GFLOW_PRINT_MODE::ALL:
            gflow.print();
            gflow.printSummary();
            if (!gflow.isValid()) gflow.printFailedVertices();
            break;
        case GFLOW_PRINT_MODE::LEVELS:
            gflow.printLevels();
            gflow.printSummary();
            if (!gflow.isValid()) gflow.printFailedVertices();
            break;
        case GFLOW_PRINT_MODE::CORRECTION_SETS:
            gflow.printCorrectionSets();
            gflow.printSummary();
            break;
        case GFLOW_PRINT_MODE::SUMMARY:
            gflow.printSummary();
            if (!gflow.isValid()) gflow.printFailedVertices();
            break;
        default:
            break;
    }

    return CMD_EXEC_DONE;
}

void ZXGGFlowCmd::usage(ostream &os) const {
    os << "Usage: ZXGGFlow [-All | -Summary | -Levels | -CorrectionSets]" << endl;
}

void ZXGGFlowCmd::help() const {
    cout << setw(15) << left << "ZXGGFlow: "
         << "calculate the generalized flow of current ZX-graph.\n";
}