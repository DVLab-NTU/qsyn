/****************************************************************************
  FileName     [ argparse.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define class ArgumentParser member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparser.h"

#include <iomanip>
#include <iostream>
#include <ranges>
#include <string>

#include "util.h"
#include "myTrie.h"
#include "textFormat.h"

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

void ArgumentParser::printHelp() const {
    cout << setw(14 + TF::tokenSize(accentFormat)) << left << formattedCmdName() << " "
         << _cmdDescription << endl;
}

/**
 * @brief Print the usage of the argument parser. This function
 *        is intentionally not a `const` function since it might
 *        call `analyzeOptions()`.
 *
 */
void ArgumentParser::printArgumentInfo() const {
    printUsage();

    cout << TF::LIGHT_BLUE("\nDescription:\n  ") << _cmdDescription << "\n";

    cout << TF::LIGHT_BLUE("\nMandatory arguments:\n");
    for (auto const& [_, arg] : _arguments) {
        if (arg.isMandatory()) {
            arg.printInfoString();
        }
    }

    cout << TF::LIGHT_BLUE("\nOptional arguments:\n");

    for (auto const& [_, arg] : _arguments) {
        if (arg.isOptional()) {
            arg.printInfoString();
        }
    }
}

void ArgumentParser::printTokens() const {
    size_t i = 0;
    for (auto token : _tokens) {
        cout << "Token #" << ++i << ":\t" << token << endl;
    }
}

void ArgumentParser::cmdInfo(std::string const& cmdName, std::string const& description) {
    _cmdName = toLowerString(cmdName);
    _cmdDescription = description;
    _cmdNumMandatoryChars = countUpperChars(cmdName);
}

Argument& ArgumentParser::operator[](std::string const& key) {
    try {
        return _arguments[key];
    } catch (std::out_of_range& e) {
        std::cerr << "key = " << key << ", " << e.what() << std::endl;
        exit(-1);
    }
}

Argument const& ArgumentParser::operator[](std::string const& key) const {
    try {
        return _arguments.at(key);
    } catch (std::out_of_range& e) {
        std::cerr << "key = " << key << ", " << e.what() << std::endl;
        exit(-1);
    }
}

bool ArgumentParser::parse(std::string const& line) {
    if (!_optionsAnalyzed) {
        _optionsAnalyzed = analyzeOptions();
    }

    tokenize(line);
    return true;
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

std::string ArgumentParser::toLowerString(std::string const& str) const {
    std::string ret = str;
    for_each(ret.begin(), ret.end(), [](char& ch) { ch = ::tolower(ch); });
    return ret;
};

size_t ArgumentParser::countUpperChars(std::string const& str) const {
    size_t cnt = 0;
    for (auto& ch : str) {
        if (::islower(ch)) return cnt;
        ++cnt;
    }
    return str.size();
};

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
    _tokens.clear();
    string buffer, stripped;
    if (!stripQuotes(line, stripped)) {
        cerr << "Error: missing ending quote!!" << endl;
        return false;
    }
    size_t pos = myStrGetTok(stripped, buffer);
    while (buffer.size()) {
        _tokens.push_back(buffer);
        pos = myStrGetTok(stripped, buffer, pos);
    }
    if (_tokens.empty()) return true;
    // concat tokens with '\ ' to a single token with space in it
    for (auto itr = next(_tokens.rbegin()); itr != _tokens.rend(); ++itr) {
        string& currToken = *itr;
        string& nextToken = *prev(itr);

        if (currToken.ends_with('\\')) {
            currToken.back() = ' ';
            currToken += nextToken;
            nextToken = "";
        }
    }
    std::erase(_tokens, "");
    
    return true;
}

}  // namespace ArgParse