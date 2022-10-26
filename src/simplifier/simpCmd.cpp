/****************************************************************************
  FileName     [ simpCmd.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define simplifier package commands ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <iostream>
#include <iomanip>
#include "simpCmd.h"
#include "zxGraph.h"
#include "zxGraphMgr.h"
#include "simplify.h"
#include "util.h"

using namespace std;
extern size_t verbose;

bool initSimpCmd(){
    if(!(
         cmdMgr->regCmd("ZXGSimp", 4, new ZXGSimpCmd)
         
         )){
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//    ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -STCOpy | -HFusion | -HOPF | -PIVOT | -LComp | -INTERClifford ]
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
CmdExecStatus
ZXGSimpCmd::exec(const string &option){
    
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if(zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()){
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXPrint." << endl;
        return CMD_EXEC_ERROR;
    }
    else{
        Simplifier s(zxGraphMgr->getGraph());
        // Stats stats;
        if(token.empty() || myStrNCmp("-TOGraph", token, 3) == 0)   s.to_graph();
        else if(myStrNCmp("-TORGraph", token, 4) == 0)              s.to_rgraph();
        else if(myStrNCmp("-HRule", token, 2) == 0)                 s.hrule_simp();
        else if(myStrNCmp("-SPIderfusion", token, 3) == 0)          s.sfusion_simp();
        else if(myStrNCmp("-BIAlgebra", token, 3) == 0)             s.bialg_simp();
        else if(myStrNCmp("-IDRemoval", token, 3) == 0)             s.id_simp();
        else if(myStrNCmp("-STCOpy", token, 4) == 0)                s.copy_simp();
        else if(myStrNCmp("-HFusion", token, 2) == 0)               s.hfusion_simp();
        else if(myStrNCmp("-HOPF", token, 4) == 0)                  s.hopf_simp();
        else if(myStrNCmp("-PIVOT", token, 5) == 0)                 s.pivot_simp();
        else if(myStrNCmp("-LComp", token, 2) == 0)                 s.lcomp_simp();
        else if(myStrNCmp("-INTERClifford", token, 6) == 0)         s.interior_clifford_simp();
        else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    }
    return CMD_EXEC_DONE;
}

void ZXGSimpCmd::usage(ostream &os) const{
    os << "Usage: ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -STCOpy | -HFusion | -PIVOT | -LComp ]" << endl;
}

void ZXGSimpCmd::help() const{
    cout << setw(15) << left << "ZXGSimp: " << "perform simplification strategies for ZX-graph" << endl; 
}

