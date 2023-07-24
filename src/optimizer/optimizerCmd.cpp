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

    cmd->onParseSuccess = [](std::stop_token st, ArgumentParser const &parser) {
        Optimizer Opt(qcirMgr->getQCircuit());
        QCir *result;
        if (parser["-trivial"])
            result = Opt.trivial_optimization();
        else
            result = Opt.basic_optimization(!parser["-physical"], false, 1000, parser["-statistics"]);
        if (result == nullptr) {
            cout << "Error: fail to optimize circuit." << endl;
        } else {
            if (parser["-copy"]) {
                qcirMgr->addQCir(qcirMgr->getNextID());
            }
            qcirMgr->setQCircuit(result);
            qcirMgr->getQCircuit()->printCirInfo();
        }
        return CMD_EXEC_DONE;
    };
    return cmd;
}