/****************************************************************************
  FileName     [ argparseErrorMsg.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define error outputs for argparser commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_ARG_PARSE_ERROR_MSG_H
#define QSYN_ARG_PARSE_ERROR_MSG_H

#include <exception>
#include <string>

#include "argparseDef.h"

namespace ArgParse {

namespace detail {

void printArgumentCastErrorMsg(Argument const& arg);
void printDefaultValueErrorMsg(Argument const& arg);
void printArgParseFatalErrorMsg();
void printArgNameEmptyErrorMsg();
void printArgNameDuplicateErrorMsg(std::string const& name);
void printDuplicatedAttrErrorMsg(Argument const& arg, std::string const& attrName);

}  // namespace detail

class argparse_exception : public std::exception {
public:
    virtual const char* what() const throw() = 0;
};

class bad_arg_cast : public argparse_exception {
public:
    const char* what() const throw() override {
        return "failed to cast argument to specific type\n";
    }
};

class illegal_parser_arg : public argparse_exception {
public:
    const char* what() const throw() override {
        return "failed to cast argument to specific type\n";
    }
};

}  // namespace ArgParse

#endif  // QSYN_ARG_PARSE_ERROR_MSG_H