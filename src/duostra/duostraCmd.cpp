/****************************************************************************
  FileName     [ duostraCmd.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define Duostra package commands ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <iostream>
#include <string>

#include "cmdParser.h"
#include "deviceMgr.h"
#include "duostra.h"
#include "mappingEQChecker.h"
#include "qcir.h"
#include "qcirCmd.h"
#include "qcirMgr.h"
#include "textFormat.h"
#include "variables.h"

using namespace std;
using namespace ArgParse;
namespace TF = TextFormat;
extern size_t verbose;
extern int effLimit;
extern QCirMgr qcirMgr;
extern DeviceMgr* deviceMgr;

unique_ptr<ArgParseCmdType> duostraCmd();
unique_ptr<ArgParseCmdType> duostraPrintCmd();
unique_ptr<ArgParseCmdType> duostraSetCmd();
unique_ptr<ArgParseCmdType> mapEQCmd();

bool initDuostraCmd() {
    if (!(cli.regCmd("DUOSTRA", 7, duostraCmd()) &&
          cli.regCmd("DUOSET", 6, duostraSetCmd()) &&
          cli.regCmd("DUOPrint", 4, duostraPrintCmd()) &&
          cli.regCmd("MPEQuiv", 4, mapEQCmd()))) {
        cerr << "Registering \"Duostra\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
//    DUOSTRA
//------------------------------------------------------------------------------
unique_ptr<ArgParseCmdType> duostraCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DUOSTRA");

    cmd->precondition = []() { return qcirMgrNotEmpty("DUOSTRA") && deviceMgrNotEmpty(); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("map logical circuit to physical circuit");
        parser.addArgument<bool>("-check")
            .defaultValue(false)
            .action(storeTrue)
            .help("check whether the mapping result is correct");
        parser.addArgument<bool>("-mute-tqdm")
            .defaultValue(false)
            .action(storeTrue)
            .help("mute tqdm");
        parser.addArgument<bool>("-silent")
            .defaultValue(false)
            .action(storeTrue)
            .help("mute all messages");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        QCir* logicalQCir = qcirMgr.get();
        Duostra duo{logicalQCir, deviceMgr->getDevice(), parser["-check"], !parser["-mute-tqdm"], parser["-silent"]};
        if (duo.flow() == ERROR_CODE) {
            return CmdExecStatus::ERROR;
        }

        if (duo.getPhysicalCircuit() == nullptr) {
            cerr << "Error: Something wrong in Duostra Mapping!!" << endl;
        }
        size_t id = qcirMgr.getNextID();
        qcirMgr.add(id);
        qcirMgr.set(std::move(duo.getPhysicalCircuit()));

        qcirMgr.get()->setId(id);
        qcirMgr.get()->setFileName(logicalQCir->getFileName());
        qcirMgr.get()->addProcedures(logicalQCir->getProcedures());
        qcirMgr.get()->addProcedure("Duostra");

        return CmdExecStatus::DONE;
    };
    return cmd;
}

//------------------------------------------------------------------------------
//    DUOSET  ..... neglect
//------------------------------------------------------------------------------
unique_ptr<ArgParseCmdType> duostraSetCmd() {
    auto duostraSetCmd = make_unique<ArgParseCmdType>("DUOSET");
    duostraSetCmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("set Duostra parameter(s)");

        parser.addArgument<string>("-scheduler")
            .choices({"base", "static", "random", "greedy", "search"})
            .help("< base   | static | random | greedy | search >");
        parser.addArgument<string>("-router")
            .choices({"apsp", "duostra"})
            .help("< apsp   | duostra >");
        parser.addArgument<string>("-placer")
            .choices({"static", "random", "dfs"})
            .help("< static | random | dfs >");

        parser.addArgument<bool>("-orient")
            .help("smaller logical qubit index with little priority");

        parser.addArgument<int>("-candidates")
            .help("top k candidates");

        parser.addArgument<size_t>("-apsp-coeff")
            .help("coefficient of apsp cost");

        parser.addArgument<string>("-available")
            .choices({"min", "max"})
            .help("available time of double-qubit gate is set to min or max of occupied time");

        parser.addArgument<string>("-cost")
            .choices({"min", "max"})
            .help("select min or max cost from the waitlist");

        parser.addArgument<int>("-depth")
            .help("depth of searching region");

        parser.addArgument<bool>("-never-cache")
            .help("never cache any children unless children() is called");

        parser.addArgument<bool>("-single-immediately")
            .help("execute the single gates when they are available");
    };

    duostraSetCmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["-scheduler"].isParsed())
            DUOSTRA_SCHEDULER = getSchedulerType(parser["-scheduler"]);
        if (parser["-router"].isParsed())
            DUOSTRA_ROUTER = getRouterType(parser["-router"]);
        if (parser["-placer"].isParsed())
            DUOSTRA_PLACER = getPlacerType(parser["-placer"]);
        if (parser["-orient"].isParsed())
            DUOSTRA_ORIENT = parser["-orient"];

        if (parser["-candidates"].isParsed()) {
            int resCand = parser["-candidates"];
            DUOSTRA_CANDIDATES = (size_t)resCand;
        }

        if (parser["-apsp-coeff"].isParsed()) {
            DUOSTRA_APSP_COEFF = parser["-apsp-coeff"];
        }

        if (parser["-available"].isParsed()) {
            string resAvail = parser["-available"];
            DUOSTRA_AVAILABLE = (resAvail == "min") ? false : true;
        }

        if (parser["-cost"].isParsed()) {
            string resCost = parser["-cost"];
            DUOSTRA_COST = (resCost == "min") ? false : true;
        }

        if (parser["-depth"].isParsed()) {
            int resDepth = parser["-depth"];
            DUOSTRA_DEPTH = (size_t)resDepth;
        }

        if (parser["-never-cache"].isParsed()) {
            DUOSTRA_NEVER_CACHE = parser["-never-cache"];
        }

        if (parser["-single-immediately"].isParsed()) {
            DUOSTRA_EXECUTE_SINGLE = parser["-single-immediately"];
        }

        return CmdExecStatus::DONE;
    };
    return duostraSetCmd;
}

//------------------------------------------------------------------------------
//    DUOPrint [-detail]
//------------------------------------------------------------------------------
unique_ptr<ArgParseCmdType> duostraPrintCmd() {
    // SECTION - DUOPrint

    auto duostraPrintCmd = make_unique<ArgParseCmdType>("DUOPrint");
    duostraPrintCmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print Duostra parameters");
        parser.addArgument<bool>("-detail")
            .defaultValue(false)
            .action(storeTrue)
            .help("print detailed information");
    };

    duostraPrintCmd->onParseSuccess = [](ArgumentParser const& parser) {
        cout << '\n'
             << "Scheduler:         " << getSchedulerTypeStr() << '\n'
             << "Router:            " << getRouterTypeStr() << '\n'
             << "Placer:            " << getPlacerTypeStr() << endl;

        if (parser["-detail"]) {
            cout << '\n'
                 << "Candidates:        " << ((DUOSTRA_CANDIDATES == size_t(-1)) ? "-1" : to_string(DUOSTRA_CANDIDATES)) << '\n'
                 << "Search Depth:      " << DUOSTRA_DEPTH << '\n'
                 << '\n'
                 << "Orient:            " << ((DUOSTRA_ORIENT == 1) ? "true" : "false") << '\n'
                 << "APSP Coeff.:       " << DUOSTRA_APSP_COEFF << '\n'
                 << "Available Time:    " << ((DUOSTRA_AVAILABLE == 0) ? "min" : "max") << '\n'
                 << "Prefer Cost:       " << ((DUOSTRA_COST == 0) ? "min" : "max") << '\n'
                 << "Never Cache:       " << ((DUOSTRA_NEVER_CACHE == 1) ? "true" : "false") << '\n'
                 << "Single Immed.:     " << ((DUOSTRA_EXECUTE_SINGLE == 1) ? "true" : "false") << endl;
        }
        return CmdExecStatus::DONE;
    };
    return duostraPrintCmd;
}

unique_ptr<ArgParseCmdType> mapEQCmd() {
    auto cmd = make_unique<ArgParseCmdType>("MPEQuiv");

    cmd->precondition = []() { return qcirMgrNotEmpty("MPEQuiv") && deviceMgrNotEmpty(); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("check equivalence of the physical and the logical circuits");
        parser.addArgument<size_t>("-logical")
            .required(true)
            .help("logical circuit id");
        parser.addArgument<size_t>("-physical")
            .required(true)
            .help("physical circuit id");
        parser.addArgument<bool>("-reverse")
            .defaultValue(false)
            .action(storeTrue)
            .help("check the circuit reversily, used in extracted circuit");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (qcirMgr.findByID(parser["-physical"]) == nullptr || qcirMgr.findByID(parser["-logical"]) == nullptr) {
            return CmdExecStatus::ERROR;
        }
        MappingEQChecker mpeqc(qcirMgr.findByID(parser["-physical"]), qcirMgr.findByID(parser["-logical"]), deviceMgr->getDevice(), {});
        if (mpeqc.check()) {
            cout << TF::BOLD(TF::GREEN("Equivalent up to permutation")) << endl;
        } else {
            cout << TF::BOLD(TF::RED("Not Equivalent")) << endl;
        }
        return CmdExecStatus::DONE;
    };

    return cmd;
}
