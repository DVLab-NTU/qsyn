/****************************************************************************
  FileName     [ argparserCmd.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define classes for argparser commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <stdlib.h>  // for srand

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "cmdParser.h"
#include "util.h"

using namespace std;
using namespace ArgParse;

// init

unique_ptr<ArgParseCmdType> argparseCmd();

bool initArgParserCmd() {
    if (!(cmdMgr->regCmd("Argparse", 1, argparseCmd()))) {
        cerr << "Registering \"argparser\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

unique_ptr<ArgParseCmdType> argparseCmd() {
    auto cmd = make_unique<ArgParseCmdType>("Argparse");

    cmd->parserDefinition = [](ArgumentParser & parser) {
        parser.help("ArgParse package sandbox");

        parser.addArgument<string>("cat")
            .help("won't eat veggies");

        parser.addArgument<string>("dog")
            .help("humans' best friend");

        auto mutex1 = parser.addMutuallyExclusiveGroup().required(true);

        mutex1.addArgument<int>("-bacon")
            .help("yummy");

        mutex1.addArgument<int>("-badge")
            .help("a sign of honour");

        mutex1.addArgument<int>("-bus")
            .help("public transport");
    };

    cmd->onParseSuccess = [](std::stop_token st, ArgumentParser const& parser) {
        parser.printTokens();
        parser.printArguments();

        return CMD_EXEC_DONE;
    };

    return cmd;
}
