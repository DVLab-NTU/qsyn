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
        if (arg.isMandatory())
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

    if (count_if(_arguments.begin(), _arguments.end(), [](auto const argPair) { return argPair.second.isMandatory(); })) {
        cout << TF::LIGHT_BLUE("\nMandatory arguments:\n");
        for (auto const& [_, arg] : _arguments) {
            if (arg.isMandatory()) {
                arg.printHelpString();
            }
        }
    }

    if (count_if(_arguments.begin(), _arguments.end(), [](auto const argPair) { return argPair.second.isOptional(); })) {
        cout << TF::LIGHT_BLUE("\nOptional arguments:\n");
        for (auto const& [_, arg] : _arguments) {
            if (arg.isOptional()) {
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
    cout << "Mandatory arguments:\n";
    for (auto const& [name, arg] : _arguments) {
        if (arg.isMandatory()) arg.printStatus();
    }

    cout << "Optional arguments:\n";

    for (auto const& [name, arg] : _arguments) {
        if (arg.isOptional()) arg.printStatus();
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

ParseResult ArgumentParser::parse(string const& line) {
    if (!_optionsAnalyzed) {
        _optionsAnalyzed = analyzeOptions();
    }

    for (auto& [_, arg] : _arguments) {
        arg.reset();
    }

    tokenize(line);

    auto result = parseOptionalArguments();
    if (result != ParseResult::success) {
        return result;
    }

    result = parseMandatoryArguments();
    if (result != ParseResult::success) {
        return result;
    }

    return ParseResult::success;
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
        if (arg.isMandatory()) continue;
        trie.insert(name);
    }

    for (auto& [name, arg] : _arguments) {
        if (arg.isMandatory()) continue;
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

ParseResult ArgumentParser::parseOptionalArguments() {
    size_t i = _tokenPairs.size() - 1;
    size_t lastUnparsed = _tokenPairs.size();

    for (auto& [token, parsed] : _tokenPairs | views::reverse) {
        for (auto& [name, arg] : _arguments) {
            if (arg.isMandatory()) continue;
            if (myStrNCmp(arg.getName(), token, arg.getNumMandatoryChars()) != 0) continue;

            auto tokenSpan = span<TokenPair>{_tokenPairs}.subspan(i + 1, lastUnparsed - (i + 1));
            if (tokenSpan.empty() && !arg.hasAction()) {
                return errorOption(ParseErrorType::missing_arg, _tokenPairs[i].first);
            }

            auto result = arg.parse(tokenSpan);

            if (result != ParseResult::success) {
                return result;
            }
            parsed = true;
            lastUnparsed = i;
            break;
        }
        --i;
    }

    return ParseResult::success;
}

ParseResult ArgumentParser::parseMandatoryArguments() {
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
            currArg != _arguments.end() && (currArg->second.isOptional() ||
                                            currArg->second.isParsed()))
            currArg++;
    };

    auto tokenIsParsed = [](auto const& tokenPair) {
        return tokenPair.second;
    };

    auto mandatoryArgIsParsed = [](auto const& argPair) {
        return argPair.second.isOptional() || argPair.second.isParsed();
    };

    nextTokenPair();
    nextArgument();

    for (; currTokenPair != _tokenPairs.end() && currArg != _arguments.end(); nextTokenPair(), nextArgument()) {
        auto& [token, parsed] = *currTokenPair;
        auto& [name, arg] = *currArg;

        assert(arg.isMandatory());

        auto result = arg.parse(span<TokenPair>{_tokenPairs}.subspan(i));

        if (result != ParseResult::success) {
            return errorOption(ParseErrorType::illegal_arg, _tokenPairs[i].first);
        }
        lastParsedToken = token;
        parsed = true;
    }

    if (!all_of(_arguments.begin(), _arguments.end(), mandatoryArgIsParsed)) {
        return errorOption(ParseErrorType::missing_arg, lastParsedToken);
    }

    if (!all_of(_tokenPairs.begin(), _tokenPairs.end(), tokenIsParsed)) {
        return errorOption(ParseErrorType::extra_arg, currTokenPair->first);
    }

    return ParseResult::success;
}

//----------------------------------
// Argument type: subparser
//----------------------------------

ArgumentParser& SubParsers::addParser(string const& name, string const& help) {
    _subparsers.emplace(name, ArgumentParser{});
    _subparsers.at(name).cmdInfo(name, help);
    return _subparsers.at(name);
}

}  // namespace ArgParse