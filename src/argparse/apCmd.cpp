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
#include <memory>
#include <string>
#include <vector>

#include "apArgParser.h"
#include "util.h"

using namespace std;

// init

bool initArgParserCmd() {
    using namespace ArgParse;

    auto argparseCmd = make_unique<ArgParseCmdType>("Argparse");  // argparse package sandbox

    argparseCmd->parserDefinition = [](ArgumentParser& parser) {
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
        // parser.addArgument<unsigned>("-another")
        //     .defaultValue(5)
        //     .action(storeConst<unsigned>)
        //     .constValue(87)
        //     .help("another variable");

        // parser.addArgument<int>("-answer")
        //     .defaultValue(42)
        //     .constraint({
        //     [](ArgType<int> const& arg) {
        //         return [&arg]() {
        //             return arg.getValue() < 10 && arg.getValue() >= 1;
        //         };
        //     },
        //     [](ArgType<int> const& arg) {
        //         return [&arg]() {
        //             cerr << "Error: invalid choice for argument \"" << arg.getName() << ": please choose within range [1, 10)!!\n";
        //         };
        //     }
        //     })
        //     .help("the answer to everything");
    };

    argparseCmd->onParseSuccess = [](ArgumentParser const& parser) {
        cout << "Here's my cat, its name is " << parser["cat"] << endl;
        cout << parser["cat"] << " has a dog friend, " << parser["dog"] << ".\n"
             << endl;

        [[maybe_unused]] auto add = [](unsigned a, int b) {
            return a + b;
        };

        [[maybe_unused]] auto multiply = [](int a, int b) {
            return a * b;
        };

        // cout << "another + answer = " << add(parser["-another"], parser["-answer"]) << endl;
        // cout << "another * answer = " << multiply(parser["-another"], parser["-answer"]) << endl;

        return CMD_EXEC_DONE;
    };

    if (!(cmdMgr->regCmd("Argparse", 1, std::move(argparseCmd)))) {
        cerr << "Registering \"argparser\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}
// -------------------------------------------------
//     generic definition for ArgParseCmdType
// -------------------------------------------------

/**
 * @brief Check the soundness of the parser before initializing the command with parserDefinition;
 *
 * @return true if succeeded
 * @return false if failed
 */
bool ArgParseCmdType::initialize() {
    if (!parserDefinition) {
        printMissingParserDefinitionErrorMsg();
        return false;
    }
    if (!onParseSuccess) {
        printMissingOnParseSuccessErrorMsg();
        return false;
    }
    parserDefinition(_parser);
    return _parser.analyzeOptions();
}

/**
 * @brief parse the argument. On parse success, execute the onParseSuccess callback.
 *
 * @return true if succeeded
 * @return false if failed
 */
CmdExecStatus ArgParseCmdType::exec(const std::string& option) {
    if (!_parser.parse(option)) {
        return CMD_EXEC_ERROR;
    }
    return onParseSuccess(_parser);
}

void ArgParseCmdType::printMissingParserDefinitionErrorMsg() const {
    cerr << "[ArgParse] Error:   please define parser definition for command \"" << _parser.getName() << "\"!!\n"
         << "           Syntax:  <cmd>->parserDefinition = [](ArgumentParser& parser) -> void { ... }; " << endl;
}

void ArgParseCmdType::printMissingOnParseSuccessErrorMsg() const {
    cerr << "[ArgParse] Error:   please define on-parse-success action for command \"" << _parser.getName() << "\"!!\n"
         << "           Syntax:  <cmd>->onParseSuccess = [](ArgumentParser const& parser) -> CmdExecStatus { ... }; " << endl;
}
