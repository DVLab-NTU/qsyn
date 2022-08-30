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
#include "zxGraphMgr.h"
#include "util.h"

using namespace std;

extern ZXGraphMgr *zxGraphMgr;
// ZXGraph* zxGraph = new ZXGraph(0);

bool initZXCmd(){
    if(!(cmdMgr->regCmd("ZXMode", 3, new ZXModeCmd) && 
         cmdMgr->regCmd("ZXNew", 3, new ZXNewCmd) && 
         cmdMgr->regCmd("ZXRemove", 3, new ZXRemoveCmd) && 
         cmdMgr->regCmd("ZXCheckout", 3, new ZXCheckoutCmd) && 
         cmdMgr->regCmd("ZXPrint", 3, new ZXPrintCmd) && 
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
    ZXOFF,
    ZXON,
    // dummy end
    ZXCMDTOT
};

static ZXCmdState curCmd = ZXOFF;

//----------------------------------------------------------------------
//    ZXMode [-ON | -OFF | -Reset | -Print]
//----------------------------------------------------------------------

CmdExecStatus
ZXModeCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if(zxGraphMgr != 0){}
    zxGraphMgr = new ZXGraphMgr;
    if(token.empty() || myStrNCmp("-Print", token, 2) == 0){
        if(curCmd == ZXON) cout << "ZX ON" << endl;
        else if (curCmd == ZXOFF) cout << "ZX OFF" << endl;
        else cout << "Error: curCmd loading fail... exiting" << endl;
    }
    else if(myStrNCmp("-ON", token, 3) == 0){
        if(curCmd == ZXON) cout << "Error: ZXMODE is already ON" << endl;
        else{
            curCmd = ZXON;
            cout << "ZXMODE turn ON!" << endl;
        }
    }
    else if(myStrNCmp("-OFF", token, 4) == 0){
        if(curCmd == ZXON){
            curCmd = ZXOFF;
            delete zxGraphMgr;
            zxGraphMgr = 0;
            cout << "ZXMODE turn OFF!" << endl;
        }
        else cout << "Error: ZXMODE is already OFF" << endl;
    }
    else if(myStrNCmp("-Reset", token, 2) == 0){
        if(curCmd == ZXON){
            zxGraphMgr->reset();
            cout << "ZX is successfully RESET!" << endl;
        } 
        else cout << "Error: ZXMODE OFF, turn ON before RESET" << endl;
    }
    else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    return CMD_EXEC_DONE;
}

void ZXModeCmd::usage(ostream &os) const{
    os << "Usage: ZXMode [-On | -Off | -Reset | -Print]" << endl;
}

void ZXModeCmd::help() const{
    cout << setw(15) << left << "ZXMode: " << "check out to ZX-graph mode" << endl; 
}


//----------------------------------------------------------------------
//    ZXNew [size_t id]
//----------------------------------------------------------------------

CmdExecStatus
ZXNewCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXNew" << endl;
        return CMD_EXEC_ERROR;
    }
    if(token.empty()) zxGraphMgr->addZXGraph(zxGraphMgr->getNextID());
    else{
        int id; bool isNum = myStr2Int(token, id);
        if(!isNum || id < 0){
            cerr << "Error: ZX-graph's id must be a nonnegative integer!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        }
        else zxGraphMgr->addZXGraph(id);
    }
    return CMD_EXEC_DONE;
}

void ZXNewCmd::usage(ostream &os) const{
    os << "Usage: ZXNew [size_t id]" << endl;
}

void ZXNewCmd::help() const{
    cout << setw(15) << left << "ZXNew: " << "new ZX-graph to ZXGraphMgr" << endl; 
}


//----------------------------------------------------------------------
//    ZXRemove <(size_t id)>
//----------------------------------------------------------------------

