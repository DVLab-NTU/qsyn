/****************************************************************************
  FileName     [ cliCommand.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Define cli command behaviour ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "cli/cli.hpp"

using namespace std;

void Command::addSubCommand(Command const& cmd) {
    auto oldDefinition = this->_parserDefinition;
    auto oldOnParseSuccess = this->_onParseSuccess;
    this->_parserDefinition = [cmd, oldDefinition](ArgParse::ArgumentParser& parser) {
        oldDefinition(parser);
        if (!parser.hasSubParsers()) parser.addSubParsers();
        auto subparsers = parser.getSubParsers().value();
        auto subparser = subparsers.addParser(cmd._parser.getName());

        cmd._parserDefinition(subparser);
    };

    this->_onParseSuccess = [cmd, oldOnParseSuccess](ArgParse::ArgumentParser const& parser) {
        if (parser.usedSubParser(cmd._parser.getName())) {
            return cmd._onParseSuccess(parser);
        }

        return oldOnParseSuccess(parser);
    };
}

/**
 * @brief Check the soundness of the parser before initializing the command with parserDefinition;
 *
 * @return true if succeeded
 * @return false if failed
 */
bool Command::initialize(size_t numRequiredChars) {
    if (!_parserDefinition) {
        printMissingParserDefinitionErrorMsg();
        return false;
    }
    if (!_onParseSuccess) {
        printMissingOnParseSuccessErrorMsg();
        return false;
    }
    _parserDefinition(_parser);
    _parser.numRequiredChars(numRequiredChars);
    return _parser.analyzeOptions();
}

/**
 * @brief parse the argument. On parse success, execute the onParseSuccess callback.
 *
 * @return true if succeeded
 * @return false if failed
 */
CmdExecResult Command::exec(const std::string& option) {
    if (_precondition && !_precondition()) {
        return CmdExecResult::ERROR;
    }
    if (!_parser.parseArgs(option)) {
        return CmdExecResult::ERROR;
    }

    return _onParseSuccess(_parser);
}

void Command::printMissingParserDefinitionErrorMsg() const {
    fmt::println(stderr, "[ArgParse] Error:   please define parser definition for command \"{}\"!!", _parser.getName());
    fmt::println(stderr, "           Syntax:  <cmd>->parserDefinition = [](ArgumentParser& parser) {{ ... }};");
}

void Command::printMissingOnParseSuccessErrorMsg() const {
    fmt::println(stderr, "[ArgParse] Error:   please define on-parse-success action for command \"{}\"!!", _parser.getName());
    fmt::println(stderr, "           Syntax:  <cmd>->onParseSuccess = [](ArgumentParser const& parser) {{ ... }};");
}
