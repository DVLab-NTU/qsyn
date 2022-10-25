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

//----------------------------------------------------------------------
//    ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -PICOPY | -HFusion | -PIVOT | -LComp ]
//----------------------------------------------------------------------
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
        if(token.empty() || myStrNCmp("-TOGraph", token, 3) == 0) s.to_graph();
        else if(myStrNCmp("-TORGraph", token, 4) == 0) s.to_rgraph();
        else if(myStrNCmp("-HRule", token, 2) == 0){
            s.setRule(new HRule());
            s.hadamard_simp();
        }
        else if(myStrNCmp("-SPIderfusion", token, 3) == 0){
            s.setRule(new SpiderFusion());
            s.simp();
        }
        else if(myStrNCmp("-BIAlgebra", token, 3) == 0){
            s.setRule(new Bialgebra());
            s.simp();
        }
        else if(myStrNCmp("-IDRemoval", token, 3) == 0){
            s.setRule(new IdRemoval());
            s.simp();
        }
        else if(myStrNCmp("-STCOpy", token, 4) == 0){
            s.setRule(new StateCopy());
            s.simp();
        }
        else if(myStrNCmp("-HFusion", token, 2) == 0){
            s.setRule(new HboxFusion());
            s.simp();
        }
        else if(myStrNCmp("-PIVOT", token, 5) == 0){
            s.setRule(new Pivot());
            s.simp();
        }
        else if(myStrNCmp("-LComp", token, 2) == 0){
            s.setRule(new LComp());
            s.simp();
        }
        else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    }
    return CMD_EXEC_DONE;
}

void ZXGSimpCmd::usage(ostream &os) const{
    os << "Usage: ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -PICOPY | -HFusion | -PIVOT | -LComp ]" << endl;
}

void ZXGSimpCmd::help() const{
    cout << setw(15) << left << "ZXGSimp: " << "perform simplification strategies for ZX-graph" << endl; 
}

