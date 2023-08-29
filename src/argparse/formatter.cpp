/****************************************************************************
  FileName     [ formatter.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser formatter functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparse/formatter.hpp"

#include <algorithm>
#include <cstring>
#include <numeric>
#include <ranges>

#include "./argType.hpp"
#include "./argparse.hpp"
#include "argparse/argGroup.hpp"
#include "fmt/core.h"
#include "unicode/display_width.hpp"
#include "util/terminalAttributes.hpp"
#include "util/textFormat.hpp"
#include "util/util.hpp"

using namespace dvlab;

namespace ArgParse {

constexpr auto sectionHeaderStyle = [](std::string const& str) -> std::string { return fmt::format("{}", fmt_ext::styled_if_ANSI_supported(str, fmt::fg(fmt::terminal_color::bright_blue))); };
constexpr auto requiredStyle = [](std::string const& str) -> std::string { return fmt::format("{}", fmt_ext::styled_if_ANSI_supported(str, fmt::fg(fmt::terminal_color::cyan))); };
constexpr auto metavarStyle = [](std::string const& str) -> std::string { return fmt::format("{}", fmt_ext::styled_if_ANSI_supported(str, fmt::emphasis::bold)); };
constexpr auto optionStyle = [](std::string const& str) -> std::string { return fmt::format("{}", fmt_ext::styled_if_ANSI_supported(str, fmt::fg(fmt::terminal_color::yellow))); };
constexpr auto typeStyle = [](std::string const& str) -> std::string { return fmt::format("{}", fmt_ext::styled_if_ANSI_supported(str, fmt::fg(fmt::terminal_color::cyan) | fmt::emphasis::italic)); };
constexpr auto accentStyle = [](std::string const& str) -> std::string { return fmt::format("{}", fmt_ext::styled_if_ANSI_supported(str, fmt::emphasis::bold | fmt::emphasis::underline)); };

/**
 * @brief get the syntax representation string of an argument.
 *
 * @param arg
 * @return string
 */
std::string Formatter::getSyntax(Argument const& arg) {
    std::string ret = "";
    NArgsRange nargs = arg.getNArgs();
    auto usageString = arg.getUsage().has_value()
                           ? arg.getUsage().value()
                           : fmt::format("{}{} {}{}", requiredStyle("<"), typeStyle(arg.getTypeString()), metavarStyle(arg.getMetavar()), requiredStyle(">"));

    if (nargs.upper == SIZE_MAX) {
        if (nargs.lower == 0)
            ret = optionStyle("[") + usageString + optionStyle("]") + "...";
        else {
            auto repeat_view = std::views::iota(0u, nargs.lower) | std::views::transform([&usageString](size_t i) { return usageString; });
            ret = fmt::format("{}...", fmt::join(repeat_view, " "));
        }
    } else {
        auto repeat_view = std::views::iota(0u, nargs.upper) | std::views::transform([&usageString, &nargs](size_t i) { return (i < nargs.lower) ? usageString : (optionStyle("[") + usageString + optionStyle("]")); });
        ret = fmt::format("{}", fmt::join(repeat_view, " "));
    }
    if (arg.isOption()) {
        ret = styledArgName(arg) + (ret.size() ? (" " + ret) : "");
    }

    return ret;
}

std::string Formatter::getSyntax(SubParsers const& parsers) {
    return fmt::format("{}{}{}",
                       parsers.isRequired() ? "(" : optionStyle("["),
                       fmt::join(parsers.getSubParsers() | std::views::values | std::views::transform([](ArgumentParser const& parser) { return styledParserName(parser); }), " | "),
                       parsers.isRequired() ? ")" : optionStyle("]"));
}

