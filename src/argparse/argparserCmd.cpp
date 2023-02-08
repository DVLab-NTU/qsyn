/****************************************************************************
  FileName     [ argparserCmd.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define classes for argparser commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "argparserCmd.h"

#include <stdlib.h>  // for srand

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "argparser.h"
#include "util.h"

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

void ArgParserCmd::parserDefinition() {
    parser.cmdInfo("Argparse", "argparse function playground");

    parser.addArgument<string>("reqpos")
        .help("Required positional argument");
    parser.addArgument<string>("optpos")
        .help("Optional positional argument")
        .optional();
    parser.addArgument<string>("-reqopt")
        .help("Required option")
        .required()
        .metavar("apple");
    parser.addArgument<string>("-optopt")
        .help("Optional option")
        .metavar("banana");
}

CmdExecStatus
ArgParserCmd::exec(const string& option) {

    parser.parse(option);

    parser.printArguments();
    parser.printTokens();
    
    // access parsed variable here. e.g.,
    // if (parser["-replace"]) {
    //    cout << "replace the current graph"
    //}
    // g->readZX(parser["filepath"], ...);

    return CMD_EXEC_DONE;
}
