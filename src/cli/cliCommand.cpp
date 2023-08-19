/****************************************************************************
  FileName     [ cliCommand.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Define cli command behaviour ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "cli/cli.hpp"

using namespace std;

/**
 * @brief Check the soundness of the parser before initializing the command with parserDefinition;
 *
 * @return true if succeeded
 * @return false if failed
 */
bool Command::initialize() {
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
CmdExecResult Command::exec(const std::string& option) {
    if (precondition && !precondition()) {
        return CmdExecResult::ERROR;
    }
    if (!_parser.parseArgs(option)) {
        return CmdExecResult::ERROR;
    }

    return onParseSuccess(_parser);
}

void Command::printMissingParserDefinitionErrorMsg() const {
    fmt::println(stderr, "[ArgParse] Error:   please define parser definition for command \"{}\"!!", _parser.getName());
    fmt::println(stderr, "           Syntax:  <cmd>->parserDefinition = [](ArgumentParser& parser) {{ ... }};");
}

void Command::printMissingOnParseSuccessErrorMsg() const {
    fmt::println(stderr, "[ArgParse] Error:   please define on-parse-success action for command \"{}\"!!", _parser.getName());
    fmt::println(stderr, "           Syntax:  <cmd>->onParseSuccess = [](ArgumentParser const& parser) {{ ... }};");
}
