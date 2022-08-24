/****************************************************************************
  FileName     [ zxCmd.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define basic zx package commands ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <iostream>
#include <iomanip>
#include "zxCmd.h"
#include "util.h"

using namespace std;

bool initZXCmd(){
    if(!(cmdMgr->regCmd("ZXPrint", 5, new ZXPrintCmd))){
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZXPrint [-Summary | -Netlist | -PI | -PO | -FLoating | -FECpairs]
//----------------------------------------------------------------------

CmdExecStatus
ZXPrintCmd::exec(const string &option){
    // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

   if(token.empty() || myStrNCmp("-Summary", token, 2) == 0) cout << "Print ZX Summary" << endl;
   else if (myStrNCmp("-Inputs", token, 2) == 0) cout << "Print ZX Graph inputs" << endl;
   else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   return CMD_EXEC_DONE;
}

void ZXPrintCmd::usage(ostream &os) const{
    os << "Usage: ZXPrint [-Summary | -Inputs]" << endl;
}

void ZXPrintCmd::help() const{
    cout << setw(15) << left << "ZXPrint: print ZX-graph" << endl; 
}