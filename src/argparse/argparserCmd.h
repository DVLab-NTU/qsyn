/****************************************************************************
  FileName     [ argparserCmd.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define classes for argparser commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef QSYN_ARG_PARSER_CMD_H
#define QSYN_ARG_PARSER_CMD_H

#include "cmdParser.h"
#include "argparser.h"

class ArgParserCmd : public CmdExec {
public: 
    ArgParserCmd() { 
        try {
            parserDefinition(); 
        } catch (ArgParse::argparse_exception& e) {
            ArgParse::detail::printArgParseFatalErrorMsg();
            exit(-1);
        }
    }
    ~ArgParserCmd() {}
    void parserDefinition();
    CmdExecStatus exec(const std::string& option);
    void usage() const { parser.printUsage(); }
    void help() const { parser.printHelp(); }
    void manual() const { parser.printArgumentInfo(); }

    ArgParse::ArgumentParser parser;
};

#endif  // QSYN_ARG_PARSER_CMD_H
