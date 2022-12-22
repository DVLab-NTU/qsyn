/****************************************************************************
  FileName     [ latticeCmd.cpp ]
  PackageName  [ lattice ]
  Synopsis     [ Define lattice package commands ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <iostream>
#include <iomanip>
#include "latticeCmd.h"
#include "zxGraph.h"
#include "zxGraphMgr.h"
#include "lattice.h"
#include "util.h"

using namespace std;
extern size_t verbose;

bool initLTCmd(){
    if(!(
         cmdMgr->regCmd("LTS", 3, new LTCmd)
         
         )){
        cerr << "Registering \"lts\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------------------------------------------
//    LT [ -p ]
//------------------------------------------------------------------------------------------------------------------
CmdExecStatus
LTCmd::exec(const string &option){
    
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if(zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()){
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXPrint." << endl;
        return CMD_EXEC_ERROR;
    }
    else{
        LTContainer lt(0, 0);
        lt.generateLTC(zxGraphMgr->getGraph());
        if(token.empty()) lt.printLTC();
        
        // Stats stats;
        // if(token.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
        // else if(myStrNCmp("-BIAlgebra", token, 4) == 0)             s.bialgSimp();
        // else if(myStrNCmp("-STCOpy", token, 5) == 0)                s.copySimp();
        // else if(myStrNCmp("-HFusion", token, 3) == 0)               s.hfusionSimp();
        // // else if(myStrNCmp("-HOPF", token, 5) == 0)                  s.hopfSimp();
        // else if(myStrNCmp("-HRule", token, 3) == 0)                 s.hruleSimp();
        // else if(myStrNCmp("-IDRemoval", token, 4) == 0)             s.idSimp();
        // else if(myStrNCmp("-LComp", token, 3) == 0)                 s.lcompSimp();

        // // else if(myStrNCmp("-PIVOTBoundary", token, 7) == 0)         s.pivotBoundarySimp();
        // else if(myStrNCmp("-PIVOTGadget", token, 7) == 0)           s.pivotGadgetSimp();
        // else if(myStrNCmp("-PIVOT", token, 6) == 0)                 s.pivotSimp();
        // else if(myStrNCmp("-GADgetfusion", token, 4) == 0)          s.gadgetSimp();
        // else if(myStrNCmp("-SPIderfusion", token, 4) == 0)          s.sfusionSimp();

        // else if(myStrNCmp("-TOGraph", token, 4) == 0)               s.toGraph();
        // else if(myStrNCmp("-TORGraph", token, 5) == 0)              s.toRGraph();
        // else if(myStrNCmp("-INTERClifford", token, 7) == 0)          s.interiorCliffordSimp();
        // else if(myStrNCmp("-CLIFford", token, 5) == 0)              s.cliffordSimp();
        // else if(myStrNCmp("-FReduce", token, 3) == 0)               s.fullReduce();
        // else if(myStrNCmp("-SReduce", token, 3) == 0)               s.symbolicReduce();
        else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    }
    return CMD_EXEC_DONE;
}

void LTCmd::usage(ostream &os) const{
    // os << "Usage: ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -STCOpy | -HFusion | \n"
    //                    << "-HOPF | -PIVOT | -LComp | -INTERClifford | -PIVOTGadget | -PIVOTBoundary | -CLIFford | -FReduce | -SReduce]" << endl;
    os << "Usage: LTS [ -Print ]" << endl;
}

void LTCmd::help() const{
    cout << setw(15) << left << "ZXGSimp: " << "perform simplification strategies for ZX-graph" << endl; 
}

