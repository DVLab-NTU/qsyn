/****************************************************************************
  FileName     [ argument.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument interface for ArgumentParser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iomanip>
#include <iostream>

#include "./argparse.h"

using namespace std;

namespace ArgParse {

/**
 * @brief If the argument has a default value, reset to it.
 *
 */
void Argument::reset() {
    _parsed = false;
    _pimpl->do_reset();
}

/**
 * @brief get tokens from `tokens` and takes actions accordingly.
 *        if tokens are not empty, mark the
 *
 * @param tokens
 * @return true if action success, or
 * @return false if action failed or < l argument are available
 */
bool Argument::takeAction(TokensView tokens) {
    if (!_pimpl->do_takeAction(tokens) || !constraintsSatisfied()) return false;

    return true;
}

/**
 * @brief Get a range of at most nargs.upper consecutive unparsed tokens.
 *
 * @param tokens
 * @return TokensView
 */
TokensView Argument::getParseRange(TokensView tokens) const {
    size_t parse_start = std::find_if(
                             tokens.begin(), tokens.end(),
                             [](Token& token) { return token.parsed == false; }) -
                         tokens.begin();

    size_t parse_end = std::find_if(
                           tokens.begin() + parse_start, tokens.end(),
                           [](Token& token) { return token.parsed == true; }) -
                       tokens.begin();

    return tokens.subspan(parse_start, std::min(getNArgs().upper, parse_end - parse_start));
}

bool Argument::tokensEnoughToParse(TokensView tokens) const {
    auto [lower, upper] = getNArgs();
    if (tokens.size() < lower) {
        cerr << "Error: missing argument \""
             << this->getName() << "\": expected ";

        if (lower < upper) {
            cerr << "at least ";
        }
        cerr << lower << " arguments!!\n";
        return false;
    }
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

}  // namespace ArgParse