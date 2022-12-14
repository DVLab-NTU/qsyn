/****************************************************************************
  FileName     [ m2Cmd.cpp ]
  PackageName  [ m2 ]
  Synopsis     [ Define basic m2 package commands ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <iostream>
#include <iomanip>
#include "util.h"
#include "extractorCmd.h"
#include "extract.h"
#include "zxGraphMgr.h"

using namespace std;
extern size_t verbose;
extern int effLimit;
extern ZXGraphMgr *zxGraphMgr;

bool initExtractCmd()
{
   if (!(cmdMgr->regCmd("EXTract", 3, new ExtractCmd)
         ))
   {
      cerr << "Registering \"extract\" commands fails... exiting" << endl;
      return false;
   }
   return true;
}

CmdExecStatus
ExtractCmd::exec(const string &option) {    
    string token;
    // check option
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if(zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()){
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXPrint." << endl;
        return CMD_EXEC_ERROR;
    }
    else{
        zxGraphMgr->copy(zxGraphMgr->getNextID());
        Extractor ext(zxGraphMgr->getGraph());
        ext.cleanFrontier();
        // ext.removeGadget();
    }
    return CMD_EXEC_DONE;
}

void ExtractCmd::usage(ostream &os) const {
    os << "Usage: EXTract" << endl;
}

void ExtractCmd::help() const {
    cout << setw(15) << left << "EXTract: "
         << "extract the circuit from the ZX-graph" << endl;
}