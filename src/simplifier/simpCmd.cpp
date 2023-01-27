/****************************************************************************
  FileName     [ simpCmd.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define simplifier package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "simpCmd.h"

#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>
#include <string>

#include "simplify.h"
#include "zxGraphMgr.h"

using namespace std;
extern size_t verbose;

bool initSimpCmd() {
    if (!(
            cmdMgr->regCmd("ZXGSimp", 4, new ZXGSimpCmd)

                )) {
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------------------------------------------
//    ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -STCOpy | -HFusion |
//             -HOPF | -PIVOT | -LComp | -INTERClifford | -PIVOTGadget | -PIVOTBoundary | -CLIFford | -FReduce | -SReduce]
//------------------------------------------------------------------------------------------------------------------
CmdExecStatus
ZXGSimpCmd::exec(const string &option) {
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()) {
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXPrint." << endl;
        return CMD_EXEC_ERROR;
    } else {
        Simplifier s(zxGraphMgr->getGraph());
        // Stats stats;
        if (token.empty())
            return CmdExec::errorOption(CMD_OPT_MISSING, "");
        else if (myStrNCmp("-BIAlgebra", token, 4) == 0)
            s.bialgSimp();
        else if (myStrNCmp("-STCOpy", token, 5) == 0)
            s.copySimp();
        else if (myStrNCmp("-HFusion", token, 3) == 0)
            s.hfusionSimp();
        // else if(myStrNCmp("-HOPF", token, 5) == 0)                  s.hopfSimp();
        else if (myStrNCmp("-HRule", token, 3) == 0)
            s.hruleSimp();
        else if (myStrNCmp("-IDRemoval", token, 4) == 0)
            s.idSimp();
        else if (myStrNCmp("-LComp", token, 3) == 0)
            s.lcompSimp();

        // else if(myStrNCmp("-PIVOTBoundary", token, 7) == 0)         s.pivotBoundarySimp();
        else if (myStrNCmp("-PIVOTGadget", token, 7) == 0)
            s.pivotGadgetSimp();
        else if (myStrNCmp("-PIVOT", token, 6) == 0)
            s.pivotSimp();
        else if (myStrNCmp("-GADgetfusion", token, 4) == 0)
            s.gadgetSimp();
        else if (myStrNCmp("-SPIderfusion", token, 4) == 0)
            s.sfusionSimp();

        else if (myStrNCmp("-TOGraph", token, 4) == 0)
            s.toGraph();
        else if (myStrNCmp("-TORGraph", token, 5) == 0)
            s.toRGraph();
        else if (myStrNCmp("-INTERClifford", token, 7) == 0)
            s.interiorCliffordSimp();
        else if (myStrNCmp("-CLIFford", token, 5) == 0)
            s.cliffordSimp();
        else if (myStrNCmp("-FReduce", token, 3) == 0)
            s.fullReduce();
        else if (myStrNCmp("-SReduce", token, 3) == 0)
            s.symbolicReduce();
        else
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    }
    return CMD_EXEC_DONE;
}

void ZXGSimpCmd::usage() const {
    cout << "Usage: ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -STCOpy | -HFusion | \n"
       << "-HOPF | -PIVOT | -LComp | -INTERClifford | -PIVOTGadget | -CLIFford | -FReduce | -SReduce]" << endl;
}

void ZXGSimpCmd::help() const {
    cout << setw(15) << left << "ZXGSimp: "
         << "perform simplification strategies for ZX-graph" << endl;
}
