/****************************************************************************
  FileName     [ zxCmd.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define basic zx package commands ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxCmd.h"

#include <cassert>
#include <iomanip>
#include <iostream>

#include "util.h"
#include "zxGraph.h"
#include "zxGraphMgr.h"
#include "textFormat.h"

namespace TF = TextFormat;

using namespace std;

extern ZXGraphMgr *zxGraphMgr;
extern size_t verbose;
// ZXGraph* zxGraph = new ZXGraph(0);



bool initZXCmd() {
    if (!(cmdMgr->regCmd("ZXMode", 3, new ZXModeCmd) &&
          cmdMgr->regCmd("ZXNew", 3, new ZXNewCmd) &&
          cmdMgr->regCmd("ZXRemove", 3, new ZXRemoveCmd) &&
          cmdMgr->regCmd("ZXCHeckout", 4, new ZXCHeckoutCmd) &&
          cmdMgr->regCmd("ZXCOPy", 5, new ZXCOPyCmd) &&
          cmdMgr->regCmd("ZXCOMpose", 5, new ZXCOMposeCmd) &&
          cmdMgr->regCmd("ZXTensor", 3, new ZXTensorCmd) &&
          cmdMgr->regCmd("ZXPrint", 3, new ZXPrintCmd) &&
          cmdMgr->regCmd("ZXGPrint", 4, new ZXGPrintCmd) &&
          cmdMgr->regCmd("ZXGTest", 4, new ZXGTestCmd) &&
          cmdMgr->regCmd("ZXGEdit", 4, new ZXGEditCmd) &&
          cmdMgr->regCmd("ZXGADJoint", 6, new ZXGAdjointCmd) &&
          cmdMgr->regCmd("ZXGASsign", 5, new ZXGAssignCmd) &&
          cmdMgr->regCmd("ZXGTRaverse", 5, new ZXGTraverseCmd) &&
          cmdMgr->regCmd("ZX2TS", 5, new ZX2TSCmd) &&
          cmdMgr->regCmd("ZXGRead", 4, new ZXGReadCmd) &&
          cmdMgr->regCmd("ZXGWrite", 4, new ZXGWriteCmd))) {
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

enum ZXCmdState {
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
ZXModeCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if (zxGraphMgr != 0) {
    }
    zxGraphMgr = new ZXGraphMgr;
    if (token.empty() || myStrNCmp("-Print", token, 2) == 0) {
        if (curCmd == ZXON)
            cout << "ZX ON" << endl;
        else if (curCmd == ZXOFF)
            cout << "ZX OFF" << endl;
        else
            cout << "Error: curCmd loading fail... exiting" << endl;
    } else if (myStrNCmp("-ON", token, 3) == 0) {
        if (curCmd == ZXON)
            cout << "Error: ZXMODE is already ON" << endl;
        else {
            curCmd = ZXON;
            cout << "ZXMODE turn ON!" << endl;
        }
    } else if (myStrNCmp("-OFF", token, 4) == 0) {
        if (curCmd == ZXON) {
            curCmd = ZXOFF;
            delete zxGraphMgr;
            zxGraphMgr = 0;
            cout << "ZXMODE turn OFF!" << endl;
        } else
            cout << "Error: ZXMODE is already OFF" << endl;
    } else if (myStrNCmp("-Reset", token, 2) == 0) {
        if (curCmd == ZXON) {
            zxGraphMgr->reset();
            cout << "ZX is successfully RESET!" << endl;
        } else
            cout << "Error: ZXMODE OFF, turn ON before RESET" << endl;
    } else
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    return CMD_EXEC_DONE;
}

void ZXModeCmd::usage(ostream &os) const {
    os << "Usage: ZXMode [-On | -Off | -Reset | -Print]" << endl;
}

void ZXModeCmd::help() const {
    cout << setw(15) << left << "ZXMode: "
         << "check out to ZX-graph mode" << endl;
}



//----------------------------------------------------------------------
//    ZXNew [(size_t id)]
//----------------------------------------------------------------------
CmdExecStatus
ZXNewCmd::exec(const string &option) {

    ZX_CMD_ZXMODE_ON_OR_RETURN;

    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (token.empty())
        zxGraphMgr->addZXGraph(zxGraphMgr->getNextID());
    else {
        unsigned id;
        ZX_CMD_ID_VALID_OR_RETURN(token, id, "Graph");
        zxGraphMgr->addZXGraph(id);
    }
    return CMD_EXEC_DONE;
}

void ZXNewCmd::usage(ostream &os) const {
    os << "Usage: ZXNew [size_t id]" << endl;
}

void ZXNewCmd::help() const {
    cout << setw(15) << left << "ZXNew: "
         << "new ZX-graph to ZXGraphMgr" << endl;
}



//----------------------------------------------------------------------
//    ZXRemove <(size_t id)>
//----------------------------------------------------------------------
CmdExecStatus
ZXRemoveCmd::exec(const string &option) {
    //TODO - ZXRemove   
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    
    if (token.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
    else {
        unsigned id;
        ZX_CMD_ID_VALID_OR_RETURN(token, id, "Graph");
        ZX_CMD_GRAPH_ID_EXISTED_OR_RETURN(id);
        zxGraphMgr->removeZXGraph(id);
    }
    return CMD_EXEC_DONE;
}

void ZXRemoveCmd::usage(ostream &os) const {
    os << "Usage: ZXRemove <size_t id>" << endl;
}

void ZXRemoveCmd::help() const {
    cout << setw(15) << left << "ZXRemove: "
         << "remove a ZX-graph from ZXGraphMgr" << endl;
}



//----------------------------------------------------------------------
//    ZXCHeckout <(size_t id)>
//----------------------------------------------------------------------
CmdExecStatus
ZXCHeckoutCmd::exec(const string &option) {
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    
    if (token.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");
    else {
        unsigned id;
        ZX_CMD_ID_VALID_OR_RETURN(token, id, "Graph");
        ZX_CMD_GRAPH_ID_EXISTED_OR_RETURN(id);
        zxGraphMgr->checkout2ZXGraph(id);
    }
    return CMD_EXEC_DONE;
}

void ZXCHeckoutCmd::usage(ostream &os) const {
    os << "Usage: ZXCHeckout <(size_t id)>" << endl;
}

void ZXCHeckoutCmd::help() const {
    cout << setw(15) << left << "ZXCHeckout: "
         << "chec kout to Graph <id> in ZXGraphMgr" << endl;
}



//----------------------------------------------------------------------
//    ZXPrint [-Summary | -Focus | -Num]
//----------------------------------------------------------------------
CmdExecStatus
ZXPrintCmd::exec(const string &option) {
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if (token.empty() || myStrNCmp("-Summary", token, 2) == 0) {
        cout << "ZXMode: ON" << endl;
        zxGraphMgr->printZXGraphMgr();
    } 
    else if (myStrNCmp("-Focus", token, 2) == 0) zxGraphMgr->printGListItr();
    else if (myStrNCmp("-Num", token, 2) == 0) zxGraphMgr->printGraphListSize();
    else return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    return CMD_EXEC_DONE;
}

void ZXPrintCmd::usage(ostream &os) const {
    os << "Usage: ZXPrint [-Summary | -Focus | -Num]" << endl;
}

void ZXPrintCmd::help() const {
    cout << setw(15) << left << "ZXPrint: "
         << "print info in ZXGraphMgr" << endl;
}



//----------------------------------------------------------------------
//    ZXCOPy [(size_t id)]
//----------------------------------------------------------------------
CmdExecStatus
ZXCOPyCmd::exec(const string &option) {
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    // check option
    vector<string> options;
    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    
    CMD_N_OPTS_AT_MOST_OR_RETURN(options, 2);
    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXCOPy");

    if(options.size() == 2){
        bool doReplace = false;
        size_t id_idx = 0;
        for(size_t i = 0; i < options.size(); i++){
            if(myStrNCmp("-Replace", options[i], 2) == 0){
                doReplace = true;
                id_idx = 1 - i;
                break;
            } 
        }
        if(!doReplace) return CmdExec::errorOption(CMD_OPT_MISSING, "-Replace");
        unsigned id_g;
        ZX_CMD_ID_VALID_OR_RETURN(options[id_idx], id_g, "Graph");
        ZX_CMD_GRAPH_ID_EXISTED_OR_RETURN(id_g);
        zxGraphMgr->copy(id_g, false);
    }
    else if(options.size() == 1){
        unsigned id_g;
        ZX_CMD_ID_VALID_OR_RETURN(options[0], id_g, "Graph");
        ZX_CMD_GRAPH_ID_NOT_EXISTED_OR_RETURN(id_g);
        zxGraphMgr->copy(id_g);
    }
    else{
        zxGraphMgr->copy(zxGraphMgr->getNextID());
    }
    return CMD_EXEC_DONE;
}

void ZXCOPyCmd::usage(ostream &os) const {
    os << "Usage: ZXCOPy <size_t id> [-Replace]" << endl;
}

void ZXCOPyCmd::help() const {
    cout << setw(15) << left << "ZXCOPy: "
         << "copy a ZX-graph" << endl;
}



//----------------------------------------------------------------------
//    ZXCOMpose <size_t id>
//----------------------------------------------------------------------
CmdExecStatus
ZXCOMposeCmd::exec(const string &option) {
    //TODO - ZXCOMpose
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if (token.empty()) {
        cerr << "Error: the ZX-graph id you want to compose must provided!" << endl;
        return CmdExec::errorOption(CMD_OPT_MISSING, token);
    } else {
        unsigned id;
        ZX_CMD_ID_VALID_OR_RETURN(token, id, "Graph");
        ZX_CMD_GRAPH_ID_EXISTED_OR_RETURN(id);
        zxGraphMgr->getGraph()->compose(zxGraphMgr->findZXGraphByID(id));
    }
    return CMD_EXEC_DONE;
}

void ZXCOMposeCmd::usage(ostream &os) const {
    os << "Usage: ZXCOMpose <size_t id>" << endl;
}

void ZXCOMposeCmd::help() const {
    cout << setw(15) << left << "ZXCOMpose: "
         << "compose a ZX-graph" << endl;
}



//----------------------------------------------------------------------
//    ZXTensor <size_t id>
//----------------------------------------------------------------------
CmdExecStatus
ZXTensorCmd::exec(const string &option) {
    //TODO - ZXTensor
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
        if (token.empty()) {
            cerr << "Error: the ZX-graph id you want to tensor must provided!" << endl;
            return CmdExec::errorOption(CMD_OPT_MISSING, token);
        } 
        else {
            unsigned id;
            ZX_CMD_ID_VALID_OR_RETURN(token, id, "Graph");
            ZX_CMD_GRAPH_ID_EXISTED_OR_RETURN(id);
            zxGraphMgr->getGraph()->tensorProduct(zxGraphMgr->findZXGraphByID(id));
    
        }

    return CMD_EXEC_DONE;
}

void ZXTensorCmd::usage(ostream &os) const {
    os << "Usage: ZXTensor <size_t id>" << endl;
}

void ZXTensorCmd::help() const {
    cout << setw(15) << left << "ZXTensor: "
         << "tensor a ZX-graph" << endl;
}



//----------------------------------------------------------------------
//    ZXGTest [-GenerateCNOT | -Empty | -Valid | -GLike]
//----------------------------------------------------------------------
CmdExecStatus
ZXGTestCmd::exec(const string &option) {

    ZX_CMD_ZXMODE_ON_OR_RETURN;
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGTest");
    
    if (token.empty() || myStrNCmp("-GenerateCNOT", token, 2) == 0) {
        zxGraphMgr->getGraph()->generateCNOT();
        return CMD_EXEC_DONE;
    }

    if (myStrNCmp("-Empty", token, 2) == 0) {
        if (zxGraphMgr->getGraph()->isEmpty()) {
            cout << "The graph is empty!" << endl;
        } else {
            cout << "The graph is not empty!" << endl;
        }
        return CMD_EXEC_DONE;
    }
    
    if (myStrNCmp("-Valid", token, 2) == 0) {
        if (zxGraphMgr->getGraph()->isValid()) {
            cout << "The graph is valid!" << endl;
        } else {
            cout << "The graph is invalid!" << endl;
        }
        return CMD_EXEC_DONE;
    } 
    
    if (myStrNCmp("-GLike", token, 3) == 0) {
        if (zxGraphMgr->getGraph()->isGraphLike()) {
            cout << "The graph is graph-like!" << endl;
        } else {
            cout << "The graph is not graph-like!" << endl;
        }
        return CMD_EXEC_DONE;
    } 
    
    return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    
}

void ZXGTestCmd::usage(ostream &os) const {
    os << "Usage: ZXGTest [-GenerateCNOT | -Empty | -Valid | -GLike]" << endl;
}

void ZXGTestCmd::help() const {
    cout << setw(15) << left << "ZXGTest: "
         << "test ZX-graph structures and functions" << endl;
}



//--------------------------------------------------------------------------------
//    ZXGPrint [-Summary | -Inputs | -Outputs | -IO | -Vertices | -Edges ]
//--------------------------------------------------------------------------------
//REVIEW provides filters?
CmdExecStatus
ZXGPrintCmd::exec(const string &option) {

    ZX_CMD_ZXMODE_ON_OR_RETURN;
    // check option
    vector<string> options;
    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    // string token;
    // if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGPrint");
    
    if (options.empty() || myStrNCmp("-Summary", options[0], 2) == 0) zxGraphMgr->getGraph()->printGraph();
    else if (myStrNCmp("-Inputs", options[0], 2) == 0) zxGraphMgr->getGraph()->printInputs();
    else if (myStrNCmp("-Outputs", options[0], 2) == 0) zxGraphMgr->getGraph()->printOutputs();
    else if (myStrNCmp("-IO", options[0], 3) == 0) zxGraphMgr->getGraph()->printIO();
    else if (myStrNCmp("-Vertices", options[0], 2) == 0){
        if(options.size() == 1) zxGraphMgr->getGraph()->printVertices();
        else{
            vector<unsigned> candidates;
            for(size_t i = 1; i < options.size(); i++){
                unsigned id;
                if(myStr2Uns(options[i], id)) candidates.push_back(id);
            }
            zxGraphMgr->getGraph()->printVertices(candidates);
        }
    } 
    else if (myStrNCmp("-Edges", options[0], 2) == 0) zxGraphMgr->getGraph()->printEdges();
    else if (myStrNCmp("-Qubits", options[0], 2) == 0){
        vector<unsigned> candidates;
        for(size_t i = 1; i < options.size(); i++){
            unsigned id;
            if(myStr2Uns(options[i], id)) candidates.push_back(id);
        }
        zxGraphMgr->getGraph()->printQubits(candidates);
    }
    
    else return errorOption(CMD_OPT_ILLEGAL, options[0]);
    return CMD_EXEC_DONE;
}

void ZXGPrintCmd::usage(ostream &os) const {
    os << "Usage: ZXGPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges]" << endl;
}

void ZXGPrintCmd::help() const {
    cout << setw(15) << left << "ZXGPrint: "
         << "print info in ZX-graph" << endl;
}



//------------------------------------------------------------------------------------
//    ZXGEdit -RMVertex <-Isolated | (size_t id)... >
//            -RMEdge <(size_t id_s), (size_t id_t)> <-ALL | (EdgeType et)>
//            -ADDVertex <(size_t qubit), (VertexType vt), [Phase phase]>
//            -ADDInput <(size_t qubit)>
//            -ADDOutput <(size_t qubit)>
//            -ADDEdge <(size_t id_s), (size_t id_t), (EdgeType et)>
//------------------------------------------------------------------------------------
CmdExecStatus
ZXGEditCmd::exec(const string &option) {

    ZX_CMD_ZXMODE_ON_OR_RETURN;
    // check option
    vector<string> options;
    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    
    CMD_N_OPTS_AT_LEAST_OR_RETURN(options, 2);
    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGEdit");

    string action = options[0];
    if (myStrNCmp("-RMVertex", action, 4) == 0) {
        if (myStrNCmp("-Isolated", options[1], 2) == 0) {
            CMD_N_OPTS_AT_MOST_OR_RETURN(options, 2);
            zxGraphMgr->getGraph()->removeIsolatedVertices();
            cout << "Note: removing isolated vertices..." << endl;
            return CMD_EXEC_DONE; 
        }
        
        for (size_t i = 1; i < options.size(); i++) {
            unsigned id;
            if (!myStr2Uns(options[i], id)) {
                cerr << "Warning: invalid vertex ID (" << options[i] <<")!!" << endl;
                continue;
            }
            ZXVertex* v = zxGraphMgr->getGraph()->findVertexById(id);
            if (!v) {
                cerr << "Warning: Cannot find vertex with id " << id << " in the graph!!" << endl;
                continue;
            } 
            zxGraphMgr->getGraph()->removeVertex(v);
        }
        return CMD_EXEC_DONE;
    } 

    if (myStrNCmp("-RMEdge", action, 4) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 4);

        unsigned id_s, id_t;
        ZXVertex* vs;
        ZXVertex* vt;
        EdgeType etype;

        ZX_CMD_ID_VALID_OR_RETURN(options[1], id_s, "Vertex");
        ZX_CMD_ID_VALID_OR_RETURN(options[2], id_t, "Vertex");
        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id_s, vs);
        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id_t, vt);

        if (myStrNCmp("-ALL", options[3], 4) == 0) {
            zxGraphMgr->getGraph()->removeAllEdgesBetween(vs, vt);
        } else {
            ZX_CMD_EDGE_TYPE_VALID_OR_RETURN(options[3], etype);
            zxGraphMgr->getGraph()->removeEdge(vs, vt, etype);
        }

        return CMD_EXEC_DONE;
    } 
    
    if (myStrNCmp("-ADDVertex", action, 4) == 0) {
        CMD_N_OPTS_BETWEEN_OR_RETURN(options, 3, 4);

        int qid;
        VertexType vt;
        Phase phase;

        ZX_CMD_QUBIT_ID_VALID_OR_RETURN(options[1], qid);
        ZX_CMD_VERTEX_TYPE_VALID_OR_RETURN(options[2], vt);
        if (options.size() == 4) 
            ZX_CMD_PHASE_VALID_OR_RETURN(options[3], phase);
        
        zxGraphMgr->getGraph()->addVertex(qid, vt, phase);
        return CMD_EXEC_DONE;
    } 
    
    if (myStrNCmp("-ADDInput", action, 4) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 2);
        
        int qid;
        ZX_CMD_QUBIT_ID_VALID_OR_RETURN(options[1], qid);

        zxGraphMgr->getGraph()->addInput(qid);
        return CMD_EXEC_DONE;
    } 
    
    if (myStrNCmp("-ADDOutput", action, 4) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 2);

        int qid;
        ZX_CMD_QUBIT_ID_VALID_OR_RETURN(options[1], qid);

        zxGraphMgr->getGraph()->addOutput(qid);
        return CMD_EXEC_DONE;
    } 
    
    if (myStrNCmp("-ADDEdge", action, 4) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 4);

        unsigned id_s, id_t;
        ZXVertex* vs;
        ZXVertex* vt;
        EdgeType etype;

        ZX_CMD_ID_VALID_OR_RETURN(options[1], id_s, "Vertex");
        ZX_CMD_ID_VALID_OR_RETURN(options[2], id_t, "Vertex");

        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id_s, vs);
        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id_t, vt);

        ZX_CMD_EDGE_TYPE_VALID_OR_RETURN(options[3], etype);
        
        zxGraphMgr->getGraph()->addEdge(vs, vt, etype);
        return CMD_EXEC_DONE;
    } 
        
    return errorOption(CMD_OPT_ILLEGAL, action);
}

void ZXGEditCmd::usage(ostream &os) const {
    os << "Usage: ZXGEdit -RMVertex <-Isolated | (size_t id)... >" << endl;
    os << "               -RMEdge <(size_t id_s), (size_t id_t)> <-ALL | (EdgeType et)>" << endl;
    os << "               -ADDVertex <(size_t qubit), (VertexType vt), [Phase phase]>" << endl;
    os << "               -ADDInput <(size_t qubit)>" << endl;
    os << "               -ADDOutput <(size_t qubit)>" << endl;
    os << "               -ADDEdge <(size_t id_s), (size_t id_t), (EdgeType et)>" << endl;
}

void ZXGEditCmd::help() const {
    cout << setw(15) << left << "ZXGEdit: "
         << "edit ZX-graph" << endl;
}



//----------------------------------------------------------------------
//    ZXGTRaverse
//----------------------------------------------------------------------
CmdExecStatus
ZXGTraverseCmd::exec(const string &option) {
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    string token;
    if (!CmdExec::lexNoOption(option)) return CMD_EXEC_ERROR;
    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGTraverse");
    zxGraphMgr->getGraph()->updateTopoOrder();
    return CMD_EXEC_DONE;
}

void ZXGTraverseCmd::usage(ostream &os) const {
    os << "Usage: ZXGTRaverse" << endl;
}

void ZXGTraverseCmd::help() const {
    cout << setw(15) << left << "ZXGTRaverse: "
         << "traverse ZX-graph and update topological order of vertices" << endl;
}



//----------------------------------------------------------------------
//    ZX2TS
//----------------------------------------------------------------------
CmdExecStatus
ZX2TSCmd::exec(const string &option) {
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    
    if (!CmdExec::lexNoOption(option)) return CMD_EXEC_ERROR;
    zxGraphMgr->getGraph()->toTensor();
    return CMD_EXEC_DONE;
}

void ZX2TSCmd::usage(ostream &os) const {
    os << "Usage: ZX2TS" << endl;
}

void ZX2TSCmd::help() const {
    cout << setw(15) << left << "ZX2TS: "
         << "convert the ZX-graph to its corresponding tensor" << endl;
}



//----------------------------------------------------------------------
//    ZXGRead <string Input.(b)zx> [-BZX] [-Replace]
//----------------------------------------------------------------------
CmdExecStatus
ZXGReadCmd::exec(const string &option) {
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    // check option
    vector<string> options;

    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    if (options.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");

    bool doReplace = false;
    bool doBZX = false;
    size_t eraseIndexReplace = 0;
    size_t eraseIndexBZX = 0;
    string fileName;
    for (size_t i = 0, n = options.size(); i < n; ++i) {
        if (myStrNCmp("-Replace", options[i], 2) == 0) {
            if (doReplace)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            doReplace = true;
            eraseIndexReplace = i;
        } else if (myStrNCmp("-BZX", options[i], 4) == 0) {
            if (doBZX)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            doBZX = true;
            eraseIndexBZX = i;
        } else {
            if (fileName.size())
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            fileName = options[i];
        }
    }
    string replaceStr = options[eraseIndexReplace];
    string bzxStr = options[eraseIndexBZX];
    if (doReplace)
        options.erase(std::remove(options.begin(), options.end(), replaceStr), options.end());
    if (doBZX)
        options.erase(std::remove(options.begin(), options.end(), bzxStr), options.end());
    if (options.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, (eraseIndexBZX > eraseIndexReplace) ? bzxStr : replaceStr);

    if (doReplace) {
        if (zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()) {
            //FIXME - Use `ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN`
            cout << "Note: ZX-graph list is empty now. Create a new one." << endl;
            zxGraphMgr->addZXGraph(zxGraphMgr->getNextID());
            if (!zxGraphMgr->getGraph()->readZX(fileName, doBZX)) {
                cerr << "Error: The format in \"" << fileName << "\" has something wrong!!" << endl;
                return CMD_EXEC_ERROR;
            }
        } else {
            cerr << "Note: original zxGraph is replaced..." << endl;
            zxGraphMgr->getGraph()->reset();
            if (!zxGraphMgr->getGraph()->readZX(fileName, doBZX)) {
                cerr << "Error: The format in \"" << fileName << "\" has something wrong!!" << endl;
                return CMD_EXEC_ERROR;
            }
        }
    } else {
        zxGraphMgr->addZXGraph(zxGraphMgr->getNextID());
        if (!zxGraphMgr->getGraph()->readZX(fileName, doBZX)) {
            cerr << "Error: The format in \"" << fileName << "\" has something wrong!!" << endl;
            return CMD_EXEC_ERROR;
        }
    }
    return CMD_EXEC_DONE;
}

void ZXGReadCmd::usage(ostream &os) const {
    os << "Usage: ZXGRead <string Input.(b)zx> [-BZX] [-Replace]" << endl;
}

void ZXGReadCmd::help() const {
    cout << setw(15) << left << "ZXGRead: "
         << "read a ZXGraph" << endl;
}



//----------------------------------------------------------------------
//    ZXGWrite <string Output.(b)zx> [-Complete] [-BZX]
//----------------------------------------------------------------------
CmdExecStatus
ZXGWriteCmd::exec(const string &option) {
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    vector<string> options;

    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    if (options.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");

    bool doComplete = false;
    bool doBZX = false;
    size_t eraseIndexComplete = 0;
    size_t eraseIndexBZX = 0;
    string fileName;
    for (size_t i = 0, n = options.size(); i < n; ++i) {
        if (myStrNCmp("-Complete", options[i], 2) == 0) {
            if (doComplete)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            doComplete = true;
            eraseIndexComplete = i;
        } else if (myStrNCmp("-BZX", options[i], 4) == 0) {
            if (doBZX)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            doBZX = true;
            eraseIndexBZX = i;
        } else {
            if (fileName.size())
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            fileName = options[i];
        }
    }
    string completeStr = options[eraseIndexComplete];
    string bzxStr = options[eraseIndexBZX];
    if (doComplete)
        options.erase(std::remove(options.begin(), options.end(), completeStr), options.end());
    if (doBZX)
        options.erase(std::remove(options.begin(), options.end(), bzxStr), options.end());
    if (options.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, (eraseIndexBZX > eraseIndexComplete) ? bzxStr : completeStr);
    
    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXWrite");
    
    if (!zxGraphMgr->getGraph()->writeZX(fileName, doComplete, doBZX)) {
        cerr << "Error: fail to write ZX-Graph to \"" << fileName << "\"!!" << endl;
        return CMD_EXEC_ERROR;
    }
    return CMD_EXEC_DONE;
}

void ZXGWriteCmd::usage(ostream &os) const {
    os << "Usage: ZXGWrite <string Output.(b)zx> [-Complete] [-BZX]" << endl;
}

void ZXGWriteCmd::help() const {
    cout << setw(15) << left << "ZXGWrite: "
         << "write ZXFile\n";
}



//----------------------------------------------------------------------
//    ZXGASsign <size_t qubit> <I|O> <VertexType vt> <string Phase>
//----------------------------------------------------------------------
CmdExecStatus
ZXGAssignCmd::exec(const string &option) {
    // check option
    ZX_CMD_ZXMODE_ON_OR_RETURN;

    vector<string> options;
    if (!CmdExec::lexOptions(option, options))
        return CMD_EXEC_ERROR;

    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGASsign");
    
    CMD_N_OPTS_EQUAL_OR_RETURN(options, 4);
    int qid;
    ZX_CMD_QUBIT_ID_VALID_OR_RETURN(options[0], qid);

    bool isInput;
    if (options[1] == "I") {
        isInput = true;
    } else if (options[1] == "O") {
        isInput = false;
    } else {
        cerr << "Error: a boundary must be either \"I\" or \"O\"!!" << endl;
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[1]);
    }

    if (!(isInput ? zxGraphMgr->getGraph()->isInputQubit(qid) : zxGraphMgr->getGraph()->isOutputQubit(qid))) {
        cerr << "Error: the specified boundary does not exist!!" << endl;
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[1]);
    }

    VertexType vt;
    ZX_CMD_VERTEX_TYPE_VALID_OR_RETURN(options[2], vt);

    Phase phase;
    ZX_CMD_PHASE_VALID_OR_RETURN(options[3], phase);

    zxGraphMgr->getGraph()->assignBoundary(qid, (options[1] == "I"), vt, phase);
    return CMD_EXEC_DONE;
}

void ZXGAssignCmd::usage(ostream &os) const {
    os << "Usage: ZXGASsign <size_t qubit> <I|O> <VertexType vt> <string Phase>" << endl;
}

void ZXGAssignCmd::help() const {
    cout << setw(15) << left << "ZXGASsign: "
         << "assign an input/output vertex to specific qubit\n";
}



//----------------------------------------------------------------------
//    ZXGADJoint
//----------------------------------------------------------------------
CmdExecStatus
ZXGAdjointCmd::exec(const string &option) {
    ZX_CMD_ZXMODE_ON_OR_RETURN;
    if (!lexNoOption(option)) return CMD_EXEC_ERROR;
    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGAdjoint");
    
    zxGraphMgr->getGraph()->adjoint();
    return CMD_EXEC_DONE;
}

void ZXGAdjointCmd::usage(ostream &os) const {
    os << "Usage: ZXGADJoint" << endl;
}

void ZXGAdjointCmd::help() const {
    cout << setw(15) << left << "ZXGADJoint: "
         << "adjoint the current ZX-graph.\n";
}

