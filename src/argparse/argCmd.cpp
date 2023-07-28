/****************************************************************************
  FileName     [ argCmd.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define classes for argparse commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
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
            .nargs(NArgsOption::ZERO_OR_MORE)
            .help("won't eat veggies");

        parser.addArgument<string>("-dog")
            .action(storeConst("rocky"s))
            .defaultValue("good boi"s)
            .help("humans' best friend");
    };

    cmd->onParseSuccess = [](mythread::stop_token st, ArgumentParser const& parser) {
        parser.printTokens();
        parser.printArguments();

        ordered_hashset<string> cats = parser["cat"];

        cout << "# cats = " << cats.size() << ":";
        for (auto& name : cats) cout << " " << name;
        cout << endl;

        return CMD_EXEC_DONE;
    };

    return cmd;
}
