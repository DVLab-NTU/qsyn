/****************************************************************************
  FileName     [ apArgParser.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser print functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iomanip>
#include <iostream>
#include <numeric>

#include "apArgParser.h"
#include "textFormat.h"

using namespace std;

extern size_t colorLevel;

namespace TF = TextFormat;

constexpr auto requiredStyle = TF::CYAN;
constexpr auto metaVarStyle = TF::BOLD;
constexpr auto optionalStyle = TF::YELLOW;
constexpr auto typeStyle = [](string const& str) -> string { return TF::CYAN(TF::ITALIC(str)); };
constexpr auto accentStyle = [](string const& str) -> string { return TF::BOLD(TF::ULINE(str)); };

namespace ArgParse {

void ArgumentParser::printDuplicateArgNameErrorMsg(std::string const& name) const {
    std::cerr << "[ArgParse] Error: Duplicate argument name \"" << name << "\"!!" << std::endl;
}

string ArgumentParser::styledCmdName() const {
    if (colorLevel >= 1) {
        string mand = _name.substr(0, _numRequiredChars);
        string rest = _name.substr(_numRequiredChars);
        return accentStyle(mand) + rest;
    } else {
        string tmp = _name;
        for (size_t i = 0; i < tmp.size(); ++i) {
            tmp[i] = (i < _numRequiredChars) ? ::toupper(tmp[i]) : ::tolower(tmp[i]);
        }
        return tmp;
    }
}

/**
 * @brief Print the tokens and their parse states
 *
 */
void ArgumentParser::printTokens() const {
    size_t i = 0;
    for (auto& [token, parsed] : _tokens) {
        cout << "Token #" << ++i << ":\t"
             << left << setw(8) << token << " (" << (parsed ? "parsed" : "unparsed") << ")  "
             << "Frequency: " << right << setw(3) << _trie.frequency(token) << endl;
    }
}

/**
 * @brief Print the argument and their parse states
 *
 */
void ArgumentParser::printArguments() const {
    for (auto& [_, arg] : _arguments) {
        if (arg.isRequired()) arg.printStatus();
    }
    for (auto& [_, arg] : _arguments) {
        if (!arg.isRequired()) arg.printStatus();
    }
}

/**
 * @brief Print the usage of the command
 *
 */
void ArgumentParser::printUsage() const {
    if (!analyzeOptions()) {
        cerr << "[ArgParse] Failed to generate usage information!!" << endl;
        return;
    }

    cout << TF::LIGHT_BLUE("Usage: ");
    cout << styledCmdName();
    for (auto const& [_, arg] : _arguments) {
        if (arg.isRequired()) {
            cout << " " << getSyntaxString(arg);
        }
    }

    for (auto const& [_, arg] : _arguments) {
        if (!arg.isRequired()) {
            cout << " " << getSyntaxString(arg);
        }
    }

    cout << endl;
}

/**
 * @brief Print the argument name and help message
 *
 */
void ArgumentParser::printSummary() const {
    if (!analyzeOptions()) {
        cerr << "[ArgParse] Failed to generate usage information!!" << endl;
        return;
    }
    cout << setw(13) << left << styledCmdName() << "  "
         << getHelp() << endl;
}

/**
 * @brief Print the help information for a command
 *
 */
void ArgumentParser::printHelp() const {
    printUsage();
    if (getHelp().size()) {
        cout << TF::LIGHT_BLUE("\nDescription:\n  ") << getHelp() << endl;
    }

    auto argPairIsRequired = [this](pair<string, Argument> const& argPair) {
        return argPair.second.isRequired();
    };

    auto argPairIsOptional = [this](pair<string, Argument> const& argPair) {
        return !argPair.second.isRequired();
    };

    if (count_if(_arguments.begin(), _arguments.end(), argPairIsRequired)) {
        cout << TF::LIGHT_BLUE("\nRequired Arguments:\n");
        for (auto const& [_, arg] : _arguments) {
            if (arg.isRequired()) {
                printHelpString(arg);
            }
        }
    }

    if (count_if(_arguments.begin(), _arguments.end(), argPairIsOptional)) {
        cout << TF::LIGHT_BLUE("\nOptional Arguments:\n");
        for (auto const& [_, arg] : _arguments) {
            if (!arg.isRequired()) {
                printHelpString(arg);
            }
        }
    }
}

string ArgumentParser::getSyntaxString(Argument const& arg) const {
    string ret = "";

    if (!arg.hasAction()) {
        // TODO - metavar
        ret += requiredArgBracket(
            typeStyle(arg.getTypeString()) + " " + metaVarStyle(arg.getMetaVar()));
    }
    if (hasOptionPrefix(arg)) {
        ret = optionalStyle(styledArgName(arg)) + (arg.hasAction() ? "" : (" " + ret));
    }

    return (arg.isRequired()) ? ret : optionalArgBracket(ret);
}

// printing helpers

string ArgumentParser::requiredArgBracket(std::string const& str) const {
    return requiredStyle("<") + str + requiredStyle(">");
}

string ArgumentParser::optionalArgBracket(std::string const& str) const {
    return optionalStyle("[") + str + optionalStyle("]");
}

void ArgumentParser::printHelpString(Argument const& arg) const {
    // argument types and names

    size_t lineBreakThres = accumulate(_printTableWidths.begin(), _printTableWidths.end(), 0);

    cout << "  ";
    if (!arg.hasAction()) {
        cout << left << setw(_printTableWidths[0] + TF::tokenSize(typeStyle)) << typeStyle(arg.getTypeString());
    } else {
        cout << typeStyle("flag") << string(_printTableWidths[0] - 4, ' ');
    }
    cout << "  ";
    if (hasOptionPrefix(arg)) {
        cout << left << setw(_printTableWidths[1] + TF::tokenSize(accentStyle) + (colorLevel >= 1) * 2 * TF::tokenSize(optionalStyle))
             << styledArgName(arg);
        cout << "  ";
        cout << left << setw(_printTableWidths[2] + TF::tokenSize(metaVarStyle))
             << metaVarStyle(arg.getMetaVar());

        cout << "  ";
        if (arg.getName().size() + arg.getMetaVar().size() + arg.getTypeString().size() > lineBreakThres) {
            cout << "\n"
                 << string(lineBreakThres + 8, ' ');
        }
    } else {
        cout << left << setw(_printTableWidths[1] + _printTableWidths[2] + TF::tokenSize(metaVarStyle) + 2)
             << metaVarStyle(arg.getMetaVar());
        cout << "  ";
        if (arg.getMetaVar().size() + arg.getTypeString().size() > lineBreakThres + 2) {
            cout << "\n"
                 << string(lineBreakThres + 8, ' ');
        }
    }

    cout << arg.getHelp() << endl;
}

string ArgumentParser::styledArgName(Argument const& arg) const {
    if (!hasOptionPrefix(arg)) return metaVarStyle(arg.getName());
    if (colorLevel >= 1) {
        string mand = arg.getName().substr(0, arg.getNumRequiredChars());
        string rest = arg.getName().substr(arg.getNumRequiredChars());
        return optionalStyle(accentStyle(mand)) + optionalStyle(rest);
    } else {
        string tmp = arg.getName();
        for (size_t i = 0; i < tmp.size(); ++i) {
            tmp[i] = (i < arg.getNumRequiredChars()) ? ::toupper(tmp[i]) : ::tolower(tmp[i]);
        }
        return tmp;
    }
}

}  // namespace ArgParse