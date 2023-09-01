/****************************************************************************
  FileName     [ latticeCmd.cpp ]
  PackageName  [ lattice ]
  Synopsis     [ Define lattice package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <string>

#include "./lattice.hpp"
#include "cli/cli.hpp"
#include "zx/zxCmd.hpp"
#include "zx/zxGraphMgr.hpp"

using namespace std;
using namespace ArgParse;
extern ZXGraphMgr zxGraphMgr;

Command latticeSurgeryCompilationCmd();

bool initLTCmd() {
    if (!(
            cli.registerCommand("lts", 3, latticeSurgeryCompilationCmd())

                )) {
        cerr << "Registering \"lts\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------------------------------------------
//    LT [ -p ]
//------------------------------------------------------------------------------------------------------------------

Command latticeSurgeryCompilationCmd() {
    return {"lts",
            zxGraphMgrNotEmpty,
            [](ArgumentParser& parser) {
                parser.description("(experimental) perform mapping from ZXGraph to corresponding lattice surgery");

                parser.addArgument<bool>("-p")
                    .action(storeTrue)
                    .help("print the lattice surgery circuit");
            },
            [](ArgumentParser const& parser) {
                LTContainer lt(1, 1);
                lt.generateLTC(zxGraphMgr.get());
                return CmdExecResult::DONE;
            }};
}
