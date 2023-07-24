/****************************************************************************
  FileName     [ zxCmd.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define zx package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxCmd.h"

#include <cassert>
#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>
#include <string>

#include "cmdMacros.h"   // for CMD_N_OPTS_EQUAL_OR_RETURN, CMD_N_OPTS_AT_LE...
#include "phase.h"       // for Phase
#include "textFormat.h"  // for TextFormat
#include "zxGraphMgr.h"  // for ZXGraph, ZXVertex

namespace TF = TextFormat;

using namespace std;

ZXGraphMgr zxGraphMgr{"ZXGraph"};
using namespace ArgParse;
extern size_t verbose;

unique_ptr<ArgParseCmdType> ZXCHeckoutCmd();
unique_ptr<ArgParseCmdType> ZXNewCmd();
unique_ptr<ArgParseCmdType> ZXResetCmd();
unique_ptr<ArgParseCmdType> ZXDeleteCmd();
unique_ptr<ArgParseCmdType> ZXPrintCmd();
unique_ptr<ArgParseCmdType> ZXCopyCmd();
unique_ptr<ArgParseCmdType> ZXComposeCmd();
unique_ptr<ArgParseCmdType> ZXTensorCmd();
unique_ptr<ArgParseCmdType> ZXGTraverseCmd();
unique_ptr<ArgParseCmdType> ZX2TSCmd();
unique_ptr<ArgParseCmdType> ZXGADjointCmd();
unique_ptr<ArgParseCmdType> ZXGTestCmd();
unique_ptr<ArgParseCmdType> ZXGDrawCmd();
// unique_ptr<ArgParseCmdType> ZXGPrintCmd();

// unique_ptr<ArgParseCmdType> ZXGWriteCmd();

bool initZXCmd() {
    if (!(cmdMgr->regCmd("ZXCHeckout", 4, ZXCHeckoutCmd()) &&
          cmdMgr->regCmd("ZXNew", 3, ZXNewCmd()) &&
          cmdMgr->regCmd("ZXReset", 3, ZXResetCmd()) &&
          cmdMgr->regCmd("ZXDelete", 3, ZXDeleteCmd()) &&
          cmdMgr->regCmd("ZXCOPy", 5, ZXCopyCmd()) &&
          cmdMgr->regCmd("ZXCOMpose", 5, ZXComposeCmd()) &&
          cmdMgr->regCmd("ZXTensor", 3, ZXTensorCmd()) &&
          cmdMgr->regCmd("ZXPrint", 3, ZXPrintCmd()) &&
          cmdMgr->regCmd("ZXGPrint", 4, make_unique<ZXGPrintCmd>()) &&
          cmdMgr->regCmd("ZXGTest", 4, ZXGTestCmd()) &&
          cmdMgr->regCmd("ZXGEdit", 4, make_unique<ZXGEditCmd>()) &&
          cmdMgr->regCmd("ZXGADJoint", 6, ZXGADjointCmd()) &&
          cmdMgr->regCmd("ZXGASsign", 5, make_unique<ZXGAssignCmd>()) &&
          cmdMgr->regCmd("ZXGTRaverse", 5, ZXGTraverseCmd()) &&
          cmdMgr->regCmd("ZXGDraw", 4, ZXGDrawCmd()) &&
          cmdMgr->regCmd("ZX2TS", 5, ZX2TSCmd()) &&
          cmdMgr->regCmd("ZXGRead", 4, make_unique<ZXGReadCmd>()) &&
          cmdMgr->regCmd("ZXGWrite", 4, make_unique<ZXGWriteCmd>()))) {
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType const validZXGraphId = {
    [](size_t const &id) {
        return zxGraphMgr.isID(id);
    },
    [](size_t const &id) {
        cerr << "Error: ZXGraph " << id << " does not exist!!\n";
    }};

bool zxGraphMgrNotEmpty(std::string const &command) {
    if (zxGraphMgr.empty()) {
        cerr << "Error: ZX-graph list is empty now. Please ZXNew before " << command << ".\n";
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZXCHeckout <(size_t id)>
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXCHeckoutCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXCHeckout");
    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("checkout to Graph <id> in ZXGraphMgr");
        parser.addArgument<size_t>("id")
            .constraint(validZXGraphId)
            .help("the ID of the ZXGraph");
    };
    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        zxGraphMgr.checkout(parser["id"]);
        return CMD_EXEC_DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    ZXNew [(size_t id)]
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXNewCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXNew");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("create a new ZXGraph to ZXGraphMgr");

        parser.addArgument<size_t>("id")
            .nargs(NArgsOption::OPTIONAL)
            .help("the ID of the ZXGraph");

        parser.addArgument<bool>("-Replace")
            .action(storeTrue)
            .help("if specified, replace the current ZXGraph; otherwise store to a new one");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        size_t id = (parser["id"].isParsed()) ? parser.get<size_t>("id") : zxGraphMgr.getNextID();

        if (zxGraphMgr.isID(id)) {
            if (!parser["-Replace"].isParsed()) {
                cerr << "Error: ZXGraph " << id << " already exists!! Specify `-Replace` if needed." << endl;
                return CMD_EXEC_ERROR;
            }
            zxGraphMgr.set(make_unique<ZXGraph>(id));
            return CMD_EXEC_DONE;
        }

        zxGraphMgr.add(id);
        return CMD_EXEC_DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    ZXReset
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXResetCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXReset");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("reset ZXGraphMgr");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        zxGraphMgr.reset();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXDelete <(size_t id)>
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXDeleteCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXDelete");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("remove a ZXGraph from ZXGraphMgr");

        parser.addArgument<size_t>("id")
            .constraint(validZXGraphId)
            .help("the ID of the ZXGraph");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        zxGraphMgr.remove(parser["id"]);
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXPrint [-Summary | -Focus | -Num]
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXPrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXPrint");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("print info of ZXGraphMgr");
        auto mutex = parser.addMutuallyExclusiveGroup().required(false);

        mutex.addArgument<bool>("-summary")
            .action(storeTrue)
            .help("print summary of all ZXGraphs");
        mutex.addArgument<bool>("-focus")
            .action(storeTrue)
            .help("print the info of the ZXGraph in focus");
        mutex.addArgument<bool>("-list")
            .action(storeTrue)
            .help("print a list of ZXGraph");
        mutex.addArgument<bool>("-number")
            .action(storeTrue)
            .help("print the number of ZXGraph managed");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (parser["-focus"].isParsed())
            zxGraphMgr.printFocus();
        else if (parser["-number"].isParsed())
            zxGraphMgr.printListSize();
        else if (parser["-list"].isParsed())
            zxGraphMgr.printList();
        else
            zxGraphMgr.printMgr();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXCOPy [(size_t id)]
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXCopyCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXCOPy");

    cmd->precondition = []() { return zxGraphMgrNotEmpty("ZXCOPy"); };

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("copy a ZXGraph to ZXGraphMgr");

        parser.addArgument<size_t>("id")
            .nargs(NArgsOption::OPTIONAL)
            .help("the ID copied ZXGraph to be stored");

        parser.addArgument<bool>("-Replace")
            .defaultValue(false)
            .action(storeTrue)
            .help("replace the current focused ZXGraph");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        size_t id = (parser["id"].isParsed()) ? parser.get<size_t>("id") : zxGraphMgr.getNextID();
        if (zxGraphMgr.isID(id)) {
            if (!parser["-Replace"].isParsed()) {
                cerr << "Error: ZXGraph " << id << " already exists!! Specify `-Replace` if needed." << endl;
                return CMD_EXEC_ERROR;
            }
            zxGraphMgr.copy(id, false);
            return CMD_EXEC_DONE;
        }

        zxGraphMgr.copy(id);
        return CMD_EXEC_DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    ZXCOMpose <size_t id>
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXComposeCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXCOMpose");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("compose a ZXGraph");

        parser.addArgument<size_t>("id")
            .constraint(validZXGraphId)
            .help("the ID of the ZXGraph to compose with");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        zxGraphMgr.get()->compose(*zxGraphMgr.findByID(parser["id"]));
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXTensor <size_t id>
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXTensorCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXTensor");
    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("tensor a ZXGraph");
        parser.addArgument<size_t>("id")
            .constraint(validZXGraphId)
            .help("the ID of the ZXGraph");
    };
    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        zxGraphMgr.get()->tensorProduct(*zxGraphMgr.findByID(parser["id"]));
        return CMD_EXEC_DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    ZXGTest [-Empty | -Valid | -GLike | -IDentity]
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXGTestCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXGTest");

    cmd->precondition = []() { return zxGraphMgrNotEmpty("ZXGTest"); };

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("test ZXGraph structures and functions");

        auto mutex = parser.addMutuallyExclusiveGroup().required(true);

        mutex.addArgument<bool>("-empty")
            .action(storeTrue)
            .help("check if the ZXGraph is empty");
        mutex.addArgument<bool>("-valid")
            .action(storeTrue)
            .help("check if the ZXGraph is valid");
        mutex.addArgument<bool>("-glike")
            .action(storeTrue)
            .help("check if the ZXGraph is graph-like");
        mutex.addArgument<bool>("-identity")
            .action(storeTrue)
            .help("check if the ZXGraph is equivalent to identity");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (parser["-empty"].isParsed()) {
            if (zxGraphMgr.get()->isEmpty())
                cout << "The graph is empty!" << endl;
            else
                cout << "The graph is not empty!" << endl;
        } else if (parser["-valid"].isParsed()) {
            if (zxGraphMgr.get()->isValid())
                cout << "The graph is valid!" << endl;
            else
                cout << "The graph is invalid!" << endl;
        } else if (parser["-glike"].isParsed()) {
            if (zxGraphMgr.get()->isGraphLike())
                cout << "The graph is graph-like!" << endl;
            else
                cout << "The graph is not graph-like!" << endl;
        } else if (parser["-identity"].isParsed()) {
            if (zxGraphMgr.get()->isIdentity())
                cout << "The graph is an identity!" << endl;
            else
                cout << "The graph is not an identity!" << endl;
        }
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//-----------------------------------------------------------------------------------------------------------
//    ZXGPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges | -Qubits | -Neighbors | -Analysis | -Density]
//-----------------------------------------------------------------------------------------------------------
// unique_ptr<ArgParseCmdType> ZXGPrintCmd() {
//     auto cmd = make_unique<ArgParseCmdType>("ZXGPrint");

//     cmd->parserDefinition = [](ArgumentParser &parser) {
//         parser.help("print info of ZXGraph");

//         auto mutex = parser.addMutuallyExclusiveGroup();

//         mutex.addArgument<bool>("-summary")
//             .action(storeTrue)
//             .help("print the summary info of ZXGraph");
//         mutex.addArgument<bool>("-io")
//             .action(storeTrue)
//             .help("print the I/O info of ZXGraph");
//         mutex.addArgument<bool>("-inputs")
//             .action(storeTrue)
//             .help("print the inputs info of ZXGraph");
//         mutex.addArgument<bool>("-outputs")
//             .action(storeTrue)
//             .help("print the outputs info of ZXGraph");
//         mutex.addArgument<bool>("-vertices")
//             .action(storeTrue)
//             .help("print the vertices info of ZXGraph");
//         mutex.addArgument<bool>("-edges")
//             .action(storeTrue)
//             .help("print the edges info of ZXGraph");
//         mutex.addArgument<bool>("-qubits")
//             .action(storeTrue)
//             .help("print the qubits info of ZXGraph");
//         mutex.addArgument<bool>("-neighbors")
//             .action(storeTrue)
//             .help("print the neighbors info of ZXGraph");
//         mutex.addArgument<bool>("-density")
//             .action(storeTrue)
//             .help("print the density of ZXGraph");
//     };

//     cmd->onParseSuccess = [](ArgumentParser const &parser) {
//         ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGPrint");
//         //TODO - `-vertices`, `-qubits`, `-neighbors` specific printing
//         if(parser["-summary"].isParsed()){
//             zxGraphMgr->getGraph()->printGraph();
//             cout << setw(30) << left << "#T-gate: " << zxGraphMgr->getGraph()->TCount() << "\n";
//             cout << setw(30) << left << "#Non-(Clifford+T)-gate: " << zxGraphMgr->getGraph()->nonCliffordCount(false) << "\n";
//             cout << setw(30) << left << "#Non-Clifford-gate: " << zxGraphMgr->getGraph()->nonCliffordCount(true) << "\n";
//         }
//         else if(parser["-io"].isParsed()) zxGraphMgr->getGraph()->printIO();
//         else if(parser["-inputs"].isParsed()) zxGraphMgr->getGraph()->printInputs();
//         else if(parser["-outputs"].isParsed()) zxGraphMgr->getGraph()->printOutputs();
//         else if(parser["-vertices"].isParsed()) zxGraphMgr->getGraph()->printVertices();
//         else if(parser["-edges"].isParsed()) zxGraphMgr->getGraph()->printEdges();
//         else if(parser["-qubits"].isParsed()) zxGraphMgr->getGraph()->printQubits();
//         else if(parser["-neighbors"].isParsed()){}
//         else if(parser["-density"].isParsed()){
//             cout << "Density: " << zxGraphMgr->getGraph()->Density() << endl;
//         }
//         else zxGraphMgr->getGraph()->printGraph();
//         return CMD_EXEC_DONE;
//     };

//     return cmd;
// }

CmdExecStatus
ZXGPrintCmd::exec(std::stop_token, const string &option) {
    // check option
    vector<string> options;
    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    // string token;
    // if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGPrint");

    if (options.empty())
        zxGraphMgr.get()->printGraph();
    else if (myStrNCmp("-Summary", options[0], 2) == 0) {
        zxGraphMgr.get()->printGraph();
        cout << setw(30) << left << "#T-gate: " << zxGraphMgr.get()->TCount() << "\n";
        cout << setw(30) << left << "#Non-(Clifford+T)-gate: " << zxGraphMgr.get()->nonCliffordCount(false) << "\n";
        cout << setw(30) << left << "#Non-Clifford-gate: " << zxGraphMgr.get()->nonCliffordCount(true) << "\n";
    } else if (myStrNCmp("-Inputs", options[0], 2) == 0)
        zxGraphMgr.get()->printInputs();
    else if (myStrNCmp("-Outputs", options[0], 2) == 0)
        zxGraphMgr.get()->printOutputs();
    else if (myStrNCmp("-IO", options[0], 3) == 0)
        zxGraphMgr.get()->printIO();
    else if (myStrNCmp("-Vertices", options[0], 2) == 0) {
        if (options.size() == 1)
            zxGraphMgr.get()->printVertices();
        else {
            vector<size_t> candidates;
            for (size_t i = 1; i < options.size(); i++) {
                unsigned id;
                if (myStr2Uns(options[i], id)) candidates.push_back(id);
            }
            zxGraphMgr.get()->printVertices(candidates);
        }
    } else if (myStrNCmp("-Edges", options[0], 2) == 0)
        zxGraphMgr.get()->printEdges();
    else if (myStrNCmp("-Qubits", options[0], 2) == 0) {
        vector<int> candidates;
        for (size_t i = 1; i < options.size(); i++) {
            int qid;
            if (myStr2Int(options[i], qid))
                candidates.push_back(qid);
            else {
                cout << "Warning: " << options[i] << " is not a valid qubit ID!!" << endl;
            }
        }
        zxGraphMgr.get()->printQubits(candidates);
    } else if (myStrNCmp("-Neighbors", options[0], 2) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 2);

        unsigned id;
        ZXVertex *v;
        ZX_CMD_ID_VALID_OR_RETURN(options[1], id, "Vertex");
        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id, v);

        v->printVertex();
        cout << "----- Neighbors -----" << endl;
        for (auto [nb, _] : v->getNeighbors()) {
            nb->printVertex();
        }
    } else if (myStrNCmp("-Analysis", options[0], 2) == 0) {
        cout << setw(30) << left << "#T-gate: " << zxGraphMgr.get()->TCount() << "\n";
        cout << setw(30) << left << "#Non-(Clifford+T)-gate: " << zxGraphMgr.get()->nonCliffordCount(false) << "\n";
        cout << setw(30) << left << "#Non-Clifford-gate: " << zxGraphMgr.get()->nonCliffordCount(true) << "\n";
        return CMD_EXEC_DONE;
    }

    else
        return errorOption(CMD_OPT_ILLEGAL, options[0]);
    return CMD_EXEC_DONE;
}

void ZXGPrintCmd::usage() const {
    cout << "Usage: ZXGPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges | -Qubits | -Neighbors | -Analysis]" << endl;
}

void ZXGPrintCmd::summary() const {
    cout << setw(15) << left << "ZXGPrint: "
         << "print info of ZXGraph" << endl;
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
ZXGEditCmd::exec(std::stop_token, const string &option) {
    // check option
    vector<string> options;
    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;

    CMD_N_OPTS_AT_LEAST_OR_RETURN(options, 2);
    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGEdit");

    string action = options[0];
    if (myStrNCmp("-RMVertex", action, 4) == 0) {
        if (myStrNCmp("-Isolated", options[1], 2) == 0) {
            CMD_N_OPTS_AT_MOST_OR_RETURN(options, 2);
            zxGraphMgr.get()->removeIsolatedVertices();
            cout << "Note: removing isolated vertices..." << endl;
            return CMD_EXEC_DONE;
        }

        for (size_t i = 1; i < options.size(); i++) {
            unsigned id;
            if (!myStr2Uns(options[i], id)) {
                cerr << "Warning: invalid vertex ID (" << options[i] << ")!!" << endl;
                continue;
            }
            ZXVertex *v = zxGraphMgr.get()->findVertexById(id);
            if (!v) {
                cerr << "Warning: Cannot find vertex with id " << id << " in the graph!!" << endl;
                continue;
            }
            zxGraphMgr.get()->removeVertex(v);
        }
        return CMD_EXEC_DONE;
    }

    if (myStrNCmp("-RMEdge", action, 4) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 4);

        unsigned id_s, id_t;
        ZXVertex *vs;
        ZXVertex *vt;
        EdgeType etype;

        ZX_CMD_ID_VALID_OR_RETURN(options[1], id_s, "Vertex");
        ZX_CMD_ID_VALID_OR_RETURN(options[2], id_t, "Vertex");
        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id_s, vs);
        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id_t, vt);

        if (myStrNCmp("-ALL", options[3], 4) == 0) {
            zxGraphMgr.get()->removeAllEdgesBetween(vs, vt);
        } else {
            ZX_CMD_EDGE_TYPE_VALID_OR_RETURN(options[3], etype);
            zxGraphMgr.get()->removeEdge(vs, vt, etype);
        }

        return CMD_EXEC_DONE;
    }

    if (myStrNCmp("-RMGadget", action, 4) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 2);
        unsigned id;
        ZXVertex *v;
        ZX_CMD_ID_VALID_OR_RETURN(options[1], id, "Vertex");
        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id, v);
        zxGraphMgr.get()->removeGadget(v);
        return CMD_EXEC_DONE;
    }

    if (myStrNCmp("-ADDVertex", action, 5) == 0) {
        CMD_N_OPTS_BETWEEN_OR_RETURN(options, 3, 4);

        int qid;
        VertexType vt;
        Phase phase;

        ZX_CMD_QUBIT_ID_VALID_OR_RETURN(options[1], qid);
        ZX_CMD_VERTEX_TYPE_VALID_OR_RETURN(options[2], vt);
        if (options.size() == 4)
            ZX_CMD_PHASE_VALID_OR_RETURN(options[3], phase);

        zxGraphMgr.get()->addVertex(qid, vt, phase);
        return CMD_EXEC_DONE;
    }

    if (myStrNCmp("-ADDInput", action, 5) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 2);

        int qid;
        ZX_CMD_QUBIT_ID_VALID_OR_RETURN(options[1], qid);

        zxGraphMgr.get()->addInput(qid);
        return CMD_EXEC_DONE;
    }

    if (myStrNCmp("-ADDOutput", action, 5) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 2);

        int qid;
        ZX_CMD_QUBIT_ID_VALID_OR_RETURN(options[1], qid);

        zxGraphMgr.get()->addOutput(qid);
        return CMD_EXEC_DONE;
    }

    if (myStrNCmp("-ADDEdge", action, 5) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 4);

        unsigned id_s, id_t;
        ZXVertex *vs;
        ZXVertex *vt;
        EdgeType etype;

        ZX_CMD_ID_VALID_OR_RETURN(options[1], id_s, "Vertex");
        ZX_CMD_ID_VALID_OR_RETURN(options[2], id_t, "Vertex");

        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id_s, vs);
        ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id_t, vt);

        ZX_CMD_EDGE_TYPE_VALID_OR_RETURN(options[3], etype);

        zxGraphMgr.get()->addEdge(vs, vt, etype);
        return CMD_EXEC_DONE;
    }

    if (myStrNCmp("-ADDGadget", action, 5) == 0) {
        CMD_N_OPTS_AT_LEAST_OR_RETURN(options, 3);
        Phase phase;
        ZX_CMD_PHASE_VALID_OR_RETURN(options[1], phase);

        vector<ZXVertex *> verVec;
        for (size_t i = 2; i < options.size(); i++) {
            unsigned id;
            ZXVertex *v;
            ZX_CMD_ID_VALID_OR_RETURN(options[i], id, "Vertex");
            ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id, v);
            verVec.push_back(v);
        }
        zxGraphMgr.get()->addGadget(phase, verVec);
        return CMD_EXEC_DONE;
    }

    return errorOption(CMD_OPT_ILLEGAL, action);
}

void ZXGEditCmd::usage() const {
    cout << "Usage: ZXGEdit -RMVertex <-Isolated | (size_t id)... >" << endl;
    cout << "               -RMEdge <(size_t id_s), (size_t id_t)> <-ALL | (EdgeType et)>" << endl;
    cout << "               -ADDVertex <(size_t qubit), (VertexType vt), [Phase phase]>" << endl;
    cout << "               -ADDInput <(size_t qubit)>" << endl;
    cout << "               -ADDOutput <(size_t qubit)>" << endl;
    cout << "               -ADDEdge <(size_t id_s), (size_t id_t), (EdgeType et)>" << endl;
}

void ZXGEditCmd::summary() const {
    cout << setw(15) << left << "ZXGEdit: "
         << "edit ZXGraph" << endl;
}

//----------------------------------------------------------------------
//    ZXGTRaverse
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXGTraverseCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXGTRaverse");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("traverse ZXGraph and update topological order of vertices");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        zxGraphMgr.get()->updateTopoOrder();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXGDraw [-CLI]
//    ZXGDraw <string (path.pdf)>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> ZXGDrawCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXGDraw");

    cmd->precondition = []() { return zxGraphMgrNotEmpty("ZXGDraw"); };

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("draw ZXGraph");

        parser.addArgument<string>("filepath")
            .nargs(NArgsOption::OPTIONAL)
            .constraint(dir_for_file_exists)
            .constraint(allowed_extension({".pdf"}))
            .help("the output path. Accepted extension: .pdf");

        parser.addArgument<bool>("-CLI")
            .help("print to the console. Note that only horizontal wires will be printed");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (parser["filename"].isParsed()) {
            if (!zxGraphMgr.get()->writePdf(parser["filename"])) return CMD_EXEC_ERROR;
        }
        if (parser["-CLI"].isParsed()) {
            zxGraphMgr.get()->draw();
        }

        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZX2TS
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZX2TSCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZX2TS");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("convert ZXGraph to tensor");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        zxGraphMgr.get()->toTensor();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXGRead <string Input.(b)zx> [-KEEPid] [-Replace]
//----------------------------------------------------------------------
CmdExecStatus
ZXGReadCmd::exec(std::stop_token, const string &option) {  // check option
    vector<string> options;

    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;

    CMD_N_OPTS_BETWEEN_OR_RETURN(options, 1, 3);

    bool doReplace = false;
    bool doKeepID = false;
    size_t eraseIndexReplace = 0;
    size_t eraseIndexBZX = 0;
    string fileName = "";
    for (size_t i = 0, n = options.size(); i < n; ++i) {
        if (myStrNCmp("-Replace", options[i], 2) == 0) {
            if (doReplace)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            doReplace = true;
            eraseIndexReplace = i;
        } else if (myStrNCmp("-KEEPid", options[i], 5) == 0) {
            if (doKeepID)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            doKeepID = true;
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
        std::erase(options, replaceStr);
    if (doKeepID)
        std::erase(options, bzxStr);
    if (options.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, (eraseIndexBZX > eraseIndexReplace) ? bzxStr : replaceStr);

    auto bufferGraph = make_unique<ZXGraph>();
    if (!bufferGraph->readZX(fileName, doKeepID)) {
        // REVIEW - This error message is not always accurate
        // cerr << "Error: The format in \"" << fileName << "\" has something wrong!!" << endl;
        return CMD_EXEC_ERROR;
    }

    if (doReplace) {
        if (zxGraphMgr.empty()) {
            cout << "Note: ZXGraph list is empty now. Create a new one." << endl;
            zxGraphMgr.add(zxGraphMgr.getNextID());
        } else {
            cout << "Note: original ZXGraph is replaced..." << endl;
        }
    } else {
        zxGraphMgr.add(zxGraphMgr.getNextID());
    }
    zxGraphMgr.set(std::move(bufferGraph));
    return CMD_EXEC_DONE;
}

void ZXGReadCmd::usage() const {
    cout << "Usage: ZXGRead <string Input.(b)zx> [-KEEPid] [-Replace]" << endl;
}

void ZXGReadCmd::summary() const {
    cout << setw(15) << left << "ZXGRead: "
         << "read a file and construct the corresponding ZXGraph" << endl;
}

//----------------------------------------------------------------------
//    ZXGWrite <string Output.<zx | tikz | tex>> [-Complete]
//----------------------------------------------------------------------
CmdExecStatus
ZXGWriteCmd::exec(std::stop_token, const string &option) {
    vector<string> options;

    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;
    if (options.empty()) return CmdExec::errorOption(CMD_OPT_MISSING, "");

    bool doComplete = false;
    size_t eraseIndexComplete = 0;
    string fileName;
    for (size_t i = 0, n = options.size(); i < n; ++i) {
        if (myStrNCmp("-Complete", options[i], 2) == 0) {
            if (doComplete)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            doComplete = true;
            eraseIndexComplete = i;
        } else {
            if (fileName.size())
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            fileName = options[i];
        }
    }
    string completeStr = options[eraseIndexComplete];
    if (doComplete)
        std::erase(options, completeStr);
    if (options.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, completeStr);

    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXWrite");

    size_t extensionPosition = fileName.find_last_of(".");
    // REVIEW - should we guard the case of no file extension?
    if (extensionPosition != string::npos) {
        string extensionString = fileName.substr(extensionPosition);
        if (
            myStrNCmp(".zx", extensionString, 3) == 0 ||
            myStrNCmp(".bzx", extensionString, 4) == 0) {
            if (!zxGraphMgr.get()->writeZX(fileName, doComplete)) {
                cerr << "Error: fail to write ZXGraph to \"" << fileName << "\"!!" << endl;
                return CMD_EXEC_ERROR;
            }
        } else if (myStrNCmp(".tikz", extensionString, 5) == 0) {
            if (!zxGraphMgr.get()->writeTikz(fileName)) {
                cerr << "Error: fail to write Tikz to \"" << fileName << "\"!!" << endl;
                return CMD_EXEC_ERROR;
            }
        } else if (myStrNCmp(".tex", extensionString, 4) == 0) {
            if (!zxGraphMgr.get()->writeTex(fileName)) {
                cerr << "Error: fail to write tex to \"" << fileName << "\"!!" << endl;
                return CMD_EXEC_ERROR;
            }
        }
    } else {
        if (!zxGraphMgr.get()->writeZX(fileName, doComplete)) {
            cerr << "Error: fail to write ZXGraph to \"" << fileName << "\"!!" << endl;
            return CMD_EXEC_ERROR;
        }
    }

    return CMD_EXEC_DONE;
}

void ZXGWriteCmd::usage() const {
    cout << "Usage: ZXGWrite <string Output.<zx | tikz>> [-Complete]" << endl;
}

void ZXGWriteCmd::summary() const {
    cout << setw(15) << left << "ZXGWrite: "
         << "write a ZXGraph to a file\n";
}

//----------------------------------------------------------------------
//    ZXGASsign <size_t qubit> <I|O> <VertexType vt> <string Phase>
//----------------------------------------------------------------------
CmdExecStatus
ZXGAssignCmd::exec(std::stop_token, const string &option) {
    // check option
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

    if (!(isInput ? zxGraphMgr.get()->isInputQubit(qid) : zxGraphMgr.get()->isOutputQubit(qid))) {
        cerr << "Error: the specified boundary does not exist!!" << endl;
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[1]);
    }

    VertexType vt;
    ZX_CMD_VERTEX_TYPE_VALID_OR_RETURN(options[2], vt);

    Phase phase;
    ZX_CMD_PHASE_VALID_OR_RETURN(options[3], phase);

    zxGraphMgr.get()->assignBoundary(qid, (options[1] == "I"), vt, phase);
    return CMD_EXEC_DONE;
}

void ZXGAssignCmd::usage() const {
    cout << "Usage: ZXGASsign <size_t qubit> <I|O> <VertexType vt> <string Phase>" << endl;
}

void ZXGAssignCmd::summary() const {
    cout << setw(15) << left << "ZXGASsign: "
         << "assign quantum states to input/output vertex\n";
}

//----------------------------------------------------------------------
//    ZXGADJoint
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXGADjointCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXGADjoint");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("adjoint ZXGraph");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        zxGraphMgr.get()->adjoint();
        return CMD_EXEC_DONE;
    };

    return cmd;
}
