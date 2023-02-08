/****************************************************************************
  FileName     [ argparse.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define class ArgumentParser member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparser.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <string>

#include "myTrie.h"
#include "textFormat.h"
#include "util.h"

using namespace std;

extern size_t colorLevel;

namespace ArgParse {

namespace TF = TextFormat;

constexpr auto accentFormat = [](string const& str) { return TF::BOLD(TF::ULINE(str)); };

/**
 * @brief Print the usage of the argument parser. This function
 *        is intentionally not a `const` function since it might
 *        call `analyzeOptions()`.
 *
 */
void ArgumentParser::printUsage() const {
    if (!_optionsAnalyzed) {
        _optionsAnalyzed = analyzeOptions();
    }

    cout << TF::LIGHT_BLUE("Usage: ");
    cout << formattedCmdName();
    for (auto const& [_, arg] : _arguments) {
        if (arg.isRequired())
            cout << " " << arg.getSyntaxString();
    }

    for (auto const& [_, arg] : _arguments) {
        if (arg.isOptional())
            cout << " " << arg.getSyntaxString();
    }

    cout << endl;
}

void ArgumentParser::printSummary() const {
    cout << setw(14 + TF::tokenSize(accentFormat)) << left << formattedCmdName() << " "
         << _cmdDescription << endl;
}

/**
 * @brief Print the usage of the argument parser. This function
 *        is intentionally not a `const` function since it might
 *        call `analyzeOptions()`.
 *
 */
void ArgumentParser::printHelp() const {
    printUsage();
    if (_cmdDescription.size()) {
        cout << TF::LIGHT_BLUE("\nDescription:\n  ") << _cmdDescription << "\n";
    }

    if (count_if(_arguments.begin(), _arguments.end(), [](auto const argPair) { return argPair.second.isPositional(); })) {
        cout << TF::LIGHT_BLUE("\nPositional Arguments:\n");
        for (auto const& [_, arg] : _arguments) {
            if (arg.isPositional()) {
                arg.printHelpString();
            }
        }
    }

    if (count_if(_arguments.begin(), _arguments.end(), [](auto const argPair) { return argPair.second.isNonPositional(); })) {
        cout << TF::LIGHT_BLUE("\nOptions:\n");
        for (auto const& [_, arg] : _arguments) {
            if (arg.isNonPositional()) {
                arg.printHelpString();
            }
        }
    }
}

void ArgumentParser::printTokens() const {
    size_t i = 0;
    for (auto& [token, parsed] : _tokenPairs) {
        cout << "Token #" << ++i << ":\t" << left << setw(12) << token << " (" << (parsed ? "parsed" : "unparsed") << ")" << endl;
    }
}

void ArgumentParser::printArguments() const {
    cout << "Positional arguments:\n";
    for (auto const& [name, arg] : _arguments) {
        if (arg.isPositional()) arg.printStatus();
    }

    cout << "Options:\n";

    for (auto const& [name, arg] : _arguments) {
        if (arg.isNonPositional()) arg.printStatus();
    }
}

void ArgumentParser::cmdInfo(std::string const& cmdName, std::string const& description) {
    _cmdName = toLowerString(cmdName);
    _cmdDescription = description;
    _cmdNumMandatoryChars = countUpperChars(cmdName);
}

Argument& ArgumentParser::operator[](std::string const& name) {
    try {
        return _arguments.at(toLowerString(name));
    } catch (std::out_of_range& e) {
        std::cerr << "name = " << name << ", " << e.what() << std::endl;
        exit(-1);
    }
}

Argument const& ArgumentParser::operator[](std::string const& name) const {
    try {
        return _arguments.at(toLowerString(name));
    } catch (std::out_of_range& e) {
        std::cerr << "name = " << name << ", " << e.what() << std::endl;
        exit(-1);
    }
}

bool ArgumentParser::parse(string const& line) {
    if (!_optionsAnalyzed) {
        _optionsAnalyzed = analyzeOptions();
    }

    for (auto& [_, arg] : _arguments) {
        arg.reset();
    }

    return tokenize(line) && parseOptions() && parsePositionalArguments();
}

/**
 * @brief Analyze the argument to produce syntactic information for the parser.
 *
 * @return true
 * @return false
 */
bool ArgumentParser::analyzeOptions() const {
    MyTrie trie;
    for (auto const& [name, arg] : _arguments) {
        if (arg.isPositional()) continue;
        trie.insert(name);
    }

    for (auto& [name, arg] : _arguments) {
        if (arg.isPositional()) continue;
        arg.setNumMandatoryChars(
            max(
                trie.shortestUniquePrefix(name).value().size(),
                arg.getNumMandatoryChars()));
    }
    return true;
}

