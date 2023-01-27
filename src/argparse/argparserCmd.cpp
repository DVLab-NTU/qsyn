/****************************************************************************
  FileName     [ argparserCmd.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define classes for argparser commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "argparserCmd.h"

#include <stdlib.h>  // for srand

#include <cstddef>   // for size_t
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "util.h"
#include "argparser.h"

using namespace std;

bool initArgParserCmd() {
    if (!(cmdMgr->regCmd("Argparser", 1, new ArgParserCmd))) {
        cerr << "Registering \"argparser\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    PARSERTREE <size_t color level>
//----------------------------------------------------------------------

void 
ArgParserCmd::parserDefinition() {
    parser.cmdInfo("ZXGRead", "read a file and construct the corresponding ZX-graph");

    parser.addArgument<string>("filepath")
        .help("supports files with `.zx` or `.bzx` extension");
    parser.addArgument<bool>("mandatory-bool-arg")
        .help("test");
    parser.addArgument<bool>("-KEEPid")
        .defaultValue(false)
        .help("retain the vertex IDs from the input file (i.e., don't rearrange to consecutive numbers)");
    parser.addArgument<bool>("-replace")
        .defaultValue(false)
        .help("replace the current ZX-graph");
}

CmdExecStatus
ArgParserCmd::exec(const string& option) {
    parser.parse(option);

    parser.printTokens();
    // access parsed variable here. e.g.,
    // if (parser["-replace"]) {
    //    cout << "replace the current graph"
    //}
    // g->readZX(parser["filepath"], ...);

    return CMD_EXEC_DONE;
}
