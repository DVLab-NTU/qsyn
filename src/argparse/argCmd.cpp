/****************************************************************************
  FileName     [ argCmd.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define classes for argparse commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "argparse/argType.hpp"
#include "cli/cli.hpp"
#include "util/util.hpp"

using namespace std;
using namespace ArgParse;

// init

unique_ptr<Command> argparseCmd();

bool initArgParseCmd() {
    if (!(cli.registerCommand("Argparse", 1, argparseCmd()))) {
        fmt::println(stderr, "Registering \"argparse\" commands fails... exiting");
        return false;
    }
    return true;
}

unique_ptr<Command> argparseCmd() {
    auto cmd = make_unique<Command>("Argparse");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("ArgParse package sandbox");

        parser.addArgument<string>("-cat")
            .nargs(2, 4)
            .required(true)
            .usage("meow")
            .help("won't eat veggies");

        parser.addArgument<string>("-DOG")
            .action(storeConst("rocky"s))
            .defaultValue("good boi"s)
            .help("humans' best friend");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        parser.printTokens();
        parser.printArguments();

        auto cats = parser.get<ordered_hashset<string>>("cat");

        fmt::println("# cats = {}: {}", cats.size(), fmt::join(cats, " "));

        return CmdExecResult::DONE;
    };

    return cmd;
}
