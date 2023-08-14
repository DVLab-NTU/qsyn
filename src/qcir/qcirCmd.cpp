/****************************************************************************
  FileName     [ qcirCmd.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/qcirCmd.hpp"

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>

#include "./qcirGate.hpp"
#include "./qcirMgr.hpp"
#include "./toTensor.hpp"
#include "./toZXGraph.hpp"
#include "cli/cli.hpp"
#include "tensor/tensorMgr.hpp"
#include "util/phase.hpp"
#include "zx/zxGraphMgr.hpp"

QCirMgr qcirMgr{"QCir"};
extern ZXGraphMgr zxGraphMgr;
extern TensorMgr tensorMgr;
extern size_t verbose;
extern size_t dmode;
extern int effLimit;

using namespace std;
using namespace ArgParse;

bool qcirMgrNotEmpty(std::string const& command) {
    if (qcirMgr.empty()) {
        cerr << "Error: QCir list is empty now. Please QCNEW/QCCRead/QCBAdd before " << command << ".\n";
        return false;
    }
    return true;
}

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
unique_ptr<ArgParseCmdType> QCirAddGateCmd();
unique_ptr<ArgParseCmdType> QCirAddQubitCmd();
unique_ptr<ArgParseCmdType> QCirDeleteGateCmd();
unique_ptr<ArgParseCmdType> QCirDeleteQubitCmd();
unique_ptr<ArgParseCmdType> QCir2ZXCmd();
unique_ptr<ArgParseCmdType> QCir2TSCmd();
unique_ptr<ArgParseCmdType> QCirWriteCmd();
unique_ptr<ArgParseCmdType> QCirDrawCmd();

bool initQCirCmd() {
    if (!(cli.regCmd("QCCHeckout", 4, QCirCheckOutCmd()) &&
          cli.regCmd("QCReset", 3, QCirResetCmd()) &&
          cli.regCmd("QCDelete", 3, QCirDeleteCmd()) &&
          cli.regCmd("QCNew", 3, QCirNewCmd()) &&
          cli.regCmd("QCCOPy", 5, QCirCopyCmd()) &&
          cli.regCmd("QCCOMpose", 5, QCirComposeCmd()) &&
          cli.regCmd("QCTensor", 3, QCirTensorCmd()) &&
          cli.regCmd("QCPrint", 3, QCPrintCmd()) &&
          cli.regCmd("QCSet", 3, QCSetCmd()) &&
          cli.regCmd("QCCRead", 4, QCirReadCmd()) &&
          cli.regCmd("QCCPrint", 4, QCirPrintCmd()) &&
          cli.regCmd("QCGAdd", 4, QCirAddGateCmd()) &&
          cli.regCmd("QCBAdd", 4, QCirAddQubitCmd()) &&
          cli.regCmd("QCGDelete", 4, QCirDeleteGateCmd()) &&
          cli.regCmd("QCBDelete", 4, QCirDeleteQubitCmd()) &&
          cli.regCmd("QCGPrint", 4, QCirGatePrintCmd()) &&
          cli.regCmd("QC2ZX", 5, QCir2ZXCmd()) &&
          cli.regCmd("QC2TS", 5, QCir2TSCmd()) &&
          cli.regCmd("QCCDraw", 4, QCirDrawCmd()) &&
          cli.regCmd("QCCWrite", 4, QCirWriteCmd()))) {
        cerr << "Registering \"qcir\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType const validQCirId = {
    [](size_t const& id) {
        return qcirMgr.isID(id);
    },
    [](size_t const& id) {
        cerr << "Error: QCir " << id << " does not exist!!\n";
    }};

ArgType<size_t>::ConstraintType const validQCirGateId = {
    [](size_t const& id) {
        assert(!qcirMgr.empty());
        return (qcirMgr.get()->getGate(id) != nullptr);
    },
    [](size_t const& id) {
        assert(!qcirMgr.empty());
        cerr << "Error: Gate id " << id << " does not exist!!\n";
    }};

ArgType<size_t>::ConstraintType const validQCirBitId = {
    [](size_t const& id) {
        assert(!qcirMgr.empty());
        return (qcirMgr.get()->getQubit(id) != nullptr);
    },
    [](size_t const& id) {
        assert(!qcirMgr.empty());
        cerr << "Error: Qubit id " << id << " does not exist!!\n";
    }};

ArgType<size_t>::ConstraintType const validDMode = {
    [](size_t const& val) {
        return (val >= 0 && val <= 4);
    },
    [](size_t const& val) {
        cerr << "Error: DMode " << val << " does not exist!!\n";
    }};

//----------------------------------------------------------------------
//    QCCHeckout <(size_t id)>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirCheckOutCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCHeckout");

    cmd->precondition = []() { return qcirMgrNotEmpty("QCCHeckout"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("checkout to QCir <id> in QCirMgr");

        parser.addArgument<size_t>("id")
            .constraint(validQCirId)
            .help("the ID of the circuit");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        qcirMgr.checkout(parser["id"]);
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCReset
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirResetCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCReset");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("reset QCirMgr");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        qcirMgr.reset();
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCDelete <(size_t id)>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirDeleteCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCDelete");

    cmd->precondition = []() { return qcirMgrNotEmpty("QCDelete"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("remove a QCir from QCirMgr");

        parser.addArgument<size_t>("id")
            .constraint(validQCirId)
            .help("the ID of the circuit");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        qcirMgr.remove(parser["id"]);
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCNew [(size_t id)]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirNewCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCNew");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("create a new QCir to QCirMgr");

        parser.addArgument<size_t>("id")
            .nargs(NArgsOption::OPTIONAL)
            .help("the ID of the circuit");

        parser.addArgument<bool>("-replace")
            .action(storeTrue)
            .help("if specified, replace the current circuit; otherwise store to a new one");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        size_t id = parser["id"].isParsed() ? parser.get<size_t>("id") : qcirMgr.getNextID();

        if (qcirMgr.isID(id)) {
            if (!parser["-Replace"].isParsed()) {
                cerr << "Error: QCir " << id << " already exists!! Specify `-Replace` if needed.\n";
                return CmdExecResult::ERROR;
            }

            QCir* qcir = new QCir(id);
            qcirMgr.set(std::make_unique<QCir>(id));
        }

        qcirMgr.add(id);
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCCOPy [size_t id] [-Replace]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirCopyCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCOPy");

    cmd->precondition = []() { return qcirMgrNotEmpty("QCCOPy"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("copy a QCir to QCirMgr");

        parser.addArgument<size_t>("id")
            .nargs(NArgsOption::OPTIONAL)
            .help("the ID copied circuit to be stored");

        parser.addArgument<bool>("-replace")
            .defaultValue(false)
            .action(storeTrue)
            .help("replace the current focused circuit");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        size_t id = parser["id"].isParsed() ? parser.get<size_t>("id") : qcirMgr.getNextID();
        if (qcirMgr.isID(id) && !parser["-Replace"].isParsed()) {
            cerr << "Error: QCir " << id << " already exists!! Specify `-Replace` if needed." << endl;
            return CmdExecResult::ERROR;
        }
        qcirMgr.copy(id);

        return CmdExecResult::DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    QCCOMpose <size_t id>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirComposeCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCOMpose");

    cmd->precondition = []() { return qcirMgrNotEmpty("QCCOMpose"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("compose a QCir");

        parser.addArgument<size_t>("id")
            .constraint(validQCirId)
            .help("the ID of the circuit to compose with");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        qcirMgr.get()->compose(*qcirMgr.findByID(parser["id"]));
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCTensor <size_t id>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirTensorCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCTensor");

    cmd->precondition = []() { return qcirMgrNotEmpty("QCTensor"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("tensor a QCir");

        parser.addArgument<size_t>("id")
            .constraint(validQCirId)
            .help("the ID of the circuit to tensor with");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        qcirMgr.get()->tensorProduct(*qcirMgr.findByID(parser["id"]));
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCPrint [-SUmmary | -Focus | -Num | -SEttings]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCPrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCPrint");

    cmd->parserDefinition = [](ArgumentParser& parser) {
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

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["-settings"].isParsed()) {
            cout << endl;
            cout << "Delay of Single-qubit gate :     " << SINGLE_DELAY << endl;
            cout << "Delay of Double-qubit gate :     " << DOUBLE_DELAY << endl;
            cout << "Delay of SWAP gate :             " << SWAP_DELAY << ((SWAP_DELAY == 3 * DOUBLE_DELAY) ? " (3 CXs)" : "") << endl;
            cout << "Delay of Multiple-qubit gate :   " << MULTIPLE_DELAY << endl;
        } else if (parser["-focus"].isParsed())
            qcirMgr.printFocus();
        else if (parser["-list"].isParsed())
            qcirMgr.printList();
        else if (parser["-number"].isParsed())
            qcirMgr.printListSize();
        else
            qcirMgr.printMgr();

        return CmdExecResult::DONE;
    };

    return cmd;
}

//------------------------------------------------------------------------------
//    QCSet ...
//------------------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCSetCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCSet");
    cmd->parserDefinition = [](ArgumentParser& parser) {
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

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
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
        return CmdExecResult::DONE;
    };
    return cmd;
}

//----------------------------------------------------------------------
//    QCCRead <(string fileName)> [-Replace]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirReadCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCRead");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("read a circuit and construct the corresponding netlist");

        parser.addArgument<string>("filepath")
            .constraint(path_readable)
            .constraint(allowed_extension({".qasm", ".qc", ".qsim", ".quipper", ""}))
            .help("the filepath to quantum circuit file. Supported extension: .qasm, .qc, .qsim, .quipper");

        parser.addArgument<bool>("-replace")
            .action(storeTrue)
            .help("if specified, replace the current circuit; otherwise store to a new one");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        QCir bufferQCir;
        string filepath = parser["filepath"];
        bool replace = parser["-replace"];
        if (!bufferQCir.readQCirFile(filepath)) {
            cerr << "Error: The format in \"" << filepath << "\" has something wrong!!" << endl;
            return CmdExecResult::ERROR;
        }
        if (qcirMgr.empty()) {
            // cout << "Note: QCir list is empty now. Create a new one." << endl;
            qcirMgr.add(qcirMgr.getNextID());
        } else {
            if (replace) {
                if (verbose >= 1) cout << "Note: original QCir is replaced..." << endl;
            } else {
                qcirMgr.add(qcirMgr.getNextID());
            }
        }
        qcirMgr.set(std::make_unique<QCir>(std::move(bufferQCir)));
        qcirMgr.get()->setFileName(std::filesystem::path{filepath}.stem());
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCGPrint <(size_t gateID)> [-Time | -ZXform]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirGatePrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCGPrint");

    cmd->precondition = []() { return qcirMgrNotEmpty("QCGPrint"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
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

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["-zx-form"].isParsed()) {
            cout << "\n> Gate " << parser["id"] << " (" << qcirMgr.get()->getGate(parser["id"])->getTypeStr() << ")";
            toZXGraph(qcirMgr.get()->getGate(parser["id"]))->printVertices();
        } else {
            qcirMgr.get()->printGateInfo(parser["id"], parser["-time"].isParsed());
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCCPrint [-Summary | -Analysis | -Detail | -List | -Qubit]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirPrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCPrint");

    cmd->precondition = []() { return qcirMgrNotEmpty("QCCPrint"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
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

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["-analysis"].isParsed())
            qcirMgr.get()->countGate(false);
        else if (parser["-detail"].isParsed())
            qcirMgr.get()->countGate(true);
        else if (parser["-list"].isParsed())
            qcirMgr.get()->printGates();
        else if (parser["-qubit"].isParsed())
            qcirMgr.get()->printQubits();
        else if (parser["-summary"].isParsed())
            qcirMgr.get()->printSummary();
        else
            qcirMgr.get()->printCirInfo();

        return CmdExecResult::DONE;
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

unique_ptr<ArgParseCmdType> QCirAddGateCmd() {
    constexpr auto commandName = "QCGAdd";
    auto cmd = make_unique<ArgParseCmdType>(commandName);

    cmd->precondition = [=]() {
        return qcirMgrNotEmpty(commandName);
    };

    static ordered_hashmap<std::string, std::string> single_qubit_gates_no_phase = {
        {"-h", "Hadamard gate"},
        {"-x", "Pauli-X gate"},
        {"-y", "Pauli-Y gate"},
        {"-z", "Pauli-Z gate"},
        {"-t", "T gate"},
        {"-tdg", "T† gate"},
        {"-s", "S gate"},
        {"-sdg", "S† gate"},
        {"-sx", "√X gate"},
        {"-sy", "√Y gate"}};

    static ordered_hashmap<std::string, std::string> single_qubit_gates_with_phase = {
        {"-rz", "Rz(θ) gate"},
        {"-ry", "Rx(θ) gate"},
        {"-rx", "Ry(θ) gate"},
        {"-p", "P = (e^iθ/2)Rz gate"},
        {"-pz", "Pz = (e^iθ/2)Rz gate"},
        {"-px", "Px = (e^iθ/2)Rx gate"},
        {"-py", "Py = (e^iθ/2)Ry gate"}  //
    };

    static ordered_hashmap<std::string, std::string> double_qubit_gates_no_phase = {
        {"-cx", "CX (CNOT) gate"},
        {"-cz", "CZ gate"},
        // {"swap", "SWAP gate"}
    };

    static ordered_hashmap<std::string, std::string> three_qubit_gates_no_phase = {
        {"-ccx", "CCX (CCNOT, Toffoli) gate"},
        {"-ccz", "CCZ gate"}  //
    };

    static ordered_hashmap<std::string, std::string> multi_qubit_gates_with_phase = {
        {"-mcrz", "Multi-Controlled Rz(θ) gate"},
        {"-mcrx", "Multi-Controlled Rx(θ) gate"},
        {"-mcry", "Multi-Controlled Ry(θ) gate"},
        {"-mcp", "Multi-Controlled P(θ) gate"},
        {"-mcpz", "Multi-Controlled Pz(θ) gate"},
        {"-mcpx", "Multi-Controlled Px(θ) gate"},
        {"-mcpy", "Multi-Controlled Py(θ) gate"}  //
    };

    cmd->parserDefinition = [=](ArgumentParser& parser) {
        parser.help("add quantum gate");

        vector<string> typeChoices;
        std::string typeHelp =
            "the quantum gate type.\n"
            "For control gates, the control qubits comes before the target qubits.";

        for (auto& category : {
                 single_qubit_gates_no_phase,
                 single_qubit_gates_with_phase,
                 double_qubit_gates_no_phase,
                 three_qubit_gates_no_phase,
                 multi_qubit_gates_with_phase}) {
            for (auto& [name, help] : category) {
                typeChoices.emplace_back(name);
                typeHelp += '\n' + name + ": ";
                if (name.size() < 4) typeHelp += string(4 - name.size(), ' ');
                typeHelp += help;
            }
        }
        parser.addArgument<string>("type")
            .help(typeHelp)
            .constraint(choices_allow_prefix(typeChoices));

        auto append_or_prepend = parser.addMutuallyExclusiveGroup().required(false);
        append_or_prepend.addArgument<bool>("-append")
            .help("append the gate at the end of QCir")
            .action(storeTrue);
        append_or_prepend.addArgument<bool>("-PRepend")
            .help("prepend the gate at the start of QCir")
            .action(storeTrue);

        parser.addArgument<Phase>("-phase")
            .help("The rotation angle θ (default = π). This option must be specified if and only if the gate type takes a phase parameter.");

        parser.addArgument<size_t>("qubits")
            .nargs(NArgsOption::ZERO_OR_MORE)
            .constraint(validQCirBitId)
            .help("the qubits on which the gate applies");
    };

    cmd->onParseSuccess = [=](ArgumentParser const& parser) {
        bool doPrepend = parser["-prepend"].isParsed();

        string type = parser["type"];
        type = toLowerString(type);

        auto isGateCategory = [&](auto& category) {
            return any_of(category.begin(), category.end(),
                          [&](auto& name_help) {
                              return type == name_help.first;
                          });
        };

        Phase phase{1};
        if (isGateCategory(single_qubit_gates_with_phase) ||
            isGateCategory(multi_qubit_gates_with_phase)) {
            if (!parser["-phase"].isParsed()) {
                cerr << "Error: phase must be specified for gate type " << type << "!!\n";
                return CmdExecResult::ERROR;
            }
            phase = parser["-phase"];
        } else if (parser["-phase"].isParsed()) {
            cerr << "Error: phase is incompatible with gate type " << type << "!!\n";
            return CmdExecResult::ERROR;
        }

        vector<size_t> bits = parser["qubits"];

        if (isGateCategory(single_qubit_gates_no_phase) ||
            isGateCategory(single_qubit_gates_with_phase)) {
            if (bits.size() < 1) {
                cerr << "Error: too few qubits are supplied for gate " << type << "!!\n";
                return CmdExecResult::ERROR;
            } else if (bits.size() > 1) {
                cerr << "Error: too many qubits are supplied for gate " << type << "!!\n";
                return CmdExecResult::ERROR;
            }
        }

        if (isGateCategory(double_qubit_gates_no_phase)) {
            if (bits.size() < 2) {
                cerr << "Error: too few qubits are supplied for gate " << type << "!!\n";
                return CmdExecResult::ERROR;
            } else if (bits.size() > 2) {
                cerr << "Error: too many qubits are supplied for gate " << type << "!!\n";
                return CmdExecResult::ERROR;
            }
        }

        if (isGateCategory(three_qubit_gates_no_phase)) {
            if (bits.size() < 3) {
                cerr << "Error: too few qubits are supplied for gate " << type << "!!\n";
                return CmdExecResult::ERROR;
            } else if (bits.size() > 3) {
                cerr << "Error: too many qubits are supplied for gate " << type << "!!\n";
                return CmdExecResult::ERROR;
            }
        }

        qcirMgr.get()->addGate(type.substr(1), bits, phase, !doPrepend);

        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCBAdd [size_t addNum]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirAddQubitCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCBAdd");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("add qubit(s)");

        parser.addArgument<size_t>("amount")
            .nargs(NArgsOption::OPTIONAL)
            .help("the amount of qubits to be added");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (qcirMgr.empty()) {
            cout << "Note: QCir list is empty now. Create a new one." << endl;
            qcirMgr.add(qcirMgr.getNextID());
        }
        if (parser["amount"].isParsed())
            qcirMgr.get()->addQubit(parser["amount"]);
        else
            qcirMgr.get()->addQubit(1);
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCGDelete <(size_t gateID)>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirDeleteGateCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCGDelete");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("delete gate");

        parser.addArgument<size_t>("id")
            .constraint(validQCirGateId)
            .help("the id to be deleted");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        qcirMgr.get()->removeGate(parser["id"]);
        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCBDelete <(size_t qubitID)>
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirDeleteQubitCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCBDelete");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("delete qubit");

        parser.addArgument<size_t>("id")
            .constraint(validQCirBitId)
            .help("the id to be deleted");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (!qcirMgr.get()->removeQubit(parser["id"]))
            return CmdExecResult::ERROR;
        else
            return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QC2ZX
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCir2ZXCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QC2ZX");

    cmd->precondition = []() { return qcirMgrNotEmpty("QC2ZX"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("convert QCir to ZXGraph");

        // auto mutex = parser.addMutuallyExclusiveGroup();

        parser.addArgument<size_t>("dm")
            .nargs(NArgsOption::OPTIONAL)
            .constraint(validDMode)
            .help("decompose the graph in level 0");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["dm"].isParsed())
            dmode = parser["dm"];
        else
            dmode = 0;
        auto g = toZXGraph(*qcirMgr.get());

        if (g.has_value()) {
            zxGraphMgr.add(zxGraphMgr.getNextID());
            zxGraphMgr.set(std::make_unique<ZXGraph>(std::move(g.value())));

            zxGraphMgr.get()->setFileName(qcirMgr.get()->getFileName());
            zxGraphMgr.get()->addProcedures(qcirMgr.get()->getProcedures());
            zxGraphMgr.get()->addProcedure("QC2ZX");
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QC2TS
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCir2TSCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QC2TS");

    cmd->precondition = []() { return qcirMgrNotEmpty("QC2TS"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("convert QCir to tensor");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        auto tensor = toTensor(*qcirMgr.get());

        if (tensor.has_value()) {
            tensorMgr.add(tensorMgr.getNextID());
            tensorMgr.set(std::make_unique<QTensor<double>>(std::move(tensor.value())));

            tensorMgr.get()->setFileName(qcirMgr.get()->getFileName());
            tensorMgr.get()->addProcedures(qcirMgr.get()->getProcedures());
            tensorMgr.get()->addProcedure("QC2TS");
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}

//----------------------------------------------------------------------
//    QCCWrite
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> QCirWriteCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCWrite");

    cmd->precondition = []() { return qcirMgrNotEmpty("QCCWrite"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("write QCir to a QASM file");
        parser.addArgument<string>("output-path.qasm")
            .constraint(path_writable)
            .constraint(allowed_extension({".qasm"}))
            .help("the filepath to output file. Supported extension: .qasm");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (!qcirMgr.get()->writeQASM(parser["output-path.qasm"])) {
            cerr << "Error: path " << parser["output-path.qasm"] << " not found!!" << endl;
            return CmdExecResult::ERROR;
        }
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> QCirDrawCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QCCDraw");

    cmd->precondition = []() { return qcirMgrNotEmpty("QCCDraw"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("Draw a QCir. This command relies on qiskit and pdflatex to be present in the system.");
        parser.addArgument<string>("output_path")
            .nargs(NArgsOption::OPTIONAL)
            .constraint(path_writable)
            .defaultValue("")
            .help(
                "if specified, output the resulting drawing into this file. "
                "This argument is mandatory if the drawer is 'mpl' or 'latex'");
        parser.addArgument<string>("-drawer")
            .choices(std::initializer_list<string>{"text", "mpl", "latex", "latex_source"})
            .defaultValue("text")
            .help("the backend for drawing quantum circuit");
        parser.addArgument<float>("-scale")
            .defaultValue(1.0f)
            .help("if specified, scale the resulting drawing by this factor");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        string drawer = parser["-drawer"];
        string outputPath = parser["output_path"];
        float scale = parser["-scale"];

        if (drawer == "latex" || drawer == "mpl") {
            if (outputPath.empty()) {
                cerr << "Error: Using drawer \"" << drawer << "\" requires an output destination!!" << endl;
                return CmdExecResult::ERROR;
            }
        }

        if (drawer == "text" && parser["-scale"].isParsed()) {
            cerr << "Error: Cannot set scale for \'text\' drawer!!" << endl;
            return CmdExecResult::ERROR;
        }

        if (!qcirMgr.get()->draw(drawer, outputPath, scale)) {
            cerr << "Error: could not draw the QCir successfully!!" << endl;
            return CmdExecResult::ERROR;
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}