std::string Formatter::getSyntax(ArgumentParser parser, MutuallyExclusiveGroup const& group) {
    return fmt::format("{}{}{}",
                       group.isRequired() ? "(" : optionStyle("["),
                       fmt::join(group.getArguments() | std::views::transform([&parser](std::string const& name) { return getSyntax(parser._pimpl->arguments.at(name)); }), group.isRequired() ? " | " : optionStyle(" | ")),
                       group.isRequired() ? ")" : optionStyle("]"));
}

/**
 * @brief insert line breaks to the string to make it fit the terminal width
 *
 * @param str
 * @param max_help_width
 * @return string
 */
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

    return fmt::format("{}", fmt::join(lines, "\n"));
}

/**
 * @brief print the help string of an argument
 *
 * @param arg
 */
void Formatter::tabulateHelpString(fort::utf8_table& table, size_t max_help_string_width, Argument const& arg) {
    auto usageString = arg.getUsage().has_value() ? arg.getUsage().value() : metavarStyle(arg.getMetavar());

    table << typeStyle(arg.mayTakeArgument() ? arg.getTypeString() : "flag");
    if (arg.isOption()) {
        if (arg.mayTakeArgument()) {
            table << styledArgName(arg) << usageString;
        } else {
            table << styledArgName(arg) << "";
        }
    } else {
        table << usageString << "";
    }

    table << insertLineBreaksToString(arg.getHelp(), max_help_string_width) << fort::endr;
}

/**
 * @brief print the styled argument name.
 *
 * @param arg
 * @return string
 */
std::string Formatter::styledArgName(Argument const& arg) {
    if (!arg.isOption()) return metavarStyle(arg.getMetavar());

    if (utils::ANSI_supported()) {
        std::string mand = arg.getName().substr(0, arg.getNumRequiredChars());
        std::string rest = arg.getName().substr(arg.getNumRequiredChars());
        return optionStyle(accentStyle(mand)) + optionStyle(rest);
    }

    return arg.getName();
}

/**
 * @brief return a string of styled command name. The mandatory part is accented.
 *
 * @return string
 */
std::string Formatter::styledParserName(ArgumentParser const& parser) {
    if (utils::ANSI_supported()) {
        std::string mand = parser.getName().substr(0, parser.getNumRequiredChars());
        std::string rest = parser.getName().substr(parser.getNumRequiredChars());
        return accentStyle(mand) + rest;
    }

    return parser.getName();
}

/**
 * @brief Print the usage of the command
 *
 */
void Formatter::printUsage(ArgumentParser const& parser) {
    using namespace fmt::literals;
    if (!parser.analyzeOptions()) {
        fmt::println(stderr, "[ArgParse] Failed to generate usage information!!");
        return;
    }

    auto const& arguments = parser._pimpl->arguments;
    auto const& subparsers = parser._pimpl->subparsers;
    auto const& mutexGroups = parser._pimpl->mutuallyExclusiveGroups;
    auto const& conflictGroups = parser._pimpl->conflictGroups;

    fmt::print("{} {}",
               sectionHeaderStyle("Usage:"),
               styledParserName(parser));

    for (auto const& [name, arg] : arguments) {
        if (arg.isOption() && !conflictGroups.contains(name)) {
            fmt::print(" {}{}{}", arg.isRequired() ? "(" : optionStyle("["),
                       getSyntax(arg),
                       arg.isRequired() ? ")" : optionStyle("]"));
        }
    }

    for (auto const& group : mutexGroups) {
        fmt::print(" {}", getSyntax(parser, group));
    }

    for (auto const& [name, arg] : arguments) {
        if (!arg.isOption() && !conflictGroups.contains(name)) {
            fmt::print(" {}", getSyntax(arg));
        }
    }

    if (subparsers.has_value()) {
        fmt::print(" {} ...", getSyntax(subparsers.value()));
    }

    fmt::println("");
}

/**
 * @brief Print the argument name and help message
 *
 */
