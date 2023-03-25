/****************************************************************************
  FileName     [ argument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument interface for ArgumentParser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "apArgument.h"

#include <iomanip>
#include <iostream>

using namespace std;

namespace ArgParse {

/**
 * @brief If the argument has a default value, reset to it.
 *
 */
void Argument::reset() {
    _parsed = false;
    _pimpl->doReset();
}

/**
 * @brief parse the argument. If the argument has an action, perform it; otherwise,
 *        try to parse the value from token.
 *
 * @param token
 * @return true if succeeded
 * @return false if failed
 */
bool Argument::parse(std::string const& token) {
    if (!_pimpl->doParse(token)) return false;
    _parsed = true;
    return true;
}

/**
 * @brief If the argument is parsed, print out the parsed value. If not,
 *        print the default value if it has one, or "(unparsed)" if not.
 *
 */
void Argument::printStatus() const {
    cout << "  " << left << setw(8) << getName() << "   = ";
    if (isParsed()) {
        cout << *this;
    } else if (hasDefaultValue()) {
        cout << *this << " (default)";
    } else {
        cout << "(unparsed)";
    }
    cout << endl;
}

// error printing function

/**
 * @brief print argument casting error message
 *
 */
void Argument::printArgCastErrorMsg() const {
    std::cout << "[ArgParse] Error: cannot cast argument \""
              << getName() << "\" to target type!!" << std::endl;
}

}  // namespace ArgParse