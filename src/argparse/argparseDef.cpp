/****************************************************************************
  FileName     [ argparseDef.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Forward declaration for classes in namespace ArgParse ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparseDef.h"

#include <cassert>
#include <iostream>

/**
 * @brief Namespace for argument parser and other auxilliary classes and functions.
 *
 */
namespace ArgParse {

bool errorOption(ParseErrorType const& errType, std::string const& token) {
    using std::cerr, std::endl;

    switch (errType) {
        case ParseErrorType::illegal_arg:
            cerr << "Error: illegal argument"
                 << (token.size() ? " \"" + token + "\"" : "")
                 << "!!" << endl;
            break;
        case ParseErrorType::extra_arg:
            cerr << "Error: extra argument"
                 << (token.size() ? " \"" + token + "\"" : "")
                 << "!!" << endl;
            break;
        case ParseErrorType::missing_arg:
            cerr << "Error: missing argument"
                 << (token.size() ? " \"" + token + "\"" : "")
                 << "!!" << endl;
            break;
        case ParseErrorType::missing_arg_after:
            cerr << "Error: missing argument"
                 << (token.size() ? " after \"" + token + "\"" : "")
                 << "!!" << endl;
            break;
        default:
            break;
    }

    return false;
}

}  // namespace ArgParse