void Formatter::printSummary(ArgumentParser const& parser) {
    using namespace std::string_literals;

    if (!parser.analyzeOptions()) {
        fmt::println(stderr, "[ArgParse] Failed to generate usage information!!");
        return;
    }

    auto cmdName = styledParserName(parser);
    fmt::println("{0:<{2}}: {1}", cmdName, parser.getHelp(), 15 + dvlab::str::ansi_token_size(accentStyle));
}

/**
 * @brief Print the help information for a command
 *
 */
void Formatter::printHelp(ArgumentParser const& parser) {
    fort::utf8_table table;
    table.set_border_style(FT_EMPTY_STYLE);
    table.set_left_margin(1);

    ft_set_u8strwid_func(
        [](void const* beg, void const* end, size_t* width) -> int {
            std::string tmpStr(static_cast<const char*>(beg), static_cast<const char*>(end));

            *width = unicode::display_width(tmpStr);

            return 0;
        });

    printUsage(parser);

    if (parser.getHelp().size()) {
        fmt::println("\n{}", sectionHeaderStyle("Description:"));
        fmt::println("  {}", parser.getHelp());
    }

    auto& arguments = parser._pimpl->arguments;

    auto [terminal_width, terminal_height] = dvlab::utils::get_terminal_size();

    auto typeStringLength = arguments.empty() ? 0 : std::ranges::max(arguments | std::views::values | std::views::transform([](Argument const& arg) -> size_t { return arg.getTypeString().size(); }));

    auto nameLength = arguments.empty() ? 0 : std::ranges::max(arguments | std::views::values | std::views::transform([](Argument const& arg) -> size_t { return arg.getName().size(); }));

    auto metavarLength = arguments.empty() ? 0 : std::ranges::max(arguments | std::views::values | std::views::transform([](Argument const& arg) -> size_t { return arg.getMetavar().size(); }));

    // 7 = 1 * left margin (1) + 3 * (left+right cell padding (2))
    auto max_help_string_width = terminal_width - typeStringLength - nameLength - metavarLength - 7;

    auto& subparsers = parser._pimpl->subparsers;

    auto argPairIsPositional = [](std::pair<std::string, Argument> const& argPair) {
        return !argPair.second.isOption();
    };

    auto argPairIsOptions = [](std::pair<std::string, Argument> const& argPair) {
        return argPair.second.isOption();
    };

    bool hasPositionalArguments = find_if(arguments.begin(), arguments.end(), argPairIsPositional) != arguments.end();
    bool hasOptions = find_if(arguments.begin(), arguments.end(), argPairIsOptions) != arguments.end();

    if (hasPositionalArguments) {
        fmt::println("\n{}", sectionHeaderStyle("Positional Arguments:"));
        for (auto const& [_, arg] : arguments) {
            if (!arg.isOption()) {
                tabulateHelpString(table, max_help_string_width, arg);
            }
        }

        fmt::print("{}", table.to_string());
    }

    if (hasOptions) {
        table = fort::utf8_table();
        table.set_border_style(FT_EMPTY_STYLE);
        table.set_left_margin(1);
        fmt::println("\n{}", sectionHeaderStyle("Options:"));
        for (auto const& [_, arg] : arguments) {
            if (arg.isOption()) {
                tabulateHelpString(table, max_help_string_width, arg);
            }
        }
        fmt::print("{}", table.to_string());
    }

    if (subparsers.has_value()) {
        table = fort::utf8_table();
        table.set_border_style(FT_EMPTY_STYLE);
        table.set_left_margin(1);
        fmt::println("\n{}", sectionHeaderStyle("Subcommands:"));
        table << getSyntax(subparsers.value()) << insertLineBreaksToString(subparsers.value().getHelp(), max_help_string_width) << fort::endr;
        for (auto& [_, parser] : subparsers->getSubParsers()) {
            if (parser.getHelp().size()) {
                table << "  " + styledParserName(parser) << insertLineBreaksToString(parser.getHelp(), max_help_string_width) << fort::endr;
            }
        }

        fmt::print("{}", table.to_string());
    }
}

}  // namespace ArgParse