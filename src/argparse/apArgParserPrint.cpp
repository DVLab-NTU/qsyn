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
constexpr auto metavarStyle = TF::BOLD;
constexpr auto optionalStyle = TF::YELLOW;
constexpr auto typeStyle = [](string const& str) -> string { return TF::CYAN(TF::ITALIC(str)); };
constexpr auto accentStyle = [](string const& str) -> string { return TF::BOLD(TF::ULINE(str)); };

namespace ArgParse {

/**
 * @brief print the error message when duplicated argument name is detected
 *
 * @param name
 */
void ArgumentParser::printDuplicateArgNameErrorMsg(std::string const& name) const {
    std::cerr << "[ArgParse] Error: Duplicate argument name \"" << name << "\"!!" << std::endl;
}

/**
 * @brief return a string of styled command name. The mandatory part is accented.
 *
 * @return string
 */
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
    for (auto const& [name, arg] : _arguments) {
        if (!arg.isRequired() && !_conflictGroups.contains(name)) {
            cout << " " << optionalArgBracket(getSyntaxString(arg));
        }
    }

    for (auto const& group : _mutuallyExclusiveGroups) {
        if (!group.isRequired()) {
            cout << " " + optionalStyle("[");
            size_t ctr = 0;
            for (auto const& name : group.getArguments()) {
                cout << getSyntaxString(_arguments.at(name));
                if (++ctr < group.getArguments().size()) cout << optionalStyle(" | ");
            }
            cout << optionalStyle("]");
        }
    }

    for (auto const& group : _mutuallyExclusiveGroups) {
        if (group.isRequired()) {
            cout << " " + requiredStyle("<");
            size_t ctr = 0;
            for (auto const& name : group.getArguments()) {
                cout << getSyntaxString(_arguments.at(name));
                if (++ctr < group.getArguments().size()) cout << requiredStyle(" | ");
            }
            cout << requiredStyle(">");
        }
    }

    for (auto const& [name, arg] : _arguments) {
        if (arg.isRequired() && !_conflictGroups.contains(name)) {
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
    cout << setw(15 + TF::tokenSize(accentStyle)) << left << styledCmdName() + ":  "
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

    auto argPairIsRequired = [](pair<string, Argument> const& argPair) {
        return argPair.second.isRequired();
    };

    auto argPairIsOptional = [](pair<string, Argument> const& argPair) {
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

/**
 * @brief get the syntax representation string of an argument.
 *
 * @param arg
 * @return string
 */
string ArgumentParser::getSyntaxString(Argument const& arg) const {
    string ret = "";

    if (!arg.hasAction()) {
        ret += requiredArgBracket(
            typeStyle(arg.getTypeString()) + " " + metavarStyle(arg.getMetavar()));
    }
    if (hasOptionPrefix(arg)) {
        ret = optionalStyle(styledArgName(arg)) + (arg.hasAction() ? "" : (" " + ret));
    }

    return ret;
}

// printing helpers

/**
 * @brief circumfix the string with the required argument bracket (<...>)
 *
 * @param str
 * @return string
 */
string ArgumentParser::requiredArgBracket(std::string const& str) const {
    return requiredStyle("<") + str + requiredStyle(">");
}

/**
 * @brief circumfix the string with the optional argument bracket ([]...])
 *
 * @param str
 * @return string
 */
string ArgumentParser::optionalArgBracket(std::string const& str) const {
    return optionalStyle("[") + str + optionalStyle("]");
}

/**
 * @brief print the help string of an argument
 *
 * @param arg
 */
void ArgumentParser::printHelpString(Argument const& arg) const {
    using qsutil::Tabler;
    _tabl << (arg.hasAction() ? typeStyle("flag") : typeStyle(arg.getTypeString()));

    if (hasOptionPrefix(arg)) {
        _tabl << styledArgName(arg);
        if (arg.hasAction())
            _tabl << Tabler::Skip{};
        else
            _tabl << metavarStyle(arg.getMetavar());
    } else {
        _tabl << metavarStyle(arg.getMetavar()) << Tabler::Skip{};
    }

    _tabl << arg.getHelp();
}

/**
 * @brief print the styled argument name.
 *
 * @param arg
 * @return string
 */
string ArgumentParser::styledArgName(Argument const& arg) const {
    if (!hasOptionPrefix(arg)) return metavarStyle(arg.getName());
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