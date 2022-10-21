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
extern size_t verbose;
// ZXGraph* zxGraph = new ZXGraph(0);

bool initZXCmd(){
    if(!(cmdMgr->regCmd("ZXMode", 3, new ZXModeCmd) && 
         cmdMgr->regCmd("ZXNew", 3, new ZXNewCmd) && 
         cmdMgr->regCmd("ZXRemove", 3, new ZXRemoveCmd) && 
         cmdMgr->regCmd("ZXCHeckout", 4, new ZXCHeckoutCmd) && 
         cmdMgr->regCmd("ZXCOPy", 5, new ZXCOPyCmd) && 
         cmdMgr->regCmd("ZXCOMpose", 5, new ZXCOMposeCmd) && 
         cmdMgr->regCmd("ZXTensor", 5, new ZXTensorCmd) && 
         cmdMgr->regCmd("ZXPrint", 3, new ZXPrintCmd) && 
         cmdMgr->regCmd("ZXGPrint", 4, new ZXGPrintCmd) && 
         cmdMgr->regCmd("ZXGTest", 4, new ZXGTestCmd) && 
         cmdMgr->regCmd("ZXGEdit", 4, new ZXGEditCmd) && 
         cmdMgr->regCmd("ZXGTRaverse", 5, new ZXGTraverseCmd) &&
         cmdMgr->regCmd("ZXGTSMap", 6, new ZXGTSMappingCmd) &&
         cmdMgr->regCmd("ZXGRead", 4, new ZXGReadCmd) &&
         cmdMgr->regCmd("ZXGWrite", 4, new ZXGWriteCmd)
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
//    ZXNew [(size_t id)]
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
            cerr << "Error: The id provided does not exist!!" << endl;
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
//    ZXCHeckout <(size_t id)>
//----------------------------------------------------------------------

CmdExecStatus
ZXCHeckoutCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXCHeckout" << endl;
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
            cerr << "Error: The id provided does not exist!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        }
        else zxGraphMgr->checkout2ZXGraph(id);
    }
    return CMD_EXEC_DONE;
}

void ZXCHeckoutCmd::usage(ostream &os) const{
    os << "Usage: ZXCHeckout <(size_t id)>" << endl;
}

void ZXCHeckoutCmd::help() const{
    cout << setw(15) << left << "ZXCHeckout: " << "Checkout to Graph <id> in ZXGraphMgr" << endl; 
}



//----------------------------------------------------------------------
//    ZXPrint [-Summary | -Focus | -Num]
//----------------------------------------------------------------------

CmdExecStatus
ZXPrintCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if(curCmd != ZXON) cout << "ZXMode: OFF" << endl;
    else{
        if(token.empty() || myStrNCmp("-Summary", token, 2) == 0){
            cout << "ZXMode: ON" << endl;
            zxGraphMgr->printZXGraphMgr();
        } 
        else if(myStrNCmp("-Focus", token, 2) == 0) zxGraphMgr->printGListItr();
        else if(myStrNCmp("-Num", token, 2) == 0) zxGraphMgr->printGraphListSize();
        else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token); 
    }
    return CMD_EXEC_DONE;
}

void ZXPrintCmd::usage(ostream &os) const{
    os << "Usage: ZXPrint [-Summary | -Focus | -Num]" << endl;
}

void ZXPrintCmd::help() const{
    cout << setw(15) << left << "ZXPrint: " << "print info in ZXGraphMgr" << endl; 
}



//----------------------------------------------------------------------
//    ZXCOPy [(size_t id)]
//----------------------------------------------------------------------

CmdExecStatus
ZXCOPyCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if(curCmd != ZXON) cout << "ZXMode: OFF" << endl;
    else{
        if(token.empty()) zxGraphMgr->copy(zxGraphMgr->getNextID());
        else{
            int id; bool isNum = myStr2Int(token, id);
            if(!isNum || id < 0){
                cerr << "Error: ZX-graph's id must be a nonnegative integer!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
            }
            else zxGraphMgr->copy(id);
        }
    }
    return CMD_EXEC_DONE;
}

void ZXCOPyCmd::usage(ostream &os) const{
    os << "Usage: ZXCOPy [(size_t id)]" << endl;
}

void ZXCOPyCmd::help() const{
    cout << setw(15) << left << "ZXCOPy: " << "copy a ZX-graph" << endl; 
}



//----------------------------------------------------------------------
//    ZXCOMpose <(size_t id)>
//----------------------------------------------------------------------

CmdExecStatus
ZXCOMposeCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if(curCmd != ZXON) cout << "ZXMode: OFF" << endl;
    else{
        if(token.empty()){
            cerr << "Error: the ZX-graph id you want to compose must provided!" << endl;
            return CmdExec::errorOption(CMD_OPT_MISSING, token);
        }
        else{
            int id; bool isNum = myStr2Int(token, id);
            if(!isNum || id < 0){
                cerr << "Error: ZX-graph's id must be a nonnegative integer!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
            }
            else if(!zxGraphMgr->isID(id)){
                cerr << "Error: The id provided does not exist!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);

            }
            else zxGraphMgr->compose(zxGraphMgr->findZXGraphByID(id));
        }
    }
    return CMD_EXEC_DONE;
}

void ZXCOMposeCmd::usage(ostream &os) const{
    os << "Usage: ZXCOMpose <(size_t id)>" << endl;
}

void ZXCOMposeCmd::help() const{
    cout << setw(15) << left << "ZXCOMpose: " << "compose a ZX-graph" << endl; 
}



//----------------------------------------------------------------------
//    ZXTensor <(size_t id)>
//----------------------------------------------------------------------

CmdExecStatus
ZXTensorCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if(curCmd != ZXON) cout << "ZXMode: OFF" << endl;
    else{
        if(token.empty()){
            cerr << "Error: the ZX-graph id you want to tensor must provided!" << endl;
            return CmdExec::errorOption(CMD_OPT_MISSING, token);
        }
        else{
            int id; bool isNum = myStr2Int(token, id);
            if(!isNum || id < 0){
                cerr << "Error: ZX-graph's id must be a nonnegative integer!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
            }
            else if(!zxGraphMgr->isID(id)){
                cerr << "Error: The id provided does not exist!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);

            }
            else zxGraphMgr->tensorProduct(zxGraphMgr->findZXGraphByID(id));
        }
    }
    return CMD_EXEC_DONE;
}

void ZXTensorCmd::usage(ostream &os) const{
    os << "Usage: ZXTensor <(size_t id)>" << endl;
}

void ZXTensorCmd::help() const{
    cout << setw(15) << left << "ZXTensor: " << "tensor a ZX-graph" << endl; 
}



//----------------------------------------------------------------------
//    ZXGTest [-GenerateCNOT | -Empty | -Valid]
//----------------------------------------------------------------------

CmdExecStatus
ZXGTestCmd::exec(const string &option){
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXTest." << endl;
        return CMD_EXEC_ERROR;
    }
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
   
    
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

void ZXGTestCmd::usage(ostream &os) const{
    os << "Usage: ZXGTest [-GenerateCNOT | -Empty | -Valid]" << endl;
}

void ZXGTestCmd::help() const{
    cout << setw(15) << left << "ZXGTest: " << "test ZX-graph structures and functions" << endl; 
}



//----------------------------------------------------------------------
//    ZXGPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges]
//----------------------------------------------------------------------

CmdExecStatus
ZXGPrintCmd::exec(const string &option){
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXPrint." << endl;
        return CMD_EXEC_ERROR;
    }
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

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

void ZXGPrintCmd::usage(ostream &os) const{
    os << "Usage: ZXGPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges]" << endl;
}

void ZXGPrintCmd::help() const{
    cout << setw(15) << left << "ZXGPrint: " << "print info in ZX-graph" << endl; 
}



//------------------------------------------------------------------------------------
//    ZXGEdit -RMVertex [i | <(size_t id(s))> ]
//            -RMEdge <(size_t id_s), (size_t id_t)>
//            -ADDVertex <(size_t id), (size_t qubit), (VertexType vt), [Phase phase]> 
//            -ADDInput <(size_t id), (size_t qubit)> 
//            -ADDOutput <(size_t id), (size_t qubit)>
//            -ADDEdge <(size_t id_s), (size_t id_t), (EdgeType et)>   
//------------------------------------------------------------------------------------

CmdExecStatus
ZXGEditCmd::exec(const string &option){
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXEdit." << endl;
        return CMD_EXEC_ERROR;
    }
    // check option
    vector<string> options;
    if(!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    if(options.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
    if(options.size() == 1) return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);

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
        if(options.size() != 4 && options.size() != 5){
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
        else{
            if(options.size() == 4) zxGraphMgr->getGraph()->addVertex(id, q, str2VertexType(options[3]));
            else{
                Phase phase;
                if(!phase.fromString(options[4])){
                    cerr << "Error: not a legal phase!!" << endl;
                    return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[4]);
                }
                zxGraphMgr->getGraph()->addVertex(id, q, str2VertexType(options[3]), phase);
            }
        }
    }
    else if(myStrNCmp("-ADDInput", action, 4) == 0){
        if(options.size() != 3){
            cerr << "Error: cmd options are missing / extra" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        int id, q;
        bool isNum = myStr2Int(options[1], id) && myStr2Int(options[2], q);
        if(!isNum || id < 0){
            cerr << "Error: vertex id / qubit invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        else zxGraphMgr->getGraph()->addInput(id, q);
    }
    else if(myStrNCmp("-ADDOutput", action, 4) == 0){
        if(options.size() != 3){
            cerr << "Error: cmd options are missing / extra" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        int id, q;
        bool isNum = myStr2Int(options[1], id) && myStr2Int(options[2], q);
        
        if(!isNum || id < 0){
            cerr << "Error: vertex id / qubit invalid" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[0]);
        }
        else zxGraphMgr->getGraph()->addOutput(id, q);
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

void ZXGEditCmd::usage(ostream &os) const{
    os << "Usage: ZXGEdit -RMVertex [i | <(size_t id(s))> ]" << endl;
    os << "               -RMEdge <(size_t id_s), (size_t id_t)>" << endl;
    os << "               -ADDVertex <(size_t id), (size_t qubit), (VertexType vt)> [Phase phase]" << endl;
    os << "               -ADDInput <(size_t id), (size_t qubit)>" << endl;
    os << "               -ADDOutput <(size_t id), (size_t qubit)>" << endl;
    os << "               -ADDEdge <(size_t id_s), (size_t id_t), (EdgeType et)>" << endl;
}

void ZXGEditCmd::help() const{
    cout << setw(15) << left << "ZXGEdit: " << "edit ZX-graph" << endl;
}





//----------------------------------------------------------------------
//    ZXGTRaverse
//----------------------------------------------------------------------
CmdExecStatus
ZXGTraverseCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexNoOption(option)) return CMD_EXEC_ERROR;
    zxGraphMgr->getGraph()->updateTopoOrder();
    return CMD_EXEC_DONE;
}

void ZXGTraverseCmd::usage(ostream &os) const{
    os << "Usage: ZXGTRaverse" << endl;
}

void ZXGTraverseCmd::help() const{
    cout << setw(15) << left << "ZXGTRaverse: " << "Traverse ZXGraph and update topological order" << endl; 
}



//----------------------------------------------------------------------
//    ZXGTSMapping
//----------------------------------------------------------------------
CmdExecStatus
ZXGTSMappingCmd::exec(const string &option){
    string token;
    if(!CmdExec::lexNoOption(option)) return CMD_EXEC_ERROR;
    zxGraphMgr->getGraph()->tensorMapping();
    return CMD_EXEC_DONE;
}

void ZXGTSMappingCmd::usage(ostream &os) const{
    os << "Usage: ZXGTSMapping" << endl;
}

void ZXGTSMappingCmd::help() const{
    cout << setw(15) << left << "ZXGTSMapping: " << "get tensor form of ZXGraph" << endl; 
}

//----------------------------------------------------------------------
//    ZXGRead 
//----------------------------------------------------------------------
CmdExecStatus
ZXGReadCmd::exec(const string &option){
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXGRead." << endl;
        return CMD_EXEC_ERROR;
    }
    // check option
    vector<string> options;
    
    if(!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    if(options.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");

    bool doReplace = false;
    string fileName;
    for (size_t i = 0, n = options.size(); i < n; ++i)
    {
        if (myStrNCmp("-Replace", options[i], 2) == 0)
        {
            if (doReplace)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            doReplace = true;
        }
        else
        {
            if (fileName.size())
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            fileName = options[i];
        }
    }

    if (doReplace){
        if(zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()){
            cerr << "Note: ZX-graph list is empty now. Create a new one." << endl;
            zxGraphMgr->addZXGraph(zxGraphMgr->getNextID());
            if(! zxGraphMgr->getGraph()->readZX(fileName)){
                cerr << "Error: The format in \"" << fileName << "\" has something wrong!!" << endl;
                return CMD_EXEC_ERROR;
            }
        }
        else{
            cerr << "Note: original zxGraph is replaced..." << endl;
            zxGraphMgr->getGraph()->reset();
            if(! zxGraphMgr->getGraph()->readZX(fileName)){
                cerr << "Error: The format in \"" << fileName << "\" has something wrong!!" << endl;
                return CMD_EXEC_ERROR;
            }
        }
    }
    else{
        zxGraphMgr->addZXGraph(zxGraphMgr->getNextID());
        if(! zxGraphMgr->getGraph()->readZX(fileName)){
            cerr << "Error: The format in \"" << fileName << "\" has something wrong!!" << endl;
            return CMD_EXEC_ERROR;
        }
    }
    return CMD_EXEC_DONE;
}

void ZXGReadCmd::usage(ostream &os) const{
    os << "Usage: ZXGRead <string filename> [-Replace]" << endl;
}

void ZXGReadCmd::help() const{
    cout << setw(15) << left << "ZXGRead: " << "read a ZXGraph" << endl; 
}

//----------------------------------------------------------------------
//    ZXGWrite <string Output.zx>
//----------------------------------------------------------------------
CmdExecStatus
ZXGWriteCmd::exec(const string &option)
{
   // check option
    if(curCmd != ZXON){
        cerr << "Error: ZXMODE is OFF now. Please turn ON before ZXEdit." << endl;
        return CMD_EXEC_ERROR;
    }
    string token;
    if (!CmdExec::lexSingleOption(option, token))
        return CMD_EXEC_ERROR;
    
    if(zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()){
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before ZXWrite." << endl;
        return CMD_EXEC_ERROR;
    }
    if(!zxGraphMgr->getGraph()->writeZX(token)){
        cerr << "Error: fail to write ZX-Graph to \"" << token << "\"!!" << endl;
        return CMD_EXEC_ERROR;
    }
    return CMD_EXEC_DONE;
}

void ZXGWriteCmd::usage(ostream &os) const
{
   os << "Usage: ZXGWrite <string Output.zx>" << endl;
}

void ZXGWriteCmd::help() const
{
   cout << setw(15) << left << "ZXGWrite: "
        << "write zx file\n";
}
