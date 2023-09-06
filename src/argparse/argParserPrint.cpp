/****************************************************************************
  FileName     [ formatter.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser formatter functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>

#include <algorithm>
#include <cstring>
#include <fort.hpp>
#include <numeric>
#include <ranges>

#include "./argType.hpp"
#include "./argparse.hpp"
#include "argparse/argGroup.hpp"
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

namespace detail {

std::string styledOptionNameAndAliases(ArgumentParser parser, Argument const& arg) {
    assert(arg.isOption());

    static const auto decorate = [](std::string const& str, size_t nReq) -> std::string {
        if (utils::ANSI_supported()) {
            std::string mand = str.substr(0, nReq);
            std::string rest = str.substr(nReq);
            return optionStyle(accentStyle(mand)) + optionStyle(rest);
        }

        return str;
    };

    std::string retStr = decorate(arg.getName(), parser.getArgNumRequiredChars(arg.getName()));

    if (auto [aliasBegin, aliasEnd] = parser._pimpl->aliasReverseMap.equal_range(arg.getName());
        aliasBegin != aliasEnd) {
        retStr += optionStyle(", ") +
                  fmt::format("{}", fmt::join(std::ranges::subrange(aliasBegin, aliasEnd) |
                                                  std::views::values |
                                                  std::views::transform([&parser](std::string const& alias) {
                                                      return decorate(alias, parser.getArgNumRequiredChars(alias));
                                                  }),
                                              optionStyle(", ")));
    }

    return retStr;
}

/**
 * @brief print the styled argument name.
 *
 * @param arg
 * @return string
 */
std::string styledArgName(ArgumentParser parser, Argument const& arg) {
    if (!arg.isOption()) return metavarStyle(arg.getMetavar());
    if (utils::ANSI_supported()) {
        std::string mand = arg.getName().substr(0, parser.getArgNumRequiredChars(arg.getName()));
        std::string rest = arg.getName().substr(parser.getArgNumRequiredChars(arg.getName()));
        return optionStyle(accentStyle(mand)) + optionStyle(rest);
    }

    return arg.getName();
}

/**
 * @brief return a string of styled command name. The mandatory part is accented.
 *
 * @return string
 */
std::string styledParserName(ArgumentParser const& parser) {
    if (utils::ANSI_supported()) {
        std::string mand = parser.getName().substr(0, parser.getNumRequiredChars());
        std::string rest = parser.getName().substr(parser.getNumRequiredChars());
        return accentStyle(mand) + rest;
    }

    return parser.getName();
}

/**
 * @brief get the syntax representation string of an argument.
 *
 * @param arg
 * @return string
 */
std::string getSyntax(ArgumentParser parser, Argument const& arg) {
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
        ret = styledArgName(parser, arg) + (ret.size() ? (" " + ret) : "");
    }

    return ret;
}

std::string getSyntax(SubParsers const& parsers) {
    return fmt::format("{}{}{}",
                       parsers.isRequired() ? "(" : optionStyle("["),
                       fmt::join(parsers.getSubParsers() | std::views::values | std::views::transform([](ArgumentParser const& parser) { return styledParserName(parser); }), " | "),
                       parsers.isRequired() ? ")" : optionStyle("]"));
}

std::string getSyntax(ArgumentParser parser, MutuallyExclusiveGroup const& group) {
    return fmt::format("{}{}{}",
                       group.isRequired() ? "(" : optionStyle("["),
                       fmt::join(group.getArguments() | std::views::transform([&parser](std::string const& name) { return getSyntax(parser, parser._pimpl->arguments.at(name)); }), group.isRequired() ? " | " : optionStyle(" | ")),
                       group.isRequired() ? ")" : optionStyle("]"));
}

/**
 * @brief insert line breaks to the string to make it fit the terminal width
 *
 * @param str
 * @param max_help_width
 * @return string
 */
std::string wrapText(std::string const& str, size_t max_help_width) {
    if (!dvlab::utils::is_terminal()) return str;

    std::vector<std::string> lines = split(str, "\n");
    for (auto i = 0; i < lines.size(); ++i) {
        if (lines[i].size() < max_help_width) continue;

        size_t pos = lines[i].find_last_of(' ', max_help_width);

        if (pos == std::string::npos) {
            lines.insert(std::next(lines.begin(), i + 1), lines[i].substr(max_help_width));
            lines[i] = lines[i].substr(0, max_help_width);
        } else {
            lines.insert(std::next(lines.begin(), i + 1), lines[i].substr(pos + 1));
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
void tabulateHelpString(ArgumentParser parser, fort::utf8_table& table, size_t max_help_string_width, Argument const& arg) {
    auto usageString = arg.getUsage().has_value() ? arg.getUsage().value() : metavarStyle(arg.getMetavar());

    table << typeStyle(arg.getNArgs().upper > 0 ? arg.getTypeString() : "flag");
    if (arg.isOption()) {
        if (arg.getNArgs().upper > 0) {
            table << styledOptionNameAndAliases(parser, arg) << usageString;
        } else {
            table << styledOptionNameAndAliases(parser, arg) << "";
        }
    } else {
        table << usageString << "";
    }

    table << wrapText(arg.getHelp(), max_help_string_width) << fort::endr;
}

fort::utf8_table getParserHelpTabler() {
    fort::utf8_table table;
    table.set_border_style(FT_EMPTY_STYLE);
    table.set_left_margin(1);
    return table;
}

}  // namespace detail
/**
 * @brief Print the usage of the command
 *
 */
void ArgumentParser::printUsage() const {
    using namespace fmt::literals;
    analyzeOptions();

    fmt::print("{} {}",
               sectionHeaderStyle("Usage:"),
               detail::styledParserName(*this));

    for (auto const& [name, arg] : _pimpl->arguments) {
        if (arg.isOption() && !_pimpl->conflictGroups.contains(name)) {
            fmt::print(" {}{}{}", arg.isRequired() ? "(" : optionStyle("["),
                       detail::getSyntax(*this, arg),
                       arg.isRequired() ? ")" : optionStyle("]"));
        }
    }

    for (auto const& group : _pimpl->mutuallyExclusiveGroups) {
        fmt::print(" {}", detail::getSyntax(*this, group));
    }

    for (auto const& [name, arg] : _pimpl->arguments) {
        if (!arg.isOption() && !_pimpl->conflictGroups.contains(name)) {
            fmt::print(" {}", detail::getSyntax(*this, arg));
        }
    }

    if (_pimpl->subparsers.has_value()) {
        fmt::print(" {} ...", detail::getSyntax(_pimpl->subparsers.value()));
    }

    fmt::println("");
}

/**
 * @brief Print the argument name and help message
 *
 */
void ArgumentParser::printSummary() const {
    using namespace std::string_literals;

    analyzeOptions();

    auto cmdName = detail::styledParserName(*this);
    constexpr auto cmd_name_width = 15;
    fmt::println("{0:<{2}}: {1}", cmdName, getDescription(), cmd_name_width + dvlab::str::ansi_token_size(accentStyle));
}

/**
 * @brief Print the help information for a command
 *
 */
void ArgumentParser::printHelp() const {
    ft_set_u8strwid_func(
        [](void const* beg, void const* end, size_t* width) -> int {
            std::string tmpStr(static_cast<const char*>(beg), static_cast<const char*>(end));

            *width = unicode::display_width(tmpStr);

            return 0;
        });

    printUsage();

    if (getDescription().size()) {
        fmt::println("\n{}", sectionHeaderStyle("Description:"));
        fmt::println("  {}", getDescription());
    }

    auto getMaxLength = [this](std::function<size_t(Argument const&)>&& fn) {
        return _pimpl->arguments.empty() ? 0 : std::ranges::max(_pimpl->arguments | std::views::values | std::views::transform([](Argument const& arg) -> size_t { return arg.getTypeString().size(); }));
    };

    constexpr auto left_margin = 1;
    constexpr auto left_right_cell_padding = 2;
    constexpr auto total_padding = left_margin + 3 * left_right_cell_padding;
    auto max_help_string_width =
        dvlab::utils::get_terminal_size().width -
        getMaxLength([](Argument const& arg) -> size_t { return arg.getTypeString().size(); }) -
        getMaxLength([](Argument const& arg) -> size_t { return arg.getName().size(); }) -
        getMaxLength([](Argument const& arg) -> size_t { return arg.getMetavar().size(); }) -
        total_padding;

    if (std::ranges::any_of(_pimpl->arguments | std::views::values, [](Argument const& arg) { return !arg.isOption(); })) {
        auto table = detail::getParserHelpTabler();
        fmt::println("\n{}", sectionHeaderStyle("Positional Arguments:"));
        for (auto const& [_, arg] : _pimpl->arguments) {
            if (!arg.isOption()) {
                detail::tabulateHelpString(*this, table, max_help_string_width, arg);
            }
        }

        fmt::print("{}", table.to_string());
    }

    if (std::ranges::any_of(_pimpl->arguments | std::views::values, [](Argument const& arg) { return arg.isOption(); })) {
        auto table = detail::getParserHelpTabler();
        fmt::println("\n{}", sectionHeaderStyle("Options:"));
        for (auto const& [_, arg] : _pimpl->arguments) {
            if (arg.isOption()) {
                detail::tabulateHelpString(*this, table, max_help_string_width, arg);
            }
        }
        fmt::print("{}", table.to_string());
    }

    if (_pimpl->subparsers.has_value()) {
        auto table = detail::getParserHelpTabler();
        fmt::println("\n{}", sectionHeaderStyle("Subcommands:"));
        table.write_ln(detail::getSyntax(_pimpl->subparsers.value()), detail::wrapText(_pimpl->subparsers.value().getHelp(), max_help_string_width));
        for (auto& [_, parser] : _pimpl->subparsers->getSubParsers()) {
            if (parser.getDescription().size()) {
                table.write_ln("  " + detail::styledParserName(parser), detail::wrapText(parser.getDescription(), max_help_string_width));
            }
        }

        fmt::print("{}", table.to_string());
    }
}

/**
 * @brief Print the version information of the command
 *
 */
void ArgumentParser::printVersion() const {
    fmt::println("{}", _pimpl->config.version);
}

}  // namespace ArgParse
