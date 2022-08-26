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
#include "zxGraph.h"
#include "util.h"

using namespace std;
ZXGraph* zxGraph = new ZXGraph(0);

bool initZXCmd(){
    if(!(cmdMgr->regCmd("ZXPrint", 3, new ZXPrintCmd) && 
         cmdMgr->regCmd("ZXTest", 3, new ZXTestCmd) && 
         cmdMgr->regCmd("ZXEdit", 3, new ZXEditCmd)
         )){
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

enum ZXCmdState{
    // Order matters! Do not change the order!!
    ZXINIT,
    ZXREAD,
    ZXTEST,
    // dummy end
    ZXCMDTOT
};

static ZXCmdState curCmd = ZXINIT;

//----------------------------------------------------------------------
//    ZXTest [-GenerateCNOT | -Empty | -Valid]
//----------------------------------------------------------------------

CmdExecStatus
ZXTestCmd::exec(const string &option){
    // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

   if(token.empty() || myStrNCmp("-GenerateCNOT", token, 2) == 0) zxGraph->generateCNOT();
   else if(myStrNCmp("-Empty", token, 2) == 0){
    if(zxGraph->isEmpty()) cout << "This graph is empty!" << endl;
    else cout << "This graph is not empty!" << endl;
   }
   else if(myStrNCmp("-Valid", token, 2) == 0){
    if(zxGraph->isValid()) cout << "This graph is valid!" << endl;
    else cout << "This graph is invalid!" << endl;
   }
   else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   return CMD_EXEC_DONE;
}

void ZXTestCmd::usage(ostream &os) const{
    os << "Usage: ZXTest [-GenerateCNOT | -Empty | -Valid]" << endl;
}

void ZXTestCmd::help() const{
    cout << setw(15) << left << "ZXTest: " << "test ZX-graph structures and functions" << endl; 
}


//----------------------------------------------------------------------
//    ZXPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges]
//----------------------------------------------------------------------

CmdExecStatus
ZXPrintCmd::exec(const string &option){
   // check option
   string token;
   if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

   if(token.empty() || myStrNCmp("-Summary", token, 2) == 0) zxGraph->printGraph();
   else if (myStrNCmp("-Inputs", token, 2) == 0) zxGraph->printInputs();
   else if (myStrNCmp("-Outputs", token, 2) == 0) zxGraph->printOutputs();
   else if (myStrNCmp("-Vertices", token, 2) == 0) zxGraph->printVertices();
   else if (myStrNCmp("-Edges", token, 2) == 0) zxGraph->printEdges();
   else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   return CMD_EXEC_DONE;
}

void ZXPrintCmd::usage(ostream &os) const{
    os << "Usage: ZXPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges]" << endl;
}

void ZXPrintCmd::help() const{
    cout << setw(15) << left << "ZXPrint: " << "print ZX-graph" << endl; 
}

//----------------------------------------------------------------------
//    ZXEdit [-RMVertex <id> | -ADDVertex <id>]
//----------------------------------------------------------------------

CmdExecStatus
ZXEditCmd::exec(const string &option){
    // check option
    vector<string> options;
    if(!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    if(options.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
    if(options.size() == 1) return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);

    string action = options[0];
    if (myStrNCmp("-RMVertex", action, 3) == 0){
        for(size_t v = 1; v < options.size(); v++){
            int id;
            bool isNum = myStr2Int(options[v], id);
            if(options[1] == "i" && options.size() == 2) zxGraph->removeIsolatedVertices();
            else if(!isNum || id < 0){
                cerr << "Error: qubit id invalid" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[v]);
            }
            else{
                if(zxGraph->isId(id)) zxGraph->removeVertexById(id);
                else return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[v]);
            }
        }
    }
    
}

void ZXEditCmd::usage(ostream &os) const{
    os << "Usage: ZXEdit [-RMVertex <id> | -ADDVertex <id>]" << endl;
}

void ZXEditCmd::help() const{
    cout << setw(15) << left << "ZXEdit: " << "edit ZX-graph" << endl;
}