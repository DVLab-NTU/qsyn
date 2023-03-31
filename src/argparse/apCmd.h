/****************************************************************************
  FileName     [ argparserCmd.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define classes for argparser commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef QSYN_ARG_PARSER_CMD_H
#define QSYN_ARG_PARSER_CMD_H

#include "apArgParser.h"
#include "cmdParser.h"

/**
 * @brief class specification for commands that uses ArgParse::ArgumentParser to parse and generate help messages.
 *
 */
class ArgParseCmdType : public CmdExec {
public:
    ArgParseCmdType(std::string const& name) { _parser.name(name); }
    ~ArgParseCmdType() {}

    bool initialize() override;
    CmdExecStatus exec(const std::string& option) override;
    void usage() const override { _parser.printUsage(); }
    void summary() const override { _parser.printSummary(); }
    void help() const override { _parser.printHelp(); }

    std::function<void(ArgParse::ArgumentParser&)> parserDefinition;               // define the parser's arguments and traits
    std::function<bool()> precondition;                                            // define the parsing precondition
    std::function<CmdExecStatus(ArgParse::ArgumentParser const&)> onParseSuccess;  // define the action to take on parse success

private:
    ArgParse::ArgumentParser _parser;

    void printMissingParserDefinitionErrorMsg() const;
    void printMissingOnParseSuccessErrorMsg() const;
};

#endif  // QSYN_ARG_PARSER_CMD_H
