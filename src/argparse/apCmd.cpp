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

// init

bool initArgParserCmd() {
    using namespace ArgParse;

    auto argparseCmd = make_unique<ArgParseCmdType>("Argparse");

    argparseCmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("ArgParse package sandbox");

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
    };

    argparseCmd->onParseSuccess = [](ArgumentParser const& parser) {
        parser.printTokens();
        parser.printArguments();

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
    return true;
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