std::string ArgumentParser::formattedCmdName() const {
    if (colorLevel >= 1) {
        string mand = _cmdName.substr(0, _cmdNumMandatoryChars);
        string rest = _cmdName.substr(_cmdNumMandatoryChars);
        return accentFormat(mand) + rest;
    } else {
        string tmp = _cmdName;
        for (size_t i = 0; i < tmp.size(); ++i) {
            tmp[i] = (i < _cmdNumMandatoryChars) ? ::toupper(tmp[i]) : ::tolower(tmp[i]);
        }
        return tmp;
    }
}

bool ArgumentParser::tokenize(string const& line) {
    _tokenPairs.clear();
    string buffer, stripped;
    if (!stripQuotes(line, stripped)) {
        cerr << "Error: missing ending quote!!" << endl;
        return false;
    }
    size_t pos = myStrGetTok(stripped, buffer);
    while (buffer.size()) {
        _tokenPairs.emplace_back(buffer, false);
        pos = myStrGetTok(stripped, buffer, pos);
    }
    if (_tokenPairs.empty()) return true;
    // concat tokens with '\ ' to a single token with space in it
    for (auto itr = next(_tokenPairs.rbegin()); itr != _tokenPairs.rend(); ++itr) {
        string& currToken = itr->first;
        string& nextToken = prev(itr)->first;

        if (currToken.ends_with('\\')) {
            currToken.back() = ' ';
            currToken += nextToken;
            nextToken = "";
        }
    }
    std::erase_if(_tokenPairs, [](TokenPair const& tokenPair) { return tokenPair.first == ""; });

    return true;
}

bool ArgumentParser::parseOptions() {
    size_t i = _tokenPairs.size() - 1;
    size_t lastUnparsed = _tokenPairs.size();

    for (auto& [token, parsed] : _tokenPairs | views::reverse) {
        for (auto& [name, arg] : _arguments) {
            if (arg.isPositional()) continue;
            if (myStrNCmp(arg.getName(), token, arg.getNumMandatoryChars()) != 0) continue;

            auto tokenSpan = span<TokenPair>{_tokenPairs}.subspan(i + 1, lastUnparsed - (i + 1));
            if (tokenSpan.empty() && !arg.hasAction()) {
                return errorOption(ParseErrorType::missing_arg_after, _tokenPairs[i].first);
            }

            if (!arg.parse(tokenSpan)) {
                return false;
            }
            parsed = true;
            lastUnparsed = i;
            break;
        }
        --i;
    }

    auto reqOptionIsParsed = [](auto const& argPair) {
        Argument const& arg = argPair.second;
        return implies(arg.isNonPositional() && arg.isRequired(), arg.isParsed());
    };

    auto itr = find_if_not(_arguments.begin(), _arguments.end(), reqOptionIsParsed);
    if (itr != _arguments.end()) {
        return errorOption(ParseErrorType::missing_arg, itr->first);
    }
    

    return true;
}

bool ArgumentParser::parsePositionalArguments() {
    auto currTokenPair = _tokenPairs.begin();
    auto currArg = _arguments.begin();
    size_t i = 0;
    string lastParsedToken;

    auto nextTokenPair = [&currTokenPair, &i, this]() {
        while (currTokenPair != _tokenPairs.end() && currTokenPair->second == true) {
            currTokenPair++;
            i++;
        }
    };

    auto nextArgument = [&currArg, this]() {
        while (
            currArg != _arguments.end() && (currArg->second.isNonPositional() ||
                                            currArg->second.isParsed()))
            currArg++;
    };

    auto tokenIsParsed = [](auto const& tokenPair) {
        return tokenPair.second;
    };

    auto requiredArgIsParsed = [](auto const& argPair) {
        Argument const& arg= argPair.second;
        return implies(arg.isRequired(), arg.isParsed());
    };

    nextTokenPair();
    nextArgument();

    for (; currTokenPair != _tokenPairs.end() && currArg != _arguments.end(); nextTokenPair(), nextArgument()) {
        auto& [token, parsed] = *currTokenPair;
        auto& [name, arg] = *currArg;

        assert(arg.isPositional());
        auto tokenSpan = span<TokenPair>{_tokenPairs}.subspan(i);

        if (!arg.parse(tokenSpan)) {
            return false;
        }
        lastParsedToken = token;
        parsed = true;
    }

    if (!all_of(_arguments.begin(), _arguments.end(), requiredArgIsParsed)) {
        return errorOption(ParseErrorType::missing_arg_after, lastParsedToken);
    }

    if (!all_of(_tokenPairs.begin(), _tokenPairs.end(), tokenIsParsed)) {
        return errorOption(ParseErrorType::extra_arg, currTokenPair->first);
    }

    return true;
}

//----------------------------------
// Argument type: subparser
//----------------------------------

// ArgumentParser& SubParsers::addParser(string const& name, string const& help) {
//     _subparsers.emplace(name, ArgumentParser{});
//     _subparsers.at(name).cmdInfo(name, help);
//     return _subparsers.at(name);
// }

}  // namespace ArgParse