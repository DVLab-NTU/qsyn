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

Command argparseCmd();

bool initArgParseCmd() {
    if (!(cli.registerCommand("argparse", 1, argparseCmd()))) {
        fmt::println(stderr, "Registering \"argparse\" commands fails... exiting");
        return false;
    }
    return true;
}

Command argparseCmd() {
    return {"argparse",
            [](ArgumentParser& parser) {
                parser.description("ArgParse package sandbox");

                // parser.addArgument<bool>("-name", "-alias");
            },
            [](ArgumentParser const& parser) {
                parser.printTokens();
                parser.printArguments();

                return CmdExecResult::DONE;
            }};
}
