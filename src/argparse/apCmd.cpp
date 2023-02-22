/****************************************************************************
  FileName     [ argparserCmd.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define classes for argparser commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "apCmd.h"

#include <stdlib.h>  // for srand

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "apArgParser.h"
#include "util.h"

using namespace std;

bool initArgParserCmd() {
    if (!(cmdMgr->regCmd("Argparse", 1, new ArgParseCmd))) {
        cerr << "Registering \"argparser\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    Argparse [(...)]
//----------------------------------------------------------------------

void ArgParseCmd::parserDefinition() {
    using namespace ArgParse;
    
    parser.name("Argparse").help("ArgParse package sandbox");
    parser.addArgument<string>("cat")
        .help("cute");
    parser.addArgument<string>("dog")
        .help("humans' best friend");

    parser.addArgument<int>("-badge")
        .defaultValue(42)
        .help("a symbol of honor");
    parser.addArgument<unsigned>("-bacon")
        .defaultValue(5)
        .action(storeConst<unsigned>)
        .constValue(87)
        .help("yummy");
}

CmdExecStatus
ArgParseCmd::exec(const string& option) {
    parser.parse(option);

    parser.printTokens();
    parser.printArguments();

    return CMD_EXEC_DONE;
}