CmdExecStatus
ZXRemoveCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXRemove" << endl;
        return CMD_EXEC_ERROR;
    }
    if(token.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
    else{
        int id; bool isNum = myStr2Int(token, id);
        if(!isNum){
            cerr << "Error: ZX-graph's id must be a nonnegative integer!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        }
        if(!zxGraphMgr->isID(id)){
            cerr << "Error: The id provided is not exist!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        }
        else zxGraphMgr->removeZXGraph(id);
    }
    return CMD_EXEC_DONE;
}

void ZXRemoveCmd::usage(ostream &os) const{
    os << "Usage: ZXRemove <(size_t id)>" << endl;
}

void ZXRemoveCmd::help() const{
    cout << setw(15) << left << "ZXRemove: " << "remove ZX-graph from ZXGraphMgr" << endl; 
}


//----------------------------------------------------------------------
//    ZXCheckout <(size_t id)>
//----------------------------------------------------------------------

CmdExecStatus
ZXCheckoutCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXCheckout" << endl;
        return CMD_EXEC_ERROR;
    }
    if(token.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
    else{
        int id; bool isNum = myStr2Int(token, id);
        if(!isNum){
            cerr << "Error: ZX-graph's id must be a nonnegative integer!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        }
        if(!zxGraphMgr->isID(id)){
            cerr << "Error: The id provided is not exist!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        }
        else zxGraphMgr->checkout2ZXGraph(id);
    }
    return CMD_EXEC_DONE;
}

void ZXCheckoutCmd::usage(ostream &os) const{
    os << "Usage: ZXCheckout <(size_t id)>" << endl;
}

void ZXCheckoutCmd::help() const{
    cout << setw(15) << left << "ZXCheckout: " << "Checkout to Graph <id> in ZXGraphMgr" << endl; 
}




//----------------------------------------------------------------------
//    ZXTest [-GenerateCNOT | -Empty | -Valid]
//----------------------------------------------------------------------

CmdExecStatus
ZXTestCmd::exec(const string &option){
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXTest." << endl;
        return CMD_EXEC_ERROR;
    }
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
   
    // TODO: check existence
   if(zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()){
    cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXTest." << endl;
    return CMD_EXEC_ERROR;
   }
   else{
    if(token.empty() || myStrNCmp("-GenerateCNOT", token, 2) == 0) zxGraphMgr->getGraph()->generateCNOT();
    else if(myStrNCmp("-Empty", token, 2) == 0){
        if(zxGraphMgr->getGraph()->isEmpty()) cout << "This graph is empty!" << endl;
        else cout << "This graph is not empty!" << endl;
    }
    else if(myStrNCmp("-Valid", token, 2) == 0){
        if(zxGraphMgr->getGraph()->isValid()) cout << "This graph is valid!" << endl;
        else cout << "This graph is invalid!" << endl;
    }
    else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
   }
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
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXPrint." << endl;
        return CMD_EXEC_ERROR;
    }
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

   

   // TODO: check existence
   if(zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()){
    cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXPrint." << endl;
    return CMD_EXEC_ERROR;
   }

   if(token.empty() || myStrNCmp("-Summary", token, 2) == 0) zxGraphMgr->getGraph()->printGraph();
   else if (myStrNCmp("-Inputs", token, 2) == 0) zxGraphMgr->getGraph()->printInputs();
   else if (myStrNCmp("-Outputs", token, 2) == 0) zxGraphMgr->getGraph()->printOutputs();
   else if (myStrNCmp("-Vertices", token, 2) == 0) zxGraphMgr->getGraph()->printVertices();
   else if (myStrNCmp("-Edges", token, 2) == 0) zxGraphMgr->getGraph()->printEdges();
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
//    ZXEdit -RMVertex <id(s)> 
//           -RMEdge <id_s, id_t>
//           -ADDVertex <id, qubit, VertexType> 
//           -ADDInput <id, qubit, VertexType> 
//           -ADDOutput <id, qubit, VertexType>
//           -ADDEdge <id_s, id_t, EdgeType>      
//------------------------------------------------------------------------------------

CmdExecStatus
ZXEditCmd::exec(const string &option){
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXEdit." << endl;
        return CMD_EXEC_ERROR;
    }
    // check option
    vector<string> options;
    if(!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    if(options.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
    if(options.size() == 1) return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);

    

    // TODO: check existence
   if(zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()){
    cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXEdit." << endl;
    return CMD_EXEC_ERROR;
   }

    string action = options[0];
    if (myStrNCmp("-RMVertex", action, 4) == 0){
        for(size_t v = 1; v < options.size(); v++){
            int id;
            bool isNum = myStr2Int(options[v], id);
            if(options[1] == "i" && options.size() == 2) zxGraphMgr->getGraph()->removeIsolatedVertices();
            else if(!isNum || id < 0){
                cerr << "Error: qubit id invalid" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[v]);
            }
            else{
                if(zxGraphMgr->getGraph()->isId(id)) zxGraphMgr->getGraph()->removeVertexById(id);
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
            cerr << "Error: id_s / id_t is invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        else zxGraphMgr->getGraph()->removeEdgeById(id_s, id_t);
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
        else zxGraphMgr->getGraph()->addVertex(id, q, str2VertexType(options[3]));
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
        else zxGraphMgr->getGraph()->addInput(id, q, str2VertexType(options[3]));
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
        else zxGraphMgr->getGraph()->addOutput(id, q, str2VertexType(options[3]));
    }
    else if(myStrNCmp("-ADDEdge", action, 4) == 0){
        if(options.size() != 4){
            cerr << "Error: cmd options are missing / extra" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        int id_s, id_t;
        bool isNum = myStr2Int(options[1], id_s) && myStr2Int(options[2], id_t);
        bool isEdge = (options[3] == "SIMPLE") || (options[3] == "HADAMARD") ;
        if(!isNum || id_s < 0 || id_t < 0){
            cerr << "Error: id_s / id_t is invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        else if(!isEdge){
            cerr << "Error: EdgeType is invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
        }
        else zxGraphMgr->getGraph()->addEdgeById(id_s, id_t, str2EdgeType(options[3]));
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
    os << "              -ADDEdge <id_s, id_t, EdgeType>  " << endl;
}

void ZXEditCmd::help() const{
    cout << setw(15) << left << "ZXEdit: " << "edit ZX-graph" << endl;
}

