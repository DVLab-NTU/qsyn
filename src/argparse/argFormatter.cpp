/****************************************************************************
  FileName     [ argFormatter.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser formatter functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>

#include "argparse/argparse.h"
#include "unicode/display_width.hpp"
#include "util/terminalSize.h"
#include "util/textFormat.h"

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
        if (!arg.isRequired() && !conflictGroups.contains(toLowerString(name))) {
            cout << " " << optionalArgBracket(getSyntaxString(parser, arg));
        }
    }

    for (auto const& group : mutexGroups) {
        if (!group.isRequired()) {
            cout << " " + optionalStyle("[");
            size_t ctr = 0;
            for (auto const& name : group.getArguments()) {
                cout << getSyntaxString(parser, arguments.at(toLowerString(name)));
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
                cout << getSyntaxString(parser, arguments.at(toLowerString(name)));
                if (++ctr < group.size()) cout << requiredStyle(" | ");
            }
            cout << requiredStyle(">");
        }
    }

    for (auto const& [name, arg] : arguments) {
        if (arg.isRequired() && !conflictGroups.contains(toLowerString(name))) {
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
    fort::utf8_table table;
    table.set_border_style(FT_EMPTY_STYLE);
    table.set_left_margin(1);

    ft_set_u8strwid_func(
        [](void const* beg, void const* end, size_t* width) -> int {
            std::string tmpStr(static_cast<const char*>(beg), static_cast<const char*>(end));

            *width = unicode::display_width(tmpStr);

            return 0;
        });

    auto [terminal_width, terminal_height] = dvlab_utils::get_terminal_size();

    auto typeStringLength = std::ranges::max(
        parser._pimpl->arguments | views::values |
        views::transform([](Argument const& arg) -> size_t { return arg.getTypeString().size(); }));

    auto nameLength = std::ranges::max(
        parser._pimpl->arguments | views::values |
        views::transform([](Argument const& arg) -> size_t { return arg.getName().size(); }));
    auto metavarLength = std::ranges::max(
        parser._pimpl->arguments | views::values |
        views::transform([](Argument const& arg) -> size_t { return arg.getMetavar().size(); }));

    // 7 = 1 * left margin (1) + 3 * (left+right cell padding (2))
    auto max_help_string_width = terminal_width - typeStringLength - nameLength - metavarLength - 7;

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
                printHelpString(parser, table, max_help_string_width, arg);
            }
        }
    }

    if (subparsers.has_value() && subparsers->isRequired()) {
        printHelpString(parser, table, max_help_string_width, subparsers.value());
    }

    if (count_if(arguments.begin(), arguments.end(), argPairIsOptional)) {
        cout << TF::LIGHT_BLUE("\nOptional Arguments:\n");
        for (auto const& [_, arg] : arguments) {
            if (!arg.isRequired()) {
                printHelpString(parser, table, max_help_string_width, arg);
            }
        }
    }

    if (subparsers.has_value() && !subparsers->isRequired()) {
        printHelpString(parser, table, max_help_string_width, subparsers.value());
    }

    cout << table.to_string() << endl;
}

/**
 * @brief get the syntax representation string of an argument.
 *
 * @param arg
 * @return string
 */
string Formatter::getSyntaxString(ArgumentParser parser, Argument const& arg) {
    string ret = "";

    if (arg.takesArgument()) {
        ret += requiredArgBracket(
            typeStyle(arg.getTypeString()) + " " + metavarStyle(arg.getMetavar()));
    }
    if (parser.hasOptionPrefix(arg)) {
        ret = optionalStyle(styledArgName(parser, arg)) + (arg.takesArgument() ? (" " + ret) : "");
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

std::string insertLineBreaksToString(std::string const& str, size_t max_help_width) {
    std::vector<std::string> lines = split(str, "\n");
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].size() < max_help_width) continue;

        size_t pos = lines[i].find_last_of(' ', max_help_width);

        if (pos == std::string::npos) {
            lines.insert(lines.begin() + i + 1, lines[i].substr(max_help_width));
            lines[i] = lines[i].substr(0, max_help_width);
        } else {
            lines.insert(lines.begin() + i + 1, lines[i].substr(pos + 1));
            lines[i] = lines[i].substr(0, pos);
        }
    }

    return join("\n", lines);
}

/**
 * @brief print the help string of an argument
 *
 * @param arg
 */
void Formatter::printHelpString(ArgumentParser parser, fort::utf8_table& table, size_t max_help_string_width, Argument const& arg) {
    table << typeStyle(arg.takesArgument() ? arg.getTypeString() : "flag");
    if (parser.hasOptionPrefix(arg)) {
        if (arg.takesArgument()) {
            table << styledArgName(parser, arg) << metavarStyle(arg.getMetavar());
        } else {
            table << styledArgName(parser, arg) << "";
        }
    } else {
        table << metavarStyle(arg.getMetavar()) << "";
    }

    table << insertLineBreaksToString(arg.getHelp(), max_help_string_width) << fort::endr;
}

void Formatter::printHelpString(ArgumentParser parser, fort::utf8_table& table, size_t max_help_string_width, SubParsers parsers) {
    table << getSyntaxString(parsers) << ""
          << "" << insertLineBreaksToString(parsers.getHelp(), max_help_string_width) << fort::endr;
}

/**
 * @brief print the styled argument name.
 *
 * @param arg
 * @return string
 */
string Formatter::styledArgName(ArgumentParser parser, Argument const& arg) {
    if (!parser.hasOptionPrefix(arg)) return metavarStyle(arg.getName());
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