/****************************************************************************
  FileName     [ duostraCmd.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define Duostra package commands ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "duostraCmd.h"

#include <cstddef>   // for size_t
#include <iostream>  // for ostream
#include <string>    // for string

#include "apCmd.h"
#include "cmdMacros.h"    // for CMD_N_OPTS_EQUAL_OR_RETURN, CMD_N_OPTS_AT_LE...
#include "duostra.h"      // for Duostra
#include "qcir.h"         // for QCir
#include "qcirCmd.h"      // for QC_CMD_ID_VALID_OR_RETURN, QC_CMD_QCIR_ID_EX...
#include "qcirMgr.h"      // for QCirMgr
#include "topologyMgr.h"  // for DeviceMgr
#include "util.h"         // for myStr2Uns

using namespace std;
extern size_t verbose;
extern int effLimit;
extern QCirMgr *qcirMgr;
extern DeviceMgr *deviceMgr;

bool initDuostraCmd() {
    using namespace ArgParse;

    // SECTION - DUOSET

    auto duostraSetCmd = make_unique<ArgParseCmdType>("DUOSET");
    duostraSetCmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("set Duostra parameter");

        parser.addArgument<string>("-scheduler")
            .defaultValue(getSchedulerTypeStr())
            // .choices({"base", "static", "random", "greedy", "search"})
            .help("scheduler");
        parser.addArgument<string>("-router")
            .defaultValue(getRouterTypeStr())
            // .choices({"apsp", "duostra"})
            .help("router");
        parser.addArgument<string>("-placer")
            .defaultValue(getPlacerTypeStr())
            // .choices({"static", "random", "dfs"})
            .help("placer");

        parser.addArgument<bool>("-orient")
            .defaultValue(DUOSTRA_ORIENT)
            .help("smaller logical qubit index with little priority");

        int cand = (DUOSTRA_CANDIDATES == (size_t)-1) ? -1 : DUOSTRA_CANDIDATES;
        parser.addArgument<int>("-candidates")
            .defaultValue(cand)
            .help("top k candidates");

        parser.addArgument<int>("-apsp_coeff")
            .defaultValue(DUOSTRA_APSP_COEFF)
            .help("coefficient of apsp cost");

        parser.addArgument<string>("-available")
            .defaultValue((DUOSTRA_AVAILABLE == 0) ? "min" : "max")
            // .choices({"min", "max"})
            .help("available time of double-qubit gate is set to min or max of occupied time");

        parser.addArgument<string>("-cost")
            .defaultValue((DUOSTRA_COST == 0) ? "min" : "max")
            // .choices({"min", "max"})
            .help("select min or max cost from the waitlist");

        parser.addArgument<int>("-depth")
            .defaultValue(DUOSTRA_DEPTH)
            .help("depth of searching region");

        parser.addArgument<bool>("-never_cache")
            .defaultValue(DUOSTRA_NEVER_CACHE)
            .help("never cache any children unless children() is called");

        parser.addArgument<bool>("-single_immediately")
            .defaultValue(DUOSTRA_EXECUTE_SINGLE)
            .help("execute the single gates when they are available");
    };

    duostraSetCmd->onParseSuccess = [](ArgumentParser const &parser) {
        DUOSTRA_SCHEDULER = getSchedulerType(parser["-scheduler"]);
        DUOSTRA_ROUTER = getRouterType(parser["-router"]);
        DUOSTRA_PLACER = getPlacerType(parser["-placer"]);
        DUOSTRA_ORIENT = parser["-orient"];

        int resCand = parser["-candidates"];
        DUOSTRA_CANDIDATES = (size_t)resCand;

        int resAPSP = parser["-apsp_coeff"];
        DUOSTRA_APSP_COEFF = (size_t)resAPSP;

        string resAvail = parser["-available"];
        DUOSTRA_AVAILABLE = (resAvail == "min") ? false : true;

        string resCost = parser["-cost"];
        DUOSTRA_COST = (resCost == "min") ? false : true;

        int resDepth = parser["-depth"];
        DUOSTRA_DEPTH = (size_t)resDepth;

        DUOSTRA_NEVER_CACHE = parser["-never_cache"];
        DUOSTRA_EXECUTE_SINGLE = parser["-single_immediately"];

        return CMD_EXEC_DONE;
    };

    // SECTION - DUOPrint

    auto duostraPrintCmd = make_unique<ArgParseCmdType>("DUOPrint");
    duostraPrintCmd->parserDefinition = [](ArgumentParser &parser) {
        parser.addArgument<bool>("-detail")
            .defaultValue(false)
            .action(storeTrue)
            .help("print detailed information");
    };

    duostraPrintCmd->onParseSuccess = [](ArgumentParser const &parser) {
        cout << endl;
        cout << "Scheduler:         " << getSchedulerTypeStr() << endl;
        cout << "Router:            " << getRouterTypeStr() << endl;
        cout << "Placer:            " << getPlacerTypeStr() << endl;

        if (parser["-detail"]) {
            cout << endl;
            cout << "Candidates:        " << ((DUOSTRA_CANDIDATES == size_t(-1)) ? "-1" : to_string(DUOSTRA_CANDIDATES)) << endl;
            cout << "Search Depth:      " << DUOSTRA_DEPTH << endl;
            cout << endl;
            cout << "Orient:            " << ((DUOSTRA_ORIENT == 1) ? "true" : "false") << endl;
            cout << "APSP Coeff.:       " << DUOSTRA_APSP_COEFF << endl;
            cout << "Available Time:    " << ((DUOSTRA_AVAILABLE == 0) ? "min" : "max") << endl;
            cout << "Prefer Cost:       " << ((DUOSTRA_COST == 0) ? "min" : "max") << endl;
            cout << "Never Cache:       " << ((DUOSTRA_NEVER_CACHE == 1) ? "true" : "false") << endl;
            cout << "Single Immed.:     " << ((DUOSTRA_EXECUTE_SINGLE == 1) ? "true" : "false") << endl;
        }
        return CMD_EXEC_DONE;
    };

    if (!(cmdMgr->regCmd("DUOSTRA", 7, make_unique<DuostraCmd>()) &&
          cmdMgr->regCmd("DUOSET", 6, move(duostraSetCmd)) &&
          cmdMgr->regCmd("DUOPrint", 4, move(duostraPrintCmd)))) {
        cerr << "Registering \"Duostra\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
//    DUOSTRA
//------------------------------------------------------------------------------
CmdExecStatus
DuostraCmd::exec(const string &option) {
    if (!CmdExec::lexNoOption(option))
        return CMD_EXEC_ERROR;

    QC_CMD_MGR_NOT_EMPTY_OR_RETURN("DUOSTRA");
    Duostra duo = Duostra(qcirMgr->getQCircuit(), deviceMgr->getDevice());
    duo.flow();
    QCir *result = duo.getPhysicalCircuit();
    if (result != nullptr) {
        qcirMgr->addQCir(qcirMgr->getNextID());
        result->setId(qcirMgr->getNextID());
        qcirMgr->setQCircuit(result);
    } else {
        cerr << "Error: Something wrong in Duostra Mapping!!" << endl;
    }
    return CMD_EXEC_DONE;
}

void DuostraCmd::usage() const {
    cout << "Usage: DUOSTRA" << endl;
}

void DuostraCmd::summary() const {
    cout << setw(15) << left << "DUOSTRA: "
         << "map logical circuit to physical circuit" << endl;
}

// //------------------------------------------------------------------------------
// //    DUOSET
// //------------------------------------------------------------------------------
// CmdExecStatus
// DuostraSetCmd::exec(const string &option) {

//     return CMD_EXEC_DONE;
// }

// void DuostraSetCmd::usage() const {
//     cout << "Usage: DUOSET" << endl;
// }

// void DuostraSetCmd::summary() const {
//     cout << setw(15) << left << "DUOSET: "
//          << "set Duostra parameter" << endl;
// }