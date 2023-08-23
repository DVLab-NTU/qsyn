/****************************************************************************
  FileName     [ zxCmd.cpp ]
  PackageName  [ zx ]
  Synopsis     [ Define zx package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zxCmd.hpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <string>

#include "./toTensor.hpp"
#include "./zxGraphMgr.hpp"
#include "tensor/tensorMgr.hpp"
#include "zx/zxDef.hpp"

using namespace std;

ZXGraphMgr zxGraphMgr{"ZXGraph"};
extern TensorMgr tensorMgr;
using namespace ArgParse;

unique_ptr<Command> ZXCHeckoutCmd();
unique_ptr<Command> ZXNewCmd();
unique_ptr<Command> ZXResetCmd();
unique_ptr<Command> ZXDeleteCmd();
unique_ptr<Command> ZXPrintCmd();
unique_ptr<Command> ZXCopyCmd();
unique_ptr<Command> ZXComposeCmd();
unique_ptr<Command> ZXTensorCmd();
unique_ptr<Command> ZXGTraverseCmd();
unique_ptr<Command> ZX2TSCmd();
unique_ptr<Command> ZXGADjointCmd();
unique_ptr<Command> ZXGTestCmd();
unique_ptr<Command> ZXGDrawCmd();
unique_ptr<Command> ZXGPrintCmd();
unique_ptr<Command> ZXGEditCmd();
unique_ptr<Command> ZXGReadCmd();
unique_ptr<Command> ZXGWriteCmd();
unique_ptr<Command> ZXGAssignCmd();

bool initZXCmd() {
    if (!(cli.registerCommand("ZXCHeckout", 4, ZXCHeckoutCmd()) &&
          cli.registerCommand("ZXNew", 3, ZXNewCmd()) &&
          cli.registerCommand("ZXReset", 3, ZXResetCmd()) &&
          cli.registerCommand("ZXDelete", 3, ZXDeleteCmd()) &&
          cli.registerCommand("ZXCOPy", 5, ZXCopyCmd()) &&
          cli.registerCommand("ZXCOMpose", 5, ZXComposeCmd()) &&
          cli.registerCommand("ZXTensor", 3, ZXTensorCmd()) &&
          cli.registerCommand("ZXPrint", 3, ZXPrintCmd()) &&
          cli.registerCommand("ZXGPrint", 4, ZXGPrintCmd()) &&
          cli.registerCommand("ZXGTest", 4, ZXGTestCmd()) &&
          cli.registerCommand("ZXGEdit", 4, ZXGEditCmd()) &&
          cli.registerCommand("ZXGADJoint", 6, ZXGADjointCmd()) &&
          cli.registerCommand("ZXGASsign", 5, ZXGAssignCmd()) &&
          cli.registerCommand("ZXGTRaverse", 5, ZXGTraverseCmd()) &&
          cli.registerCommand("ZXGDraw", 4, ZXGDrawCmd()) &&
          cli.registerCommand("ZX2TS", 5, ZX2TSCmd()) &&
          cli.registerCommand("ZXGRead", 4, ZXGReadCmd()) &&
          cli.registerCommand("ZXGWrite", 4, ZXGWriteCmd()))) {
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType const validZXGraphId = {
    [](size_t const& id) {
        return zxGraphMgr.isID(id);
    },
    [](size_t const& id) {
        logger.error("ZXGraph {} does not exist!!", id);
    }};

ArgType<size_t>::ConstraintType const zxGraphIdNotExist = {
    [](size_t const& id) {
        return !zxGraphMgr.isID(id);
    },
    [](size_t const& id) {
        logger.error("ZXGraph {} already exists!!", id);
        logger.info("Use `-Replace` if you want to overwrite it.");
    }};

ArgType<size_t>::ConstraintType const validZXVertexId = {
    [](size_t const& id) {
        return zxGraphMgr.get()->isId(id);
    },
    [](size_t const& id) {
        logger.error("Cannot find vertex with ID {} in the ZXGraph!!", id);
    }};

ArgType<size_t>::ConstraintType const notExistingZXInputQubitId = {
    [](size_t const& qid) {
        return !zxGraphMgr.get()->isInputQubit(qid);
    },
    [](size_t const& qid) {
        logger.error("This qubit's input already exists!!");
    }};

ArgType<size_t>::ConstraintType const notExistingZXOutputQubitId = {
    [](size_t const& qid) {
        return !zxGraphMgr.get()->isOutputQubit(qid);
    },
    [](size_t const& qid) {
        logger.error("This qubit's output already exists!!");
    }};

bool zxGraphMgrNotEmpty() {
    if (zxGraphMgr.empty()) {
        logger.error("ZXGraph list is empty. Please create a ZXGraph first!!");
        logger.info("Use ZXNew to add a new ZXGraph, or ZXGRead to read a ZXGraph from a file.");
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZXCHeckout <(size_t id)>
//----------------------------------------------------------------------
unique_ptr<Command> ZXCHeckoutCmd() {
    auto cmd = make_unique<Command>("ZXCHeckout");

    cmd->precondition = zxGraphMgrNotEmpty;

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("checkout to Graph <id> in ZXGraphMgr");
        parser.addArgument<size_t>("id")
            .constraint(validZXGraphId)
            .help("the ID of the ZXGraph");
    };
    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        zxGraphMgr.checkout(parser.get<size_t>("id"));
        return CmdExecResult::DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    ZXNew [(size_t id)]
//----------------------------------------------------------------------
unique_ptr<Command> ZXNewCmd() {
    auto cmd = make_unique<Command>("ZXNew");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("create a new ZXGraph to ZXGraphMgr");

        parser.addArgument<size_t>("id")
            .nargs(NArgsOption::OPTIONAL)
            .help("the ID of the ZXGraph");

        parser.addArgument<bool>("-Replace")
            .action(storeTrue)
            .help("if specified, replace the current ZXGraph; otherwise store to a new one");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        size_t id = (parser.parsed("id")) ? parser.get<size_t>("id") : zxGraphMgr.getNextID();

        if (zxGraphMgr.isID(id)) {
            if (!parser.parsed("-Replace")) {
                cerr << "Error: ZXGraph " << id << " already exists!! Specify `-Replace` if needed." << endl;
                return CmdExecResult::ERROR;
            }
            zxGraphMgr.set(make_unique<ZXGraph>());
            return CmdExecResult::DONE;
        } else {
            zxGraphMgr.add(id);
        }

        return CmdExecResult::DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    ZXReset
//----------------------------------------------------------------------
unique_ptr<Command> ZXResetCmd() {
    auto cmd = make_unique<Command>("ZXReset");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("reset ZXGraphMgr");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        zxGraphMgr.reset();
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXDelete <(size_t id)>
//----------------------------------------------------------------------
unique_ptr<Command> ZXDeleteCmd() {
    auto cmd = make_unique<Command>("ZXDelete");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("remove a ZXGraph from ZXGraphMgr");

        parser.addArgument<size_t>("id")
            .constraint(validZXGraphId)
            .help("the ID of the ZXGraph");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        zxGraphMgr.remove(parser.get<size_t>("id"));
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXPrint [-Summary | -Focus | -Num]
//----------------------------------------------------------------------
unique_ptr<Command> ZXPrintCmd() {
    auto cmd = make_unique<Command>("ZXPrint");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print info about ZXGraphs");
        auto mutex = parser.addMutuallyExclusiveGroup().required(false);

        mutex.addArgument<bool>("-focus")
            .action(storeTrue)
            .help("print the info of the ZXGraph in focus");
        mutex.addArgument<bool>("-list")
            .action(storeTrue)
            .help("print a list of ZXGraphs");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser.parsed("-focus"))
            zxGraphMgr.printFocus();
        else if (parser.parsed("-list"))
            zxGraphMgr.printList();
        else
            zxGraphMgr.printManager();
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXCOPy [(size_t id)]
//----------------------------------------------------------------------
unique_ptr<Command> ZXCopyCmd() {
    auto cmd = make_unique<Command>("ZXCOPy");

    cmd->precondition = zxGraphMgrNotEmpty;

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("copy a ZXGraph to ZXGraphMgr");

        parser.addArgument<size_t>("id")
            .nargs(NArgsOption::OPTIONAL)
            .help("the ID copied ZXGraph to be stored");

        parser.addArgument<bool>("-Replace")
            .defaultValue(false)
            .action(storeTrue)
            .help("replace the current focused ZXGraph");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        size_t id = (parser.parsed("id")) ? parser.get<size_t>("id") : zxGraphMgr.getNextID();
        if (zxGraphMgr.isID(id)) {
            if (!parser.parsed("-Replace")) {
                cerr << "Error: ZXGraph " << id << " already exists!! Specify `-Replace` if needed." << endl;
                return CmdExecResult::ERROR;
            }
            zxGraphMgr.copy(id);
            return CmdExecResult::DONE;
        }

        zxGraphMgr.copy(id);
        return CmdExecResult::DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    ZXCOMpose <size_t id>
//----------------------------------------------------------------------
unique_ptr<Command> ZXComposeCmd() {
    auto cmd = make_unique<Command>("ZXCOMpose");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("compose a ZXGraph");

        parser.addArgument<size_t>("id")
            .constraint(validZXGraphId)
            .help("the ID of the ZXGraph to compose with");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        zxGraphMgr.get()->compose(*zxGraphMgr.findByID(parser.get<size_t>("id")));
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXTensor <size_t id>
//----------------------------------------------------------------------
unique_ptr<Command> ZXTensorCmd() {
    auto cmd = make_unique<Command>("ZXTensor");
    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("tensor a ZXGraph");
        parser.addArgument<size_t>("id")
            .constraint(validZXGraphId)
            .help("the ID of the ZXGraph");
    };
    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        zxGraphMgr.get()->tensorProduct(*zxGraphMgr.findByID(parser.get<size_t>("id")));
        return CmdExecResult::DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    ZXGTest [-Empty | -Valid | -GLike | -IDentity]
//----------------------------------------------------------------------
unique_ptr<Command> ZXGTestCmd() {
    auto cmd = make_unique<Command>("ZXGTest");

    cmd->precondition = zxGraphMgrNotEmpty;

    cmd->parserDefinition = [](ArgumentParser& parser) {
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

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser.parsed("-empty")) {
            if (zxGraphMgr.get()->isEmpty())
                cout << "The graph is empty!" << endl;
            else
                cout << "The graph is not empty!" << endl;
        } else if (parser.parsed("-valid")) {
            if (zxGraphMgr.get()->isValid())
                cout << "The graph is valid!" << endl;
            else
                cout << "The graph is invalid!" << endl;
        } else if (parser.parsed("-glike")) {
            if (zxGraphMgr.get()->isGraphLike())
                cout << "The graph is graph-like!" << endl;
            else
                cout << "The graph is not graph-like!" << endl;
        } else if (parser.parsed("-identity")) {
            if (zxGraphMgr.get()->isIdentity())
                cout << "The graph is an identity!" << endl;
            else
                cout << "The graph is not an identity!" << endl;
        }
        return CmdExecResult::DONE;
    };

    return cmd;
}

//-----------------------------------------------------------------------------------------------------------
//    ZXGPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges | -Qubits | -Neighbors | -Analysis | -Density]
//-----------------------------------------------------------------------------------------------------------
unique_ptr<Command> ZXGPrintCmd() {
    auto cmd = make_unique<Command>("ZXGPrint");

    cmd->precondition = zxGraphMgrNotEmpty;

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print info of ZXGraph");

        auto mutex = parser.addMutuallyExclusiveGroup();
        mutex.addArgument<bool>("-list")
            .action(storeTrue)
            .help("print a list of ZXGraphs");
        mutex.addArgument<bool>("-summary")
            .action(storeTrue)
            .help("print the summary info of ZXGraph");
        mutex.addArgument<bool>("-io")
            .action(storeTrue)
            .help("print the I/O info of ZXGraph");
        mutex.addArgument<bool>("-inputs")
            .action(storeTrue)
            .help("print the input info of ZXGraph");
        mutex.addArgument<bool>("-outputs")
            .action(storeTrue)
            .help("print the output info of ZXGraph");
        mutex.addArgument<size_t>("-vertices")
            .nargs(NArgsOption::ZERO_OR_MORE)
            .constraint(validZXVertexId)
            .help("print the vertex info of ZXGraph");
        mutex.addArgument<bool>("-edges")
            .action(storeTrue)
            .help("print the edges info of ZXGraph");
        mutex.addArgument<int>("-qubits")
            .nargs(NArgsOption::ZERO_OR_MORE)
            .help("print the qubit info of ZXGraph");
        mutex.addArgument<size_t>("-neighbors")
            .constraint(validZXVertexId)
            .help("print the neighbor info of ZXGraph");
        mutex.addArgument<bool>("-density")
            .action(storeTrue)
            .help("print the density of ZXGraph");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser.parsed("-summary")) {
            zxGraphMgr.get()->printGraph();
            fmt::println("{:<29} {}", "#T-gate:", zxGraphMgr.get()->TCount());
            fmt::println("{:<29} {}", "#Non-(Clifford+T)-gate: ", zxGraphMgr.get()->nonCliffordPlusTCount());
            fmt::println("{:<29} {}", "#Non-Clifford-gate: ", zxGraphMgr.get()->nonCliffordCount());
        } else if (parser.parsed("-io"))
            zxGraphMgr.get()->printIO();
        else if (parser.parsed("-list"))
            zxGraphMgr.printList();
        else if (parser.parsed("-inputs"))
            zxGraphMgr.get()->printInputs();
        else if (parser.parsed("-outputs"))
            zxGraphMgr.get()->printOutputs();
        else if (parser.parsed("-vertices")) {
            auto vids = parser.get<vector<size_t>>("-vertices");
            if (vids.empty())
                zxGraphMgr.get()->printVertices();
            else
                zxGraphMgr.get()->printVertices(vids);
        } else if (parser.parsed("-edges"))
            zxGraphMgr.get()->printEdges();
        else if (parser.parsed("-qubits")) {
            auto qids = parser.get<vector<int>>("-qubits");
            zxGraphMgr.get()->printQubits(qids);
        } else if (parser.parsed("-neighbors")) {
            auto v = zxGraphMgr.get()->findVertexById(parser.get<size_t>("-neighbors"));
            v->printVertex();
            cout << "----- Neighbors -----" << endl;
            for (auto [nb, _] : v->getNeighbors()) {
                nb->printVertex();
            }
        } else if (parser.parsed("-density")) {
            cout << "Density: " << zxGraphMgr.get()->density() << endl;
        } else
            zxGraphMgr.get()->printGraph();
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<Command> ZXGEditCmd() {
    auto cmd = make_unique<Command>("ZXGEdit");

    cmd->precondition = zxGraphMgrNotEmpty;

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("edit ZXGraph");

        auto subparsers = parser.addSubParsers().required(true);

        auto removeVertexParser = subparsers.addParser("-rmvertex");

        auto rmv_mutex = removeVertexParser.addMutuallyExclusiveGroup().required(true);

        rmv_mutex.addArgument<size_t>("ids")
            .constraint(validZXVertexId)
            .nargs(NArgsOption::ZERO_OR_MORE)
            .help("the IDs of vertices to remove");

        rmv_mutex.addArgument<bool>("-isolated")
            .action(storeTrue)
            .help("if set, remove all isolated vertices");

        auto removeEdgeParser = subparsers.addParser("-rmedge");

        removeEdgeParser.addArgument<size_t>("ids")
            .nargs(2)
            .constraint(validZXVertexId)
            .metavar("(vs, vt)")
            .help("the IDs to the two vertices to remove edges in between");

        removeEdgeParser.addArgument<string>("etype")
            .constraint(choices_allow_prefix({"simple", "hadamard", "all"}))
            .help("the edge type to remove. Options: simple, hadamard, all (i.e., remove both)");

        auto addVertexParser = subparsers.addParser("-addvertex");

        addVertexParser.addArgument<size_t>("qubit")
            .help("the qubit ID the ZXVertex belongs to");

        addVertexParser.addArgument<string>("vtype")
            .constraint(choices_allow_prefix({"zspider", "xspider", "hbox"}))
            .help("the type of ZXVertex");

        addVertexParser.addArgument<Phase>("phase")
            .nargs(NArgsOption::OPTIONAL)
            .defaultValue(Phase(0))
            .help("phase of the ZXVertex (default = 0)");

        auto addInputParser = subparsers.addParser("-addinput");

        addInputParser.addArgument<size_t>("qubit")
            .constraint(notExistingZXInputQubitId)
            .help("the qubit ID of the input");

        auto addOutputParser = subparsers.addParser("-addoutput");

        addOutputParser.addArgument<size_t>("qubit")
            .constraint(notExistingZXOutputQubitId)
            .help("the qubit ID of the output");

        auto addEdgeParser = subparsers.addParser("-addedge");

        addEdgeParser.addArgument<size_t>("ids")
            .nargs(2)
            .constraint(validZXVertexId)
            .metavar("(vs, vt)")
            .help("the IDs to the two vertices to add edges in between");

        addEdgeParser.addArgument<string>("etype")
            .constraint(choices_allow_prefix({"simple", "hadamard"}))
            .help("the edge type to add. Options: simple, hadamard");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser.usedSubParser("-rmvertex")) {
            if (parser.parsed("ids")) {
                auto ids = parser.get<vector<size_t>>("ids");
                auto vertices_range = ids |
                                      views::transform([](size_t id) { return zxGraphMgr.get()->findVertexById(id); }) |
                                      views::filter([](ZXVertex* v) { return v != nullptr; });
                for (auto&& v : vertices_range) {
                    logger.info("Removing vertex {}...", v->getId());
                }

                zxGraphMgr.get()->removeVertices({vertices_range.begin(), vertices_range.end()});
            } else if (parser.parsed("-isolated")) {
                logger.info("Removing isolated vertices...");
                zxGraphMgr.get()->removeIsolatedVertices();
            }
            return CmdExecResult::DONE;
        }
        if (parser.usedSubParser("-rmedge")) {
            auto ids = parser.get<std::vector<size_t>>("ids");
            auto v0 = zxGraphMgr.get()->findVertexById(ids[0]);
            auto v1 = zxGraphMgr.get()->findVertexById(ids[1]);
            assert(v0 != nullptr && v1 != nullptr);

            auto etype = str2EdgeType(parser.get<std::string>("etype"));

            if (etype.has_value()) {
                logger.info("Removing edge ({}, {}), edge type: {}...", v0->getId(), v1->getId(), etype.value());
                zxGraphMgr.get()->removeEdge(v0, v1, etype.value());
            } else {
                logger.info("Removing all edges between ({}, {})...", v0->getId(), v1->getId());
                zxGraphMgr.get()->removeAllEdgesBetween(v0, v1);
            }

            return CmdExecResult::DONE;
        }
        if (parser.usedSubParser("-addvertex")) {
            auto vtype = str2VertexType(parser.get<std::string>("vtype"));
            assert(vtype.has_value());

            auto v = zxGraphMgr.get()->addVertex(parser.get<size_t>("qubit"), vtype.value(), parser.get<Phase>("phase"));
            logger.info("Adding vertex {}...", v->getId());
            return CmdExecResult::DONE;
        }
        if (parser.usedSubParser("-addinput")) {
            auto i = zxGraphMgr.get()->addInput(parser.get<size_t>("qubit"));
            logger.info("Adding input {}...", i->getId());
            return CmdExecResult::DONE;
        }
        if (parser.usedSubParser("-addoutput")) {
            auto o = zxGraphMgr.get()->addOutput(parser.get<size_t>("qubit"));
            logger.info("Adding output {}...", o->getId());
            return CmdExecResult::DONE;
        }
        if (parser.usedSubParser("-addedge")) {
            auto ids = parser.get<std::vector<size_t>>("ids");
            auto vs = zxGraphMgr.get()->findVertexById(ids[0]);
            auto vt = zxGraphMgr.get()->findVertexById(ids[1]);
            assert(vs != nullptr && vt != nullptr);

            auto etype = str2EdgeType(parser.get<std::string>("etype"));
            assert(etype.has_value());

            if (vs->isNeighbor(vt, etype.value()) && (vs->isBoundary() || vt->isBoundary())) {
                logger.fatal("Cannot add edge between boundary vertices {} and {}", vs->getId(), vt->getId());
                return CmdExecResult::ERROR;
            }

            bool hadEdge = vs->isNeighbor(vt, etype.value());

            zxGraphMgr.get()->addEdge(vs, vt, etype.value());

            if (vs == vt) {
                logger.info("Note: converting this self-loop to phase {} on vertex {}...", etype.value() == EdgeType::HADAMARD ? Phase(1) : Phase(0), vs->getId());
            } else if (hadEdge) {
                bool hasEdge = vs->isNeighbor(vt, etype.value());
                if (hasEdge) {
                    logger.info("Note: redundant edge; merging into existing edge ({}, {})...", vs->getId(), vt->getId());
                } else {
                    logger.info("Note: Hopf edge; cancelling out with existing edge ({}, {})...", vs->getId(), vt->getId());
                }
            } else {
                logger.info("Adding edge ({}, {}), edge type: {}...", vs->getId(), vt->getId(), etype.value());
            }

            return CmdExecResult::DONE;
        }
        return CmdExecResult::ERROR;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXGTRaverse
//----------------------------------------------------------------------
unique_ptr<Command> ZXGTraverseCmd() {
    auto cmd = make_unique<Command>("ZXGTRaverse");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("traverse ZXGraph and update topological order of vertices");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        zxGraphMgr.get()->updateTopoOrder();
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXGDraw [-CLI]
//    ZXGDraw <string (path.pdf)>
//----------------------------------------------------------------------

unique_ptr<Command> ZXGDrawCmd() {
    auto cmd = make_unique<Command>("ZXGDraw");

    cmd->precondition = zxGraphMgrNotEmpty;

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("draw ZXGraph");

        parser.addArgument<string>("filepath")
            .nargs(NArgsOption::OPTIONAL)
            .constraint(path_writable)
            .constraint(allowed_extension({".pdf"}))
            .help("the output path. Supported extension: .pdf");

        parser.addArgument<bool>("-CLI")
            .action(storeTrue)
            .help("print to the console. Note that only horizontal wires will be printed");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser.parsed("filepath")) {
            if (!zxGraphMgr.get()->writePdf(parser.get<string>("filepath"))) return CmdExecResult::ERROR;
        }
        if (parser.parsed("-CLI")) {
            zxGraphMgr.get()->draw();
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZX2TS
//----------------------------------------------------------------------
unique_ptr<Command> ZX2TSCmd() {
    auto cmd = make_unique<Command>("ZX2TS");

    cmd->precondition = zxGraphMgrNotEmpty;

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("convert ZXGraph to tensor");

        parser.addArgument<size_t>("-zx")
            .metavar("id")
            .constraint(validZXGraphId)
            .help("the ID of the ZXGraph to be converted. If not specified, the focused ZXGraph is used");

        parser.addArgument<size_t>("-ts")
            .metavar("id")
            .help("the ID of the target tensor. If not specified, an ID is automatically assigned");

        parser.addArgument<bool>("-replace")
            .action(storeTrue)
            .help("replace the target tensor if the tensor ID is occupied");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        auto zxID = parser.parsed("-zx") ? parser.get<size_t>("-zx") : zxGraphMgr.focusedID();
        auto zx = zxGraphMgr.findByID(zxID);

        auto tsID = parser.parsed("-ts") ? parser.get<size_t>("-ts") : tensorMgr.getNextID();

        if (tensorMgr.isID(tsID) && !parser.parsed("-replace")) {
            logger.error("Tensor {} already exists!! Specify `-Replace` if you intend to replace the current one.", tsID);
            return CmdExecResult::ERROR;
        }
        logger.info("Converting ZXGraph {} to Tensor {}...", zxID, tsID);
        auto tensor = toTensor(*zx);

        if (tensor.has_value()) {
            if (!tensorMgr.isID(tsID)) {
                tensorMgr.add(tsID, std::make_unique<QTensor<double>>(std::move(tensor.value())));
            } else {
                tensorMgr.checkout(tsID);
                tensorMgr.set(std::make_unique<QTensor<double>>(std::move(tensor.value())));
            }

            tensorMgr.get()->setFileName(zx->getFileName());
            tensorMgr.get()->addProcedures(zx->getProcedures());
            tensorMgr.get()->addProcedure("ZX2TS");
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXGRead <string Input.(b)zx> [-KEEPid] [-Replace]
//----------------------------------------------------------------------

unique_ptr<Command> ZXGReadCmd() {
    auto cmd = make_unique<Command>("ZXGRead");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("read a file and construct the corresponding ZXGraph");

        parser.addArgument<string>("filepath")
            .constraint(path_readable)
            .constraint(allowed_extension({".zx", ".bzx"}))
            .help("path to the ZX file. Supported extensions: .zx, .bzx");

        parser.addArgument<bool>("-keepid")
            .action(storeTrue)
            .help("if set, retain the IDs in the ZX file; otherwise the ID is rearranged to be consecutive");

        parser.addArgument<bool>("-replace")
            .action(storeTrue)
            .constraint(zxGraphIdNotExist)
            .help("replace the current ZXGraph");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        auto filepath = parser.get<string>("filepath");
        auto doKeepID = parser.get<bool>("-keepid");
        auto doReplace = parser.get<bool>("-replace");

        auto bufferGraph = make_unique<ZXGraph>();
        if (!bufferGraph->readZX(filepath, doKeepID)) {
            return CmdExecResult::ERROR;
        }

        if (doReplace) {
            if (zxGraphMgr.empty()) {
                cout << "Note: ZXGraph list is empty now. Create a new one." << endl;
                zxGraphMgr.add(zxGraphMgr.getNextID());
            } else {
                cout << "Note: original ZXGraph is replaced..." << endl;
            }
            zxGraphMgr.set(std::move(bufferGraph));
        } else {
            zxGraphMgr.add(zxGraphMgr.getNextID(), std::move(bufferGraph));
        }
        zxGraphMgr.get()->setFileName(std::filesystem::path{filepath}.stem());
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<Command> ZXGWriteCmd() {
    auto cmd = make_unique<Command>("ZXGWrite");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("write the ZXGraph to a file");
        parser.addArgument<string>("filepath")
            .constraint(path_writable)
            .constraint(allowed_extension({".zx", ".bzx", ".tikz", ".tex", ""}))
            .help("the path to the output ZX file");

        parser.addArgument<bool>("-complete")
            .action(storeTrue)
            .help("if specified, output neighbor information on both vertices of each edge");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        auto filepath = parser.get<string>("filepath");
        auto doComplete = parser.get<bool>("-complete");
        size_t extensionPos = filepath.find_last_of('.');

        string extension = (extensionPos == string::npos) ? "" : filepath.substr(extensionPos);
        if (extension == ".zx" || extension == ".bzx" || extension == "") {
            if (!zxGraphMgr.get()->writeZX(filepath, doComplete)) {
                cerr << "Error: fail to write ZXGraph to \"" << filepath << "\"!!\n";
                return CmdExecResult::ERROR;
            }
        } else if (extension == ".tikz") {
            if (!zxGraphMgr.get()->writeTikz(filepath)) {
                cerr << "Error: fail to write Tikz to \"" << filepath << "\"!!\n";
                return CmdExecResult::ERROR;
            }
        } else if (extension == ".tex") {
            if (!zxGraphMgr.get()->writeTex(filepath)) {
                cerr << "Error: fail to write tex to \"" << filepath << "\"!!\n";
                return CmdExecResult::ERROR;
            }
        }
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXGASsign <size_t qubit> <I|O> <VertexType vt> <string Phase>
//----------------------------------------------------------------------

unique_ptr<Command> ZXGAssignCmd() {
    auto cmd = make_unique<Command>("ZXGASsign");

    cmd->precondition = zxGraphMgrNotEmpty;

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("assign quantum states to input/output vertex");

        parser.addArgument<size_t>("qubit")
            .help("the qubit to assign state to");

        parser.addArgument<string>("io")
            .constraint(choices_allow_prefix({"input", "output"}))
            .metavar("input/output")
            .help("add at input or output");

        parser.addArgument<string>("vtype")
            .constraint(choices_allow_prefix({"zspider", "xspider", "hbox"}))
            .help("the type of ZXVertex");

        parser.addArgument<Phase>("phase")
            .help("the phase of the vertex");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        auto qid = parser.get<size_t>("qubit");
        bool isInput = toLowerString(parser.get<string>("io")).starts_with('i');

        if (!(isInput ? zxGraphMgr.get()->isInputQubit(qid) : zxGraphMgr.get()->isOutputQubit(qid))) {
            cerr << "Error: the specified boundary does not exist!!" << endl;
            return CmdExecResult::ERROR;
        }

        auto vtype = str2VertexType(parser.get<std::string>("vtype"));
        assert(vtype.has_value());

        auto phase = parser.get<Phase>("phase");
        zxGraphMgr.get()->assignBoundary(qid, isInput, vtype.value(), phase);

        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    ZXGADJoint
//----------------------------------------------------------------------
unique_ptr<Command> ZXGADjointCmd() {
    auto cmd = make_unique<Command>("ZXGADjoint");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("adjoint ZXGraph");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        zxGraphMgr.get()->adjoint();
        return CmdExecResult::DONE;
    };

    return cmd;
}
