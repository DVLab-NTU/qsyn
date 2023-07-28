/****************************************************************************
  FileName     [ latticeCmd.cpp ]
  PackageName  [ lattice ]
  Synopsis     [ Define lattice package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

#include "cmdParser.h"
#include "lattice.h"
#include "zxCmd.h"
#include "zxGraphMgr.h"

using namespace std;
using namespace ArgParse;
extern size_t verbose;
extern ZXGraphMgr zxGraphMgr;

unique_ptr<ArgParseCmdType> latticeSurgeryCompilationCmd();

bool initLTCmd() {
    if (!(
            cmdMgr->regCmd("LTS", 3, latticeSurgeryCompilationCmd())

                )) {
        cerr << "Registering \"lts\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------------------------------------------
//    LT [ -p ]
//------------------------------------------------------------------------------------------------------------------

unique_ptr<ArgParseCmdType> latticeSurgeryCompilationCmd() {
    auto cmd = make_unique<ArgParseCmdType>("LTS");

    cmd->precondition = []() { return zxGraphMgrNotEmpty("LTS"); };

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("(experimental) perform mapping from ZXGraph to corresponding lattice surgery");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        LTContainer lt(1, 1);
        lt.generateLTC(zxGraphMgr.get());
        return CMD_EXEC_DONE;
    };

    return cmd;
}
