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

//------------------------------------------------------------------------------------
//    ZXEdit -RMVertex <id> 
//           -ADDVertex <id, qubit, VertexType> 
//           -ADDInput <id, qubit, VertexType> 
//           -ADDOutput <id, qubit, VertexType>
//           -RMEdge <id_s, id_t>
//------------------------------------------------------------------------------------

CmdExecStatus
ZXEditCmd::exec(const string &option){
    // check option
    vector<string> options;
    if(!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    if(options.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
    if(options.size() == 1) return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);

    string action = options[0];
    if (myStrNCmp("-RMVertex", action, 4) == 0){
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
    else if(myStrNCmp("-RMEdge", action, 4) == 0){
        if(options.size() != 3){
            cerr << "Error: cmd options are missing / extra" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        int id_s, id_t;
        bool isNum = myStr2Int(options[1], id_s) && myStr2Int(options[2], id_t);
        if(!isNum || id_s < 0 || id_t < 0){
            cerr << "Error: id_s / id_t invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        else zxGraph->removeEdgeById(id_s, id_t);
    }
    else if(myStrNCmp("-ADDVertex", action, 4) == 0){
        if(options.size() != 4){
            cerr << "Error: cmd options are missing / extra" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        int id, q;
        bool isNum = myStr2Int(options[1], id) && myStr2Int(options[2], q);
        bool isVertexType = (options[3] == "Z") || (options[3] == "X") || (options[3] == "H_BOX");
        if(!isNum || id < 0){
            cerr << "Error: vertex id / qubit invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        else if(!isVertexType){
            cerr << "Error: vertex type invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
        }
        else zxGraph->addVertex(id, q, str2VertexType(options[3]));
    }
    else if(myStrNCmp("-ADDInput", action, 4) == 0){
        if(options.size() != 4){
            cerr << "Error: cmd options are missing / extra" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        int id, q;
        bool isNum = myStr2Int(options[1], id) && myStr2Int(options[2], q);
        bool isBoundary = (options[3] == "BOUNDARY");
        if(!isNum || id < 0){
            cerr << "Error: vertex id / qubit invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        else if(!isBoundary){
            cerr << "Error: VertexType is invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
        }
        else zxGraph->addInput(id, q, str2VertexType(options[3]));
    }
    else if(myStrNCmp("-ADDOutput", action, 4) == 0){
        if(options.size() != 4){
            cerr << "Error: cmd options are missing / extra" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        int id, q;
        bool isNum = myStr2Int(options[1], id) && myStr2Int(options[2], q);
        bool isBoundary = (options[3] == "BOUNDARY");
        if(!isNum || id < 0){
            cerr << "Error: vertex id / qubit invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        else if(!isBoundary){
            cerr << "Error: VertexType is invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
        }
        else zxGraph->addOutput(id, q, str2VertexType(options[3]));
    }
    else return CmdExec::errorOption(CMD_OPT_ILLEGAL, action);
    return CMD_EXEC_DONE;
}

void ZXEditCmd::usage(ostream &os) const{
    os << "Usage: ZXEdit -RMVertex <id>" << endl;
    os << "              -RMEdge <id_s, id_t> " << endl;
    os << "              -ADDInput <id, qubit, VertexType> " << endl;
    os << "              -ADDOutput <id, qubit, VertexType> " << endl;
    os << "              -ADDVertex <id, qubit, VertexType> " << endl;
}

void ZXEditCmd::help() const{
    cout << setw(15) << left << "ZXEdit: " << "edit ZX-graph" << endl;
}