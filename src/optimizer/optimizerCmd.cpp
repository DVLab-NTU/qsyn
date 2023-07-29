/****************************************************************************
  FileName     [ optimizerCmd.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <iostream>
#include <string>

#include "cmdParser.h"
#include "optimizer.h"
#include "qcir.h"
#include "qcirCmd.h"
#include "qcirMgr.h"
#include "util.h"

using namespace std;
using namespace ArgParse;
extern size_t verbose;
extern int effLimit;
extern QCirMgr *qcirMgr;

unique_ptr<ArgParseCmdType> optimizeCmd();

bool initOptimizeCmd() {
    if (!(cmdMgr->regCmd("OPTimize", 3, optimizeCmd()))) {
        cerr << "Registering \"optimize\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    Optimize
//----------------------------------------------------------------------
unique_ptr<ArgParseCmdType> optimizeCmd() {
    auto cmd = make_unique<ArgParseCmdType>("OPTimize");

    cmd->precondition = []() { return qcirMgrNotEmpty("OPTimize"); };

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("optimize QCir");
        parser.addArgument<bool>("-physical")
            .defaultValue(false)
            .action(storeTrue)
            .help("optimize physical circuit, i.e preserve the swap path");
        parser.addArgument<bool>("-copy")
            .defaultValue(false)
            .action(storeTrue)
            .help("copy a circuit to perform optimization");
        parser.addArgument<bool>("-statistics")
            .defaultValue(false)
            .action(storeTrue)
            .help("count the number of rules operated in optimizer.");
        parser.addArgument<bool>("-trivial")
            .defaultValue(false)
            .action(storeTrue)
            .help("Use the trivial optimization.");
    };

    cmd->onParseSuccess = [](mythread::stop_token st, ArgumentParser const &parser) {
        Optimizer optimizer(qcirMgr->getQCircuit(), st);
        QCir *result;
        std::string procedure_str{};
        if (parser["-trivial"]) {
            result = optimizer.trivial_optimization();
            procedure_str = "Trivial Optimize";
        } else {
            result = optimizer.basic_optimization(!parser["-physical"], false, 1000, parser["-statistics"]);
            procedure_str = "Optimize";
        }
        if (result == nullptr) {
            cout << "Error: fail to optimize circuit." << endl;
            return CMD_EXEC_ERROR;
        }
        auto name = qcirMgr->getQCircuit()->getFileName();
        auto procedures = qcirMgr->getQCircuit()->getProcedures();

        if (parser["-copy"]) {
            qcirMgr->addQCir(qcirMgr->getNextID());
        }

        if (st.stop_requested()) {
            procedure_str += "[INT]";
        }

        qcirMgr->setQCircuit(result);
        qcirMgr->getQCircuit()->printCirInfo();

        qcirMgr->getQCircuit()->addProcedures(procedures);
        qcirMgr->getQCircuit()->addProcedure(procedure_str);

        return CMD_EXEC_DONE;
    };
    return cmd;
}
