/****************************************************************************
  FileName     [ optimizerCmd.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>   // for size_t
#include <iostream>  // for ostream
#include <string>    // for string

#include "apCmd.h"
#include "optimizer.h"  // for Extractor
#include "qcir.h"       // for QCir
#include "qcirCmd.h"    // for QC_CMD_ID_VALID_OR_RETURN, QC_CMD_QCIR_ID_EX...
#include "qcirMgr.h"    // for QCirMgr
#include "util.h"       // for myStr2Uns

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
    };

    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        QC_CMD_MGR_NOT_EMPTY_OR_RETURN("OPTimize");
        Optimizer Opt(qcirMgr->getQCircuit());
        QCir *result = Opt.parseCircuit(!parser["-physical"], false, 1000);
        if (result == nullptr) {
            cout << "Error: fail to optimize circuit." << endl;
        } else {
            if (parser["-copy"])
                qcirMgr->addQCir(qcirMgr->getNextID());
            qcirMgr->setQCircuit(result);
        }
        return CMD_EXEC_DONE;
    };
    return cmd;
}