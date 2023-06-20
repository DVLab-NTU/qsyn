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

#include "argparse.h"
#include "textFormat.h"

using namespace std;

extern size_t colorLevel;

namespace TF = TextFormat;

namespace ArgParse {

constexpr auto requiredStyle = TF::CYAN;
constexpr auto metavarStyle = TF::BOLD;
constexpr auto optionalStyle = TF::YELLOW;
constexpr auto typeStyle = [](string const& str) -> string { return TF::CYAN(TF::ITALIC(str)); };
constexpr auto accentStyle = [](string const& str) -> string { return TF::BOLD(TF::ULINE(str)); };

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
string ArgumentParser::styledCmdName(std::string const& name, size_t numRequired) const {
    if (colorLevel >= 1) {
        string mand = name.substr(0, numRequired);
        string rest = name.substr(numRequired);
        return accentStyle(mand) + rest;
    } else {
        string tmp = name;
        for (size_t i = 0; i < tmp.size(); ++i) {
            tmp[i] = (i < numRequired) ? ::toupper(tmp[i]) : ::tolower(tmp[i]);
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
    for (auto& [token, parsed] : _pimpl->tokens) {
        cout << "Token #" << ++i << ":\t"
             << left << setw(8) << token << " (" << (parsed ? "parsed" : "unparsed") << ")  "
             << "Frequency: " << right << setw(3) << _pimpl->trie.frequency(token) << endl;
    }
}

/**
 * @brief Print the argument and their parse states
 *
 */
void ArgumentParser::printArguments() const {
    for (auto& [_, arg] : _pimpl->arguments) {
        if (arg.isRequired()) arg.printStatus();
    }
    for (auto& [_, arg] : _pimpl->arguments) {
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
    cout << styledCmdName(getName(), getNumRequiredChars());
    for (auto const& [name, arg] : _pimpl->arguments) {
        if (!arg.isRequired() && !_pimpl->conflictGroups.contains(name)) {
            cout << " " << optionalArgBracket(getSyntaxString(arg));
        }
    }

    for (auto const& group : _pimpl->mutuallyExclusiveGroups) {
        if (!group.isRequired()) {
            cout << " " + optionalStyle("[");
            size_t ctr = 0;
            for (auto const& name : group.getArguments()) {
                cout << getSyntaxString(_pimpl->arguments.at(name));
                if (++ctr < group.size()) cout << optionalStyle(" | ");
            }
            cout << optionalStyle("]");
        }
    }

    for (auto const& group : _pimpl->mutuallyExclusiveGroups) {
        if (group.isRequired()) {
            cout << " " + requiredStyle("<");
            size_t ctr = 0;
            for (auto const& name : group.getArguments()) {
                cout << getSyntaxString(_pimpl->arguments.at(name));
                if (++ctr < group.size()) cout << requiredStyle(" | ");
            }
            cout << requiredStyle(">");
        }
    }

    for (auto const& [name, arg] : _pimpl->arguments) {
        if (arg.isRequired() && !_pimpl->conflictGroups.contains(name)) {
            cout << " " << getSyntaxString(arg);
        }
    }

    if (_pimpl->subparsers.has_value()) {
        cout << " " << (_pimpl->subparsers->isRequired() ? requiredStyle("<") : optionalStyle("["))
             << getSyntaxString(_pimpl->subparsers.value())
             << (_pimpl->subparsers->isRequired() ? requiredStyle(">") : optionalStyle("]")) << " ...";
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
    cout << setw(15 + TF::tokenSize(accentStyle)) << left << styledCmdName(getName(), getNumRequiredChars()) + ":  "
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

    if (count_if(_pimpl->arguments.begin(), _pimpl->arguments.end(), argPairIsRequired)) {
        cout << TF::LIGHT_BLUE("\nRequired Arguments:\n");
        for (auto const& [_, arg] : _pimpl->arguments) {
            if (arg.isRequired()) {
                printHelpString(arg);
            }
        }
    }

    if (_pimpl->subparsers.has_value() && _pimpl->subparsers->isRequired()) {
        printHelpString(_pimpl->subparsers.value());
    }

    if (count_if(_pimpl->arguments.begin(), _pimpl->arguments.end(), argPairIsOptional)) {
        cout << TF::LIGHT_BLUE("\nOptional Arguments:\n");
        for (auto const& [_, arg] : _pimpl->arguments) {
            if (!arg.isRequired()) {
                printHelpString(arg);
            }
        }
    }

    if (_pimpl->subparsers.has_value() && !_pimpl->subparsers->isRequired()) {
        printHelpString(_pimpl->subparsers.value());
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

string ArgumentParser::getSyntaxString(SubParsers parsers) const {
    string ret = "{";
    size_t ctr = 0;
    for (auto const& [name, parser] : _pimpl->subparsers->getSubParsers()) {
        ret += styledCmdName(parser.getName(), parser.getNumRequiredChars());
        if (++ctr < _pimpl->subparsers->size()) ret += ", ";
    }
    ret += "}";

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
    _pimpl->tabl << (arg.hasAction() ? typeStyle("flag") : typeStyle(arg.getTypeString()));

    if (hasOptionPrefix(arg)) {
        if (arg.hasAction()) {
            _pimpl->tabl << Tabler::Multicols(styledArgName(arg), 2);
        } else {
            _pimpl->tabl << styledArgName(arg) << metavarStyle(arg.getMetavar());
        }
    } else {
        _pimpl->tabl << Tabler::Multicols(metavarStyle(arg.getMetavar()), 2);
    }

    _pimpl->tabl << arg.getHelp();
}

void ArgumentParser::printHelpString(SubParsers parsers) const {
    using qsutil::Tabler;
    _pimpl->tabl << Tabler::Multicols(getSyntaxString(parsers), 3) << parsers.getHelp();
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