/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define argparse commands ]
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

#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "util/util.hpp"

using namespace std;
using namespace argparse;

// init

Command argparse_cmd();

bool add_argparse_cmds() {
    if (!(CLI.add_command(argparse_cmd()))) {
        fmt::println(stderr, "Registering \"argparse\" commands fails... exiting");
        return false;
    }
    return true;
}

Command argparse_cmd() {
    return {"argparse",
            [](ArgumentParser& parser) {
                parser.description("ArgParse package sandbox");
            },
            [](ArgumentParser const& parser) {
                parser.print_tokens();
                parser.print_arguments();

                return CmdExecResult::done;
            }};
}
