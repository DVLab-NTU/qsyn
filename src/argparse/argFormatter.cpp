/****************************************************************************
  FileName     [ argFormatter.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser formatter functions ]
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
 * @brief return a string of styled command name. The mandatory part is accented.
 *
 * @return string
 */
string Formatter::styledCmdName(std::string const& name, size_t numRequired) {
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
 * @brief Print the usage of the command
 *
 */
void Formatter::printUsage(ArgumentParser parser) {
    if (!parser.analyzeOptions()) {
        cerr << "[ArgParse] Failed to generate usage information!!" << endl;
        return;
    }

    auto& arguments = parser._pimpl->arguments;
    auto& subparsers = parser._pimpl->subparsers;
    auto& mutexGroups = parser._pimpl->mutuallyExclusiveGroups;
    auto& conflictGroups = parser._pimpl->conflictGroups;

    cout << TF::LIGHT_BLUE("Usage: ");
    cout << styledCmdName(parser.getName(), parser.getNumRequiredChars());
    for (auto const& [name, arg] : arguments) {
        if (!arg.isRequired() && !conflictGroups.contains(name)) {
            cout << " " << optionalArgBracket(getSyntaxString(parser, arg));
        }
    }

    for (auto const& group : mutexGroups) {
        if (!group.isRequired()) {
            cout << " " + optionalStyle("[");
            size_t ctr = 0;
            for (auto const& name : group.getArguments()) {
                cout << getSyntaxString(parser, arguments.at(name));
                if (++ctr < group.size()) cout << optionalStyle(" | ");
            }
            cout << optionalStyle("]");
        }
    }

    for (auto const& group : mutexGroups) {
        if (group.isRequired()) {
            cout << " " + requiredStyle("<");
            size_t ctr = 0;
            for (auto const& name : group.getArguments()) {
                cout << getSyntaxString(parser, arguments.at(name));
                if (++ctr < group.size()) cout << requiredStyle(" | ");
            }
            cout << requiredStyle(">");
        }
    }

    for (auto const& [name, arg] : arguments) {
        if (arg.isRequired() && !conflictGroups.contains(name)) {
            cout << " " << getSyntaxString(parser, arg);
        }
    }

    if (subparsers.has_value()) {
        cout << " " << (subparsers->isRequired() ? requiredStyle("<") : optionalStyle("["))
             << getSyntaxString(subparsers.value())
             << (subparsers->isRequired() ? requiredStyle(">") : optionalStyle("]")) << " ...";
    }

    cout << endl;
}

/**
 * @brief Print the argument name and help message
 *
 */
void Formatter::printSummary(ArgumentParser parser) {
    if (!parser.analyzeOptions()) {
        cerr << "[ArgParse] Failed to generate usage information!!" << endl;
        return;
    }
    cout << setw(15 + TF::tokenSize(accentStyle)) << left << styledCmdName(parser.getName(), parser.getNumRequiredChars()) + ":  "
         << parser.getHelp() << endl;
}

/**
 * @brief Print the help information for a command
 *
 */
void Formatter::printHelp(ArgumentParser parser) {
    printUsage(parser);
    if (parser.getHelp().size()) {
        cout << TF::LIGHT_BLUE("\nDescription:\n  ") << parser.getHelp() << endl;
    }

    auto& arguments = parser._pimpl->arguments;
    auto& subparsers = parser._pimpl->subparsers;

    auto argPairIsRequired = [](pair<string, Argument> const& argPair) {
        return argPair.second.isRequired();
    };

    auto argPairIsOptional = [](pair<string, Argument> const& argPair) {
        return !argPair.second.isRequired();
    };

    if (count_if(arguments.begin(), arguments.end(), argPairIsRequired)) {
        cout << TF::LIGHT_BLUE("\nRequired Arguments:\n");
        for (auto const& [_, arg] : arguments) {
            if (arg.isRequired()) {
                printHelpString(parser, arg);
            }
        }
    }

    if (subparsers.has_value() && subparsers->isRequired()) {
        printHelpString(parser, subparsers.value());
    }

    if (count_if(arguments.begin(), arguments.end(), argPairIsOptional)) {
        cout << TF::LIGHT_BLUE("\nOptional Arguments:\n");
        for (auto const& [_, arg] : arguments) {
            if (!arg.isRequired()) {
                printHelpString(parser, arg);
            }
        }
    }

    if (subparsers.has_value() && !subparsers->isRequired()) {
        printHelpString(parser, subparsers.value());
    }
}

/**
 * @brief get the syntax representation string of an argument.
 *
 * @param arg
 * @return string
 */
string Formatter::getSyntaxString(ArgumentParser parser, Argument const& arg) {
    string ret = "";

    if (!arg.hasAction()) {
        ret += requiredArgBracket(
            typeStyle(arg.getTypeString()) + " " + metavarStyle(arg.getMetavar()));
    }
    if (parser.isOption(arg)) {
        ret = optionalStyle(styledArgName(parser, arg)) + (arg.hasAction() ? "" : (" " + ret));
    }

    return ret;
}

string Formatter::getSyntaxString(SubParsers parsers) {
    string ret = "{";
    size_t ctr = 0;
    for (auto const& [name, parser] : parsers.getSubParsers()) {
        ret += styledCmdName(parser.getName(), parser.getNumRequiredChars());
        if (++ctr < parsers.getSubParsers().size()) ret += ", ";
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
string Formatter::requiredArgBracket(std::string const& str) {
    return requiredStyle("<") + str + requiredStyle(">");
}

/**
 * @brief circumfix the string with the optional argument bracket ([]...])
 *
 * @param str
 * @return string
 */
string Formatter::optionalArgBracket(std::string const& str) {
    return optionalStyle("[") + str + optionalStyle("]");
}

/**
 * @brief print the help string of an argument
 *
 * @param arg
 */
void Formatter::printHelpString(ArgumentParser parser, Argument const& arg) {
    using dvlab_utils::Tabler;
    auto& tabl = parser._pimpl->tabl;
    tabl << (arg.hasAction() ? typeStyle("flag") : typeStyle(arg.getTypeString()));

    if (parser.isOption(arg)) {
        if (arg.hasAction()) {
            tabl << Tabler::Multicols(styledArgName(parser, arg), 2);
        } else {
            tabl << styledArgName(parser, arg) << metavarStyle(arg.getMetavar());
        }
    } else {
        tabl << Tabler::Multicols(metavarStyle(arg.getMetavar()), 2);
    }

    tabl << arg.getHelp();
}

void Formatter::printHelpString(ArgumentParser parser, SubParsers parsers) {
    using dvlab_utils::Tabler;
    parser._pimpl->tabl << Tabler::Multicols(getSyntaxString(parsers), 3) << parsers.getHelp();
}

/**
 * @brief print the styled argument name.
 *
 * @param arg
 * @return string
 */
string Formatter::styledArgName(ArgumentParser parser, Argument const& arg) {
    if (!parser.isOption(arg)) return metavarStyle(arg.getName());
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