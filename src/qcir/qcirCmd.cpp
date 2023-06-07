/****************************************************************************
  FileName     [ qcirCmd.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcirCmd.h"

#include <cstddef>   // for size_t, NULL
#include <iostream>  // for ostream
#include <string>    // for string

#include "apCmd.h"
#include "cmdMacros.h"  // for CMD_N_OPTS_AT_MOST_OR_RETURN
#include "phase.h"      // for Phase
#include "qcir.h"       // for QCir
#include "qcirGate.h"   // for QCirGate
#include "qcirMgr.h"    // for QCirMgr
#include "zxGraph.h"    // for ZXGraph

using namespace std;
using namespace ArgParse;
extern QCirMgr *qcirMgr;
extern size_t verbose;
extern int effLimit;

unique_ptr<ArgParseCmdType> QCirCheckOutCmd();
unique_ptr<ArgParseCmdType> QCirResetCmd();
unique_ptr<ArgParseCmdType> QCirDeleteCmd();
unique_ptr<ArgParseCmdType> QCirNewCmd();
unique_ptr<ArgParseCmdType> QCirCopyCmd();
unique_ptr<ArgParseCmdType> QCirComposeCmd();
unique_ptr<ArgParseCmdType> QCirTensorCmd();
unique_ptr<ArgParseCmdType> QCPrintCmd();
unique_ptr<ArgParseCmdType> QCSetCmd();
unique_ptr<ArgParseCmdType> QCirReadCmd();
unique_ptr<ArgParseCmdType> QCirPrintCmd();
unique_ptr<ArgParseCmdType> QCirGatePrintCmd();
unique_ptr<ArgParseCmdType> QCirAddQubitCmd();
unique_ptr<ArgParseCmdType> QCirDeleteGateCmd();
unique_ptr<ArgParseCmdType> QCirDeleteQubitCmd();
unique_ptr<ArgParseCmdType> QCir2ZXCmd();
unique_ptr<ArgParseCmdType> QCir2TSCmd();
unique_ptr<ArgParseCmdType> QCirWriteCmd();
unique_ptr<ArgParseCmdType> QCirDrawCmd();

bool initQCirCmd() {
    qcirMgr = new QCirMgr;
    if (!(cmdMgr->regCmd("QCCHeckout", 4, QCirCheckOutCmd()) &&
          cmdMgr->regCmd("QCReset", 3, QCirResetCmd()) &&
          cmdMgr->regCmd("QCDelete", 3, QCirDeleteCmd()) &&
          cmdMgr->regCmd("QCNew", 3, QCirNewCmd()) &&
          cmdMgr->regCmd("QCCOPy", 5, QCirCopyCmd()) &&
          cmdMgr->regCmd("QCCOMpose", 5, QCirComposeCmd()) &&
          cmdMgr->regCmd("QCTensor", 3, QCirTensorCmd()) &&
          cmdMgr->regCmd("QCPrint", 3, QCPrintCmd()) &&
          cmdMgr->regCmd("QCSet", 3, QCSetCmd()) &&
          cmdMgr->regCmd("QCCRead", 4, QCirReadCmd()) &&
          cmdMgr->regCmd("QCCPrint", 4, QCirPrintCmd()) &&
          cmdMgr->regCmd("QCGAdd", 4, make_unique<QCirAddGateCmd>()) &&
          cmdMgr->regCmd("QCBAdd", 4, QCirAddQubitCmd()) &&
          cmdMgr->regCmd("QCGDelete", 4, QCirDeleteGateCmd()) &&
          cmdMgr->regCmd("QCBDelete", 4, QCirDeleteQubitCmd()) &&
          cmdMgr->regCmd("QCGPrint", 4, QCirGatePrintCmd()) &&
          cmdMgr->regCmd("QC2ZX", 5, QCir2ZXCmd()) &&
          cmdMgr->regCmd("QC2TS", 5, QCir2TSCmd()) &&
          cmdMgr->regCmd("QCCDraw", 4, QCirDrawCmd()) &&
          cmdMgr->regCmd("QCCWrite", 4, QCirWriteCmd()))) {
        cerr << "Registering \"qcir\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType validQCirId = {
    [](ArgType<size_t> &arg) {
        return [&arg]() {
            return qcirMgr->isID(arg.getValue());
        };
    },
    [](ArgType<size_t> const &arg) {
        return [&arg]() {
            cerr << "Error: QCir " << arg.getValue() << " does not exist!!\n";
        };
    }};

ArgType<size_t>::ConstraintType validQCirGateId = {
    [](ArgType<size_t> &arg) {
        return [&arg]() {
            if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end())
                return false;
            else
                return (qcirMgr->getQCircuit()->getGate(arg.getValue()) != nullptr);
        };
    },
    [](ArgType<size_t> const &arg) {
        return [&arg]() {
            if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end())
                cerr << "Error: QCir list is empty now. Please QCNew/QCCRead/QCBAdd first.\n";
            else
                cerr << "Error: Gate id " << arg.getValue() << " does not exist!!\n";
        };
    }};

ArgType<size_t>::ConstraintType validQCirBitId = {
    [](ArgType<size_t> &arg) {
        return [&arg]() {
            if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end())
                return false;
            else
                return (qcirMgr->getQCircuit()->getQubit(arg.getValue()) != nullptr);
        };
    },
    [](ArgType<size_t> const &arg) {
        return [&arg]() {
            if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end())
                cerr << "Error: QCir list is empty now. Please QCNew/QCCRead/QCBAdd first.\n";
            else
                cerr << "Error: Qubit id " << arg.getValue() << " does not exist!!\n";
        };
    }};

//----------------------------------------------------------------------
//    QCCHeckout <(size_t id)>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirCheckOutCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCHeckout");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("checkout to QCir <id> in QCirMgr");

        parser.addArgument<size_t>("id")
            .constraint(validQCirId)
            .help("the ID of the circuit");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        qcirMgr->checkout2QCir(parser["id"]);
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCReset
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirResetCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCReset");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("reset QCirMgr");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        qcirMgr->reset();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCDelete <(size_t id)>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirDeleteCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCDelete");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("remove a QCir from QCirMgr");

        parser.addArgument<size_t>("id")
            .constraint(validQCirId)
            .help("the ID of the circuit");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        qcirMgr->removeQCir(parser["id"]);
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCNew [(size_t id)]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirNewCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCNew");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("create a new QCir to QCirMgr");

        parser.addArgument<size_t>("id")
            .required(false)
            .help("the ID of the circuit");

        parser.addArgument<bool>("-Replace")
            .action(storeTrue)
            .help("if specified, replace the current circuit; otherwise store to a new one");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (parser["id"].isParsed()) {
            if (qcirMgr->isID(parser["id"])) {
                if (parser["-Replace"].isParsed()) {
                    size_t repId = parser["id"];
                    QCir *qcir = new QCir(repId);
                    qcirMgr->setQCircuit(qcir);
                } else
                    cerr << "Error: QCir " << parser["id"] << " already exists!! Specify `-Replace` if needed." << endl;
            } else
                qcirMgr->addQCir(parser["id"]);
        } else {
            qcirMgr->addQCir(qcirMgr->getNextID());
        }
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCCOPy [size_t id] [-Replace]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirCopyCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCOPy");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("copy a QCir to QCirMgr");

        parser.addArgument<size_t>("id")
            .required(false)
            .help("the ID copied circuit to be stored");

        parser.addArgument<bool>("-Replace")
            .defaultValue(false)
            .action(storeTrue)
            .help("replace the current focused circuit");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCCOPy");
        if (parser["id"].isParsed()) {
            if (qcirMgr->isID(parser["id"])) {
                if (parser["-Replace"].isParsed()) {
                    qcirMgr->copy(parser["id"], false);
                } else
                    cerr << "Error: QCir " << parser["id"] << " already exists!! Specify `-Replace` if needed." << endl;
            } else
                qcirMgr->copy(parser["id"]);
        } else {
            qcirMgr->copy(qcirMgr->getNextID());
        }
        return CMD_EXEC_DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    QCCOMpose <size_t id>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirComposeCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCOMpose");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("compose a QCir");

        parser.addArgument<size_t>("id")
            .constraint(validQCirId)
            .help("the ID of the circuit to compose with");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        qcirMgr->getQCircuit()->compose(qcirMgr->findQCirByID(parser["id"]));
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCTensor <size_t id>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirTensorCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCTensor");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("tensor a QCir");

        parser.addArgument<size_t>("id")
            .constraint(validQCirId)
            .help("the ID of the circuit to tensor with");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        qcirMgr->getQCircuit()->tensorProduct(qcirMgr->findQCirByID(parser["id"]));
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCPrint [-SUmmary | -Focus | -Num | -SEttings]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCPrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCPrint");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("print info of QCirMgr or settings");

        auto mutex = parser.addMutuallyExclusiveGroup();

        mutex.addArgument<bool>("-summary")
            .action(storeTrue)
            .help("print summary of all circuits");
        mutex.addArgument<bool>("-focus")
            .action(storeTrue)
            .help("print the info of circuit in focus");
        mutex.addArgument<bool>("-list")
            .action(storeTrue)
            .help("print a list of circuits");
        mutex.addArgument<bool>("-number")
            .action(storeTrue)
            .help("print number of circuits");
        mutex.addArgument<bool>("-settings")
            .action(storeTrue)
            .help("print settings of circuit");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (parser["-settings"].isParsed()) {
            cout << endl;
            cout << "Delay of Single-qubit gate :     " << SINGLE_DELAY << endl;
            cout << "Delay of Double-qubit gate :     " << DOUBLE_DELAY << endl;
            cout << "Delay of SWAP gate :             " << SWAP_DELAY << ((SWAP_DELAY == 3 * DOUBLE_DELAY) ? " (3 CXs)" : "") << endl;
            cout << "Delay of Multiple-qubit gate :   " << MULTIPLE_DELAY << endl;
        } else if (parser["-focus"].isParsed())
            qcirMgr->printCListItr();
        else if (parser["-list"].isParsed())
            qcirMgr->printCList();
        else if (parser["-number"].isParsed())
            qcirMgr->printQCircuitListSize();
        else
            qcirMgr->printQCirMgr();

        return CMD_EXEC_DONE;
    };

    return cmd;
}

//------------------------------------------------------------------------------
//    QCSet ...
//------------------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCSetCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCSet");
    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("set QCir parameters");
        parser.addArgument<size_t>("-single-delay")
            .help("delay of single-qubit gate");
        parser.addArgument<size_t>("-double-delay")
            .help("delay of double-qubit gate, SWAP excluded");
        parser.addArgument<size_t>("-swap-delay")
            .help("delay of SWAP gate, used to be 3x double-qubit gate");
        parser.addArgument<size_t>("-multiple-delay")
            .help("delay of multiple-qubit gate");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (parser["-single-delay"].isParsed()) {
            size_t singleDelay = parser["-single-delay"];
            if (singleDelay == 0)
                cerr << "Error: single delay value should > 0, skipping this option!!\n";
            else
                SINGLE_DELAY = singleDelay;
        }
        if (parser["-double-delay"].isParsed()) {
            size_t doubleDelay = parser["-double-delay"];
            if (doubleDelay == 0)
                cerr << "Error: double delay value should > 0, skipping this option!!\n";
            else
                DOUBLE_DELAY = doubleDelay;
        }
        if (parser["-swap-delay"].isParsed()) {
            size_t swapDelay = parser["-swap-delay"];
            if (swapDelay == 0)
                cerr << "Error: swap delay value should > 0, skipping this option!!\n";
            else
                SWAP_DELAY = swapDelay;
        }
        if (parser["-multiple-delay"].isParsed()) {
            size_t multiDelay = parser["-multiple-delay"];
            if (multiDelay == 0)
                cerr << "Error: multiple delay value should > 0, skipping this option!!\n";
            else
                MULTIPLE_DELAY = multiDelay;
        }
        return CMD_EXEC_DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    QCCRead <(string fileName)> [-Replace]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirReadCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCRead");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("read a circuit and construct the corresponding netlist");

        parser.addArgument<string>("filepath")
            .help("the filepath to quantum circuit file");

        parser.addArgument<bool>("-replace")
            .action(storeTrue)
            .help("if specified, replace the current circuit; otherwise store to a new one");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        QCir *bufferQCir = new QCir(0);
        string filepath = parser["filepath"];
        bool replace = parser["-replace"];
        if (!bufferQCir->readQCirFile(filepath)) {
            cerr << "Error: The format in \"" << filepath << "\" has something wrong!!" << endl;
            delete bufferQCir;
            return CMD_EXEC_ERROR;
        }
        if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end()) {
            // cout << "Note: QCir list is empty now. Create a new one." << endl;
            qcirMgr->addQCir(qcirMgr->getNextID());
        } else {
            if (replace) {
                if (verbose >= 1) cout << "Note: original QCir is replaced..." << endl;
            } else {
                qcirMgr->addQCir(qcirMgr->getNextID());
            }
        }
        qcirMgr->setQCircuit(bufferQCir);
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCGPrint <(size_t gateID)> [-Time | -ZXform]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirGatePrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCGPrint");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("print gate info in QCir");

        parser.addArgument<size_t>("id")
            .constraint(validQCirGateId)
            .help("the id of the gate");

        auto mutex = parser.addMutuallyExclusiveGroup();

        mutex.addArgument<bool>("-time")
            .action(storeTrue)
            .help("print the execution time of the gate");
        mutex.addArgument<bool>("-zx-form")
            .action(storeTrue)
            .help("print the ZX form of the gate");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCGPrint");
        if (parser["-zx-form"].isParsed()) {
            cout << "\n> Gate " << parser["id"] << " (" << qcirMgr->getQCircuit()->getGate(parser["id"])->getTypeStr() << ")";
            qcirMgr->getQCircuit()->getGate(parser["id"])->getZXform()->printVertices();
        } else {
            qcirMgr->getQCircuit()->printGateInfo(parser["id"], parser["-time"].isParsed());
        }

        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCCPrint [-Summary | -Analysis | -Detail | -List | -Qubit]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirPrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCPrint");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("print info of QCir");

        auto mutex = parser.addMutuallyExclusiveGroup();

        mutex.addArgument<bool>("-summary")
            .action(storeTrue)
            .help("print summary of the circuit");
        mutex.addArgument<bool>("-analysis")
            .action(storeTrue)
            .help("virtually decompose the circuit and print information");
        mutex.addArgument<bool>("-detail")
            .action(storeTrue)
            .help("print the constitution of the circuit");
        mutex.addArgument<bool>("-list")
            .action(storeTrue)
            .help("print a list of gates in the circuit");
        mutex.addArgument<bool>("-qubit")
            .action(storeTrue)
            .help("print the circuit along the qubits");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCCPrint");
        if (parser["-analysis"].isParsed())
            qcirMgr->getQCircuit()->analysis();
        else if (parser["-detail"].isParsed())
            qcirMgr->getQCircuit()->analysis(true);
        else if (parser["-list"].isParsed())
            qcirMgr->getQCircuit()->printGates();
        else if (parser["-qubit"].isParsed())
            qcirMgr->getQCircuit()->printQubits();
        else
            qcirMgr->getQCircuit()->printSummary();

        return CMD_EXEC_DONE;
    };

    return cmd;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------
//     QCGAdd <-H | -X | -Z | -T | -TDG | -S | -SX> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-CX | -CZ> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-CCX | -CCZ> <(size_t ctrl1)> <(size_t ctrl2)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-P | -PX | -RZ | -RX> <-PHase (Phase phase_inp)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-MCP | -MCPX | -MCRZ| -MCRX> <-PHase (Phase phase_inp)> <(size_t ctrl1)> ... <(size_t ctrln)> <(size_t targ)> [-APpend|-PRepend]
//-----------------------------------------------------------------------------------------------------------------------------------------------
CmdExecStatus
QCirAddGateCmd::exec(const string &option) {
    QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCGAdd");
    // check option
    vector<string> options;
    if (!CmdExec::lexOptions(option, options))
        return CMD_EXEC_ERROR;
    if (options.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, "");

    bool flag = false;
    bool appendGate = true;
    size_t eraseIndex = 0;
    for (size_t i = 0, n = options.size(); i < n; ++i) {
        if (myStrNCmp("-APpend", options[i], 3) == 0) {
            if (flag)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            flag = true;
            eraseIndex = i;
        } else if (myStrNCmp("-PRepend", options[i], 3) == 0) {
            if (flag)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            appendGate = false;
            flag = true;
            eraseIndex = i;
        }
    }
    string flagStr = options[eraseIndex];
    if (flag)
        options.erase(options.begin() + eraseIndex);
    if (options.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, flagStr);
    string type = options[0];
    vector<size_t> qubits;
    // <-H | -X | -Z | -TG | -TDg | -S | -V | -Y | -SY | -SDG>
    if (myStrNCmp("-H", type, 2) == 0 || myStrNCmp("-X", type, 2) == 0 || myStrNCmp("-Z", type, 2) == 0 || myStrNCmp("-T", type, 2) == 0 ||
        myStrNCmp("-TDG", type, 4) == 0 || myStrNCmp("-S", type, 2) == 0 || myStrNCmp("-SX", type, 2) == 0 || myStrNCmp("-Y", type, 2) == 0 ||
        myStrNCmp("-SY", type, 3) == 0 || myStrNCmp("-SDG", type, 4) == 0) {
        if (options.size() == 1)
            return CmdExec::errorOption(CMD_OPT_MISSING, type);
        if (options.size() > 2)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[2]);
        unsigned id;
        if (!myStr2Uns(options[1], id)) {
            cerr << "Error: target ID should be a positive integer!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[1]);
        }
        if (qcirMgr->getQCircuit()->getQubit(id) == NULL) {
            cerr << "Error: qubit ID is not in current circuit!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[1]);
        }
        qubits.push_back(id);
        type = type.erase(0, 1);
        qcirMgr->getQCircuit()->addGate(type, qubits, Phase(0), appendGate);
    } else if (myStrNCmp("-CX", type, 3) == 0 || myStrNCmp("-CZ", type, 3) == 0) {
        if (options.size() < 3)
            return CmdExec::errorOption(CMD_OPT_MISSING, options[options.size() - 1]);
        if (options.size() > 3)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[3]);
        for (size_t i = 1; i < options.size(); i++) {
            unsigned id;
            if (!myStr2Uns(options[i], id)) {
                cerr << "Error: target ID should be a positive integer!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            }
            if (qcirMgr->getQCircuit()->getQubit(id) == NULL) {
                cerr << "Error: qubit ID is not in current circuit!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            }
            qubits.push_back(id);
        }
        type = type.erase(0, 1);
        qcirMgr->getQCircuit()->addGate(type, qubits, Phase(0), appendGate);
    } else if (myStrNCmp("-RZ", type, 3) == 0 || myStrNCmp("-P", type, 2) == 0 || myStrNCmp("-PX", type, 3) == 0 || myStrNCmp("-RX", type, 3) == 0) {
        Phase phase;
        if (options.size() == 1) {
            cerr << "Error: missing -PHase flag!!" << endl;
            return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);
        } else {
            if (myStrNCmp("-PHase", options[1], 3) != 0) {
                cerr << "Error: missing -PHase flag before (" << options[1] << ")!!" << endl;
                return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);
            } else {
                if (options.size() == 2) {
                    cerr << "Error: missing phase after -PHase flag!!" << endl;
                    return CmdExec::errorOption(CMD_OPT_MISSING, options[1]);
                } else {
                    // Check Phase Legal
                    if (!Phase::fromString(options[2], phase)) {
                        cerr << "Error: not a legal phase!!" << endl;
                        return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[2]);
                    }
                }
            }
        }
        if (options.size() < 4)
            return CmdExec::errorOption(CMD_OPT_MISSING, options[2]);
        if (options.size() > 4)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[4]);
        unsigned id;
        if (!myStr2Uns(options[3], id)) {
            cerr << "Error: target ID should be a positive integer!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
        }
        if (qcirMgr->getQCircuit()->getQubit(id) == NULL) {
            cerr << "Error: qubit ID is not in current circuit!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[3]);
        }
        qubits.push_back(id);
        type = type.erase(0, 1);
        qcirMgr->getQCircuit()->addGate(type, qubits, phase, appendGate);
    } else if (myStrNCmp("-MCP", type, 4) == 0 || myStrNCmp("-MCPX", type, 5) == 0 || myStrNCmp("-MCPY", type, 5) == 0 ||
               myStrNCmp("-MCRZ", type, 5) == 0 || myStrNCmp("-MCRX", type, 5) == 0 || myStrNCmp("-MCRY", type, 5) == 0) {
        Phase phase;
        if (options.size() == 1) {
            cerr << "Error: missing -PHase flag!!" << endl;
            return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);
        } else {
            if (myStrNCmp("-PHase", options[1], 3) != 0) {
                cerr << "Error: missing -PHase flag before (" << options[1] << ")!!" << endl;
                return CmdExec::errorOption(CMD_OPT_MISSING, options[0]);
            } else {
                if (options.size() == 2) {
                    cerr << "Error: missing phase after -PHase flag!!" << endl;
                    return CmdExec::errorOption(CMD_OPT_MISSING, options[1]);
                } else {
                    // Check Phase Legal
                    if (!Phase::fromString(options[2], phase)) {
                        cerr << "Error: not a legal phase!!" << endl;
                        return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[2]);
                    }
                }
            }
        }
        if (options.size() < 4)
            return CmdExec::errorOption(CMD_OPT_MISSING, options[2]);
        for (size_t i = 3; i < options.size(); i++) {
            unsigned id;
            if (!myStr2Uns(options[i], id)) {
                cerr << "Error: target ID should be a positive integer!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            }
            if (qcirMgr->getQCircuit()->getQubit(id) == NULL) {
                cerr << "Error: qubit ID is not in current circuit!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            }
            qubits.push_back(id);
        }
        type = type.erase(0, 1);
        qcirMgr->getQCircuit()->addGate(type, qubits, phase, appendGate);

    } else if (myStrNCmp("-CCX", type, 4) == 0 || myStrNCmp("-CCZ", type, 4) == 0) {
        if (options.size() < 4)
            return CmdExec::errorOption(CMD_OPT_MISSING, options[options.size() - 1]);
        if (options.size() > 4)
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[3]);
        for (size_t i = 1; i < options.size(); i++) {
            unsigned id;
            if (!myStr2Uns(options[i], id)) {
                cerr << "Error: target ID should be a positive integer!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            }
            if (qcirMgr->getQCircuit()->getQubit(id) == NULL) {
                cerr << "Error: qubit ID is not in current circuit!!" << endl;
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            }
            qubits.push_back(id);
        }
        type = type.erase(0, 1);
        qcirMgr->getQCircuit()->addGate(type, qubits, Phase(0), appendGate);
    } else {
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, type);
    }

    return CMD_EXEC_DONE;
}

void QCirAddGateCmd::usage() const {
    cout << "QCGAdd <-H | -X | -Z | -T | -TDG | -S | -SDG | -SX | -Y | -SY> <(size_t targ)> [-APpend|-PRepend]" << endl;
    cout << "QCGAdd <-CX | -CZ> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend]" << endl;
    cout << "QCGAdd <-CCX | -CCZ> <(size_t ctrl1)> <(size_t ctrl2)> <(size_t targ)> [-APpend|-PRepend]" << endl;
    cout << "QCGAdd <-P | -PX | -RZ | -RX> <-PHase (Phase phase_inp)> <(size_t targ)> [-APpend|-PRepend]" << endl;
    cout << "QCGAdd <-MCP | -MCPX | -MCRZ| -MCRX> <-PHase (Phase phase_inp)> <(size_t ctrl1)> ... <(size_t ctrln)> <(size_t targ)> [-APpend|-PRepend]" << endl;
}

void QCirAddGateCmd::summary() const {
    cout << setw(15) << left << "QCGAdd: "
         << "add quantum gate\n";
}

//----------------------------------------------------------------------
//    QCBAdd [size_t addNum]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirAddQubitCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCBAdd");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("add qubit(s)");

        parser.addArgument<size_t>("amount")
            .required(false)
            .help("the amount of qubits to be added");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (qcirMgr->getcListItr() == qcirMgr->getQCircuitList().end()) {
            cout << "Note: QCir list is empty now. Create a new one." << endl;
            qcirMgr->addQCir(qcirMgr->getNextID());
        }
        if (parser["amount"].isParsed())
            qcirMgr->getQCircuit()->addQubit(parser["amount"]);
        else
            qcirMgr->getQCircuit()->addQubit(1);
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCGDelete <(size_t gateID)>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirDeleteGateCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCGDelete");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("delete gate");

        parser.addArgument<size_t>("id")
            .constraint(validQCirGateId)
            .help("the id to be deleted");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        qcirMgr->getQCircuit()->removeGate(parser["id"]);
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCBDelete <(size_t qubitID)>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirDeleteQubitCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCBDelete");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("delete qubit");

        parser.addArgument<size_t>("id")
            .constraint(validQCirBitId)
            .help("the id to be deleted");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (!qcirMgr->getQCircuit()->removeQubit(parser["id"]))
            return CMD_EXEC_ERROR;
        else
            return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QC2ZX
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCir2ZXCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QC2ZX");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("convert QCir to ZX-graph");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QC2ZX");
        qcirMgr->getQCircuit()->ZXMapping();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QC2TS
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCir2TSCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QC2TS");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("convert QCir to tensor");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QC2TS");
        qcirMgr->getQCircuit()->tensorMapping();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCCWrite
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirWriteCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCWrite");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("write QCir to a QASM file");
        parser.addArgument<string>("output-path.qasm")
            .help("the filepath to output file");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCCWrite");
        if (!qcirMgr->getQCircuit()->writeQASM(parser["output-path.qasm"])) {
            cerr << "Error: path " << parser["output-path.qasm"] << " not found!!" << endl;
            return CMD_EXEC_ERROR;
        }
        return CMD_EXEC_DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> QCirDrawCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCDraw");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("Draw a QCir. This command relies on qiskit and pdflatex to be present in the system.");
        parser.addArgument<string>("output_path")
            .required(false)
            .defaultValue("")
            .help(
                "if specified, output the resulting drawing into this file. "
                "This argument is mandatory if the drawer is 'mpl' or 'latex'");
        parser.addArgument<string>("-drawer")
            .choices(std::initializer_list<string>{"text", "mpl", "latex", "latex_source"})
            .defaultValue("text")
            .help("the backend for drawing quantum circuit");
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        QC_CMD_MGR_NOT_EMPTY_OR_RETURN("QCCDraw");
        string drawer = parser["-drawer"];
        string outputPath = parser["output_path"];

        if (drawer == "latex" || drawer == "mpl") {
            if (outputPath.empty()) {
                cerr << "Error: Using drawer \"" << drawer << "\" requires an output destination!!" << endl;
                return CMD_EXEC_ERROR;
            }
        }

        if (!qcirMgr->getQCircuit()->draw(drawer, outputPath)) {
            cerr << "Error: could not draw the QCir successfully!!" << endl;
            return CMD_EXEC_ERROR;
        }

        return CMD_EXEC_DONE;
    };

    return cmd;
}