/****************************************************************************
  FileName     [ argparseDef.h ]
  PackageName  [ argparser ]
  Synopsis     [ Forward declaration for classes in namespace ArgParse ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_ARG_PARSE_DEF_H
#define QSYN_ARG_PARSE_DEF_H

#include <string>

/**
 * @brief Namespace for argument parser and other auxilliary classes and functions.
 *
 */
namespace ArgParse {

enum class ParseErrorType {
    illegal_arg,
    extra_arg,
    missing_arg_after,
    missing_arg
};

class Argument;
class ArgumentParser;
// class SubParsers;

using TokenPair = std::pair<std::string, bool>;

bool errorOption(ParseErrorType const& errType, std::string const& token);
}  // namespace ArgParse

#endif  // QSYN_ARG_PARSE_DEF_H