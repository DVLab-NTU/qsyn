/****************************************************************************
  FileName     [ apCmd.cpp ]
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

bool initArgParseCmd() {
    if (!(cmdMgr->regCmd("Argparse", 1, argparseCmd()))) {
        cerr << "Registering \"argparser\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

unique_ptr<ArgParseCmdType> argparseCmd() {
    auto cmd = make_unique<ArgParseCmdType>("Argparse");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("ArgParse package sandbox");

        parser.addArgument<string>("cat")
            .help("won't eat veggies");

        parser.addArgument<string>("dog")
            .help("humans' best friend");

        auto subparsers = parser.addSubParsers()
                              .required(true)
                              .help("bird");

        auto gooseParser = subparsers.addParser("goose");

        gooseParser.addArgument<int>("honk");

        auto duckParser = subparsers.addParser("duck");

        duckParser.addArgument<string>("quack");
    };

    cmd->onParseSuccess = [](std::stop_token st, ArgumentParser const& parser) {
        parser.printTokens();
        parser.printArguments();

        if (parser.usedSubParser("goose")) {
            int honk = parser["honk"];
            cout << "honk = " << honk << endl;
        }
        if (parser.usedSubParser("duck")) {
            string quack = parser["quack"];
            cout << "quack = " << quack << endl;
        }

        return CMD_EXEC_DONE;
    };

    return cmd;
}
