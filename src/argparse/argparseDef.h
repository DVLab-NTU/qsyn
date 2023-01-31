/****************************************************************************
  FileName     [ argparseDef.h ]
  PackageName  [ argparser ]
  Synopsis     [ Forward declaration for classes in namespace ArgParse ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_ARG_PARSE_DEF_H
#define QSYN_ARG_PARSE_DEF_H

/**
 * @brief Namespace for argument parser and other auxilliary classes and functions.
 * 
 */
namespace ArgParse {
    enum class ParseResult;
    class Argument;
    class ArgumentParser;
    class SubParsers;

    using Token = std::pair<std::string, bool>;
}

#endif // QSYN_ARG_PARSE_DEF_H