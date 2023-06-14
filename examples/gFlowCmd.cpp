/****************************************************************************
  FileName     [ gFlowCmd.cpp ]
  PackageName  [ gflow ]
  Synopsis     [ Define gflow package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "gFlowCmd.h"

#include <cstddef>   // for size_t
#include <iomanip>   // for ostream
#include <iostream>  // for ostream
#include <string>    // for string

#include "cmdMacros.h"   // for CMD_N_OPTS_AT_MOST_OR_RETURN
#include "gFlow.h"       // for GFlow
#include "zxGraphMgr.h"  // for ZXGraphMgr

using namespace std;

extern ZXGraphMgr *zxGraphMgr;
extern size_t verbose;

bool initGFlowCmd() {
    if (!cmdMgr->regCmd("ZXGGFlow", 5, make_unique<ZXGGFlowCmd>())) {
        cerr << "Registering \"gflow\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZXGGFlow [-All | -Summary | -Levels | -CorrectionSets] [-Disjoint]
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
    vector<string> options;
    if (!lexOptions(option, options)) return CMD_EXEC_ERROR;

    if (zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()) {
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXGGFlow." << endl;
        return CMD_EXEC_ERROR;
    }
    GFlow gflow(zxGraphMgr->getGraph());

    CMD_N_OPTS_AT_MOST_OR_RETURN(options, 2);

    GFLOW_PRINT_MODE mode = GFLOW_PRINT_MODE::SUMMARY;
    bool doDisjoint = false;
    bool doMode = false;

    for (size_t i = 0; i < options.size(); ++i) {
        if (myStrNCmp("-All", options[i], 2) == 0) {
            if (doMode) return errorOption(CMD_OPT_EXTRA, options[i]);
            mode = GFLOW_PRINT_MODE::ALL;
            doMode = true;
        } else if (myStrNCmp("-Levels", options[i], 2) == 0) {
            if (doMode) return errorOption(CMD_OPT_EXTRA, options[i]);
            mode = GFLOW_PRINT_MODE::LEVELS;
            doMode = true;
        } else if (myStrNCmp("-Correctionsets", options[i], 2) == 0) {
            if (doMode) return errorOption(CMD_OPT_EXTRA, options[i]);
            mode = GFLOW_PRINT_MODE::CORRECTION_SETS;
            doMode = true;
        } else if (myStrNCmp("-Summary", options[i], 2) == 0) {
            if (doMode) return errorOption(CMD_OPT_EXTRA, options[i]);
            mode = GFLOW_PRINT_MODE::SUMMARY;
            doMode = true;
        } else if (myStrNCmp("-Disjoint", options[i], 2) == 0) {
            if (doDisjoint) return errorOption(CMD_OPT_EXTRA, options[i]);
            doDisjoint = true;
            gflow.doIndependentLayers(true);
        } else {
            return errorOption(CMD_OPT_ILLEGAL, options[i]);
        }
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
            gflow.printXCorrectionSets();
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

void ZXGGFlowCmd::usage() const {
    cout << "Usage: ZXGGFlow [-All | -Summary | -Levels | -CorrectionSets] [-Disjoint]" << endl;
}

void ZXGGFlowCmd::summary() const {
    cout << setw(15) << left << "ZXGGFlow: "
         << "calculate the generalized flow of current ZX-graph\n";
}