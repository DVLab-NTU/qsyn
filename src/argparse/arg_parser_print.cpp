/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define argument parser printing functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>

#include <algorithm>
#include <cstring>
#include <numeric>
#include <ranges>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>

#include "./argparse.hpp"
#include "unicode/display_width.hpp"
#include "util/dvlab_string.hpp"
#include "util/tabler.hpp"
#include "util/terminal_attributes.hpp"
#include "util/text_format.hpp"
#include "util/util.hpp"

namespace dvlab::argparse {

static constexpr auto section_header_styled = [](std::string_view str) -> std::string { return fmt::format("{}", dvlab::fmt_ext::styled_if_ansi_supported(str, fmt::fg(fmt::terminal_color::bright_blue))); };
static constexpr auto required_styled       = [](std::string_view str) -> std::string { return fmt::format("{}", dvlab::fmt_ext::styled_if_ansi_supported(str, fmt::fg(fmt::terminal_color::cyan))); };
static constexpr auto metavar_styled        = [](std::string_view str) -> std::string { return fmt::format("{}", dvlab::fmt_ext::styled_if_ansi_supported(str, fmt::emphasis::bold)); };
static constexpr auto option_styled         = [](std::string_view str) -> std::string { return fmt::format("{}", dvlab::fmt_ext::styled_if_ansi_supported(str, fmt::fg(fmt::terminal_color::yellow))); };
static constexpr auto type_styled           = [](std::string_view str) -> std::string { return fmt::format("{}", dvlab::fmt_ext::styled_if_ansi_supported(str, fmt::fg(fmt::terminal_color::cyan) | fmt::emphasis::italic)); };
static constexpr auto accent_styled         = [](std::string_view str) -> std::string { return fmt::format("{}", dvlab::fmt_ext::styled_if_ansi_supported(str, fmt::emphasis::bold | fmt::emphasis::underline)); };

namespace detail {

std::string styled_option_name_and_aliases(ArgumentParser parser, Argument const& arg) {
    assert(arg.is_option());

    static auto const decorate = [](std::string_view str, size_t n_req) -> std::string {
        if (dvlab::utils::ansi_supported()) {
            auto const mand = str.substr(0, n_req);
            auto const rest = str.substr(n_req);
            return option_styled(accent_styled(mand)) + option_styled(rest);
        }

        return std::string{str};
    };

    std::string ret_str = decorate(arg.get_name(), parser.get_arg_num_required_chars(arg.get_name()));

    if (auto [alias_begin, alias_end] = parser._pimpl->alias_reverse_map.equal_range(arg.get_name());
        alias_begin != alias_end) {
        ret_str += option_styled(", ") +
                   fmt::format("{}", fmt::join(std::ranges::subrange(alias_begin, alias_end) |
                                                   std::views::values |
                                                   std::views::transform([&parser](std::string_view alias) {
                                                       return decorate(alias, parser.get_arg_num_required_chars(alias));
                                                   }),
                                               option_styled(", ")));
    }

    return ret_str;
}

/**
 * @brief print the styled argument name.
 *
 * @param arg
 * @return string
 */
std::string styled_arg_name(ArgumentParser const& parser, Argument const& arg) {
    if (!arg.is_option()) return metavar_styled(arg.get_metavar());
    if (dvlab::utils::ansi_supported()) {
        std::string const mand = arg.get_name().substr(0, parser.get_arg_num_required_chars(arg.get_name()));
        std::string const rest = arg.get_name().substr(parser.get_arg_num_required_chars(arg.get_name()));
        return option_styled(accent_styled(mand)) + option_styled(rest);
    }

    return arg.get_name();
}

/**
 * @brief return a string of styled command name. The mandatory part is accented.
 *
 * @return string
 */
std::string styled_parser_name(ArgumentParser const& parser) {
    if (dvlab::utils::ansi_supported()) {
        std::string const mand = parser.get_name().substr(0, parser.get_num_required_chars());
        std::string const rest = parser.get_name().substr(parser.get_num_required_chars());
        return accent_styled(mand) + rest;
    }

    return parser.get_name();
}

/**
 * @brief return a string of styled command name. The mandatory part is accented.
 *
 * @return string
 */
std::string styled_parser_name_trace(ArgumentParser const& parser) {
    return (parser._pimpl->parent_parser ? styled_parser_name_trace(*parser._pimpl->parent_parser) + " " : "") + styled_parser_name(parser);
}

/**
 * @brief get the syntax representation string of an argument.
 *
 * @param arg
 * @return string
 */
std::string get_syntax(ArgumentParser const& parser, Argument const& arg) {
    std::string ret        = "";
    NArgsRange const nargs = arg.get_nargs();
    if (arg.get_usage().has_value()) {
        return arg.get_usage().value();
    }
    auto const usage_string =
        fmt::format("{}{} {}{}",
                    required_styled("<"),
                    type_styled(arg.get_type_string()),
                    metavar_styled(arg.get_metavar()),
                    required_styled(">"));

    auto count = nargs.lower;

    while (count > 0) {
        count--;
    }
    for (auto i : std::views::iota(0ul, nargs.lower)) {
        ret += fmt::format("{}{}", usage_string, i == nargs.lower - 1 ? "" : " ");
    }
    if (nargs.upper == SIZE_MAX) {
        ret += fmt::format("{}{}{}...", option_styled("["), usage_string, option_styled("]"));
    } else {
        for (auto i : std::views::iota(nargs.lower, nargs.upper)) {
            ret += fmt::format("{}{}{}{}", option_styled("["), usage_string, option_styled("]"), i == nargs.upper - 1 ? "" : " ");
        }
    }

    if (arg.is_option()) {
        ret = styled_arg_name(parser, arg) + (!ret.empty() ? (" " + ret) : "");
    }

    return ret;
}

std::string get_syntax(SubParsers const& parsers) {
    return fmt::format("{}{}{}",
                       parsers.is_required() ? "(" : option_styled("["),
                       fmt::join(parsers.get_subparsers() | std::views::values | std::views::transform([](ArgumentParser const& parser) { return styled_parser_name(parser); }), " | "),
                       parsers.is_required() ? ")" : option_styled("]"));
}

std::string get_syntax(ArgumentParser parser, MutuallyExclusiveGroup const& group) {
    return fmt::format("{}{}{}",
                       group.is_required() ? "(" : option_styled("["),
                       fmt::join(group.get_arg_names() | std::views::transform([&parser](std::string_view name) { return get_syntax(parser, parser._pimpl->arguments.at(std::string{name})); }), group.is_required() ? " | " : option_styled(" | ")),
                       group.is_required() ? ")" : option_styled("]"));
}

/**
 * @brief insert line breaks to the string to make it fit the terminal width
 *
 * @param str
 * @param max_help_width
 * @return string
 */
std::string wrap_text(std::string_view str, size_t max_help_width) {
    if (!dvlab::utils::is_terminal()) return std::string{str};
    auto lines = dvlab::str::views::split_to_string_views(str, '\n') | tl::to<std::vector>();
    max_help_width--;
    // NOTE - the following code modifies the vector while iterating over it.
    //        don't use range-based for loop here.
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].size() < max_help_width) continue;

        size_t const pos = lines[i].find_last_of(' ', max_help_width);

        if (pos == std::string::npos) {
            lines.insert(dvlab::iterator::next(std::begin(lines), i + 1), lines[i].substr(max_help_width));
            lines[i] = lines[i].substr(0, max_help_width);
        } else {
            lines.insert(dvlab::iterator::next(std::begin(lines), i + 1), lines[i].substr(pos + 1));
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
void tabulate_help_string(ArgumentParser const& parser, Tabler& table, size_t max_help_string_width, Argument const& arg) {
    auto usage_string = arg.get_usage().value_or(metavar_styled(arg.get_metavar()));

    std::vector<std::string> row;
    row.reserve(4);

    row.emplace_back(type_styled(arg.get_nargs().upper > 0 ? arg.get_type_string() : "flag"));
    if (arg.is_option()) {
        if (arg.get_nargs().upper > 0) {
            row.emplace_back(styled_option_name_and_aliases(parser, arg));
            row.emplace_back(usage_string);
        } else {
            row.emplace_back(styled_option_name_and_aliases(parser, arg));
            row.emplace_back("");
        }
    } else {
        row.emplace_back(usage_string);
        row.emplace_back("");
    }

    row.emplace_back(wrap_text(arg.get_help(), max_help_string_width));
    table.add_row(row);
}

Tabler create_parser_help_table() {
    Tabler table;
    table.left_margin()        = 1;
    table.cell_left_padding()  = 1;
    table.cell_right_padding() = 1;
    return table;
}

}  // namespace detail
/**
 * @brief Print the usage of the command
 *
 */
void ArgumentParser::print_usage() const {
    analyze_options();

    fmt::print("{} {}",
               section_header_styled("Usage:"),
               detail::styled_parser_name_trace(*this));

    for (auto const& [name, arg] : _pimpl->arguments) {
        if (arg.is_option() && !_pimpl->conflict_groups.contains(name)) {
            fmt::print(" {}{}{}", arg.is_required() ? "(" : option_styled("["),
                       detail::get_syntax(*this, arg),
                       arg.is_required() ? ")" : option_styled("]"));
        }
    }

    for (auto const& group : _pimpl->mutually_exclusive_groups) {
        fmt::print(" {}", detail::get_syntax(*this, group));
    }

    for (auto const& [name, arg] : _pimpl->arguments) {
        if (!arg.is_option() && !_pimpl->conflict_groups.contains(name)) {
            fmt::print(" {}", detail::get_syntax(*this, arg));
        }
    }

    if (_pimpl->subparsers.has_value()) {
        fmt::print(" {} ...", detail::get_syntax(_pimpl->subparsers.value()));
    }

    fmt::println("");
}

/**
 * @brief Print the argument name and help message
 *
 */
void ArgumentParser::print_summary() const {
    analyze_options();

    auto cmd_name                 = detail::styled_parser_name(*this);
    constexpr auto cmd_name_width = 15;
    fmt::println("{0:<{2}}: {1}", cmd_name, get_description(), cmd_name_width + dvlab::str::ansi_token_size(accent_styled));
}

/**
 * @brief Print the help information for a command
 *
 */
void ArgumentParser::print_help() const {
    print_usage();

    if (!get_description().empty()) {
        fmt::println("\n{}", section_header_styled("Description:"));
        fmt::println("  {}", get_description());
    }

    auto const get_max_length = [this](std::function<size_t(Argument const&)> const& fn) {
        return _pimpl->arguments.empty() ? 0 : std::ranges::max(_pimpl->arguments | std::views::values | std::views::transform(fn));
    };

    constexpr auto left_margin   = 2;
    constexpr auto cell_padding  = 2;
    constexpr auto total_padding = left_margin + 3 * cell_padding;
    auto const max_help_string_width =
        dvlab::utils::get_terminal_size().width -
        get_max_length([](Argument const& arg) -> size_t { return unicode::display_width(arg.get_type_string()); }) -
        get_max_length([](Argument const& arg) -> size_t { return unicode::display_width(arg.get_name()); }) -
        get_max_length([](Argument const& arg) -> size_t { return unicode::display_width(arg.get_metavar()); }) -
        total_padding;

    if (std::ranges::any_of(_pimpl->arguments | std::views::values, [](Argument const& arg) { return !arg.is_option(); })) {
        fmt::println("\n{}", section_header_styled("Positional Arguments:"));
        auto table = detail::create_parser_help_table();
        for (auto const& [_, arg] : _pimpl->arguments) {
            if (!arg.is_option()) {
                detail::tabulate_help_string(*this, table, max_help_string_width, arg);
            }
        }

        fmt::print("{}", table.to_string());
    }

    if (std::ranges::any_of(_pimpl->arguments | std::views::values, [](Argument const& arg) { return arg.is_option(); })) {
        fmt::println("\n{}", section_header_styled("Options:"));
        auto table = detail::create_parser_help_table();
        for (auto const& [_, arg] : _pimpl->arguments) {
            if (arg.is_option()) {
                detail::tabulate_help_string(*this, table, max_help_string_width, arg);
            }
        }
        fmt::print("{}", table.to_string());
    }

    if (_pimpl->subparsers.has_value()) {
        fmt::println("\n{}", section_header_styled("Subcommands:"));
        fmt::println("{}  {}", detail::get_syntax(_pimpl->subparsers.value()), detail::wrap_text(_pimpl->subparsers.value().get_help(), max_help_string_width));
        auto table = detail::create_parser_help_table();
        for (auto& [_, parser] : _pimpl->subparsers->get_subparsers()) {
            if (!parser.get_description().empty()) {
                std::vector<std::string> row;
                row.reserve(2);
                row.emplace_back("  " + detail::styled_parser_name(parser));
                row.emplace_back(detail::wrap_text(parser.get_description(), max_help_string_width));
                table.add_row(row);
            }
        }

        fmt::print("{}", table.to_string());
    }
}

/**
 * @brief Print the version information of the command
 *
 */
void ArgumentParser::print_version() const {
    fmt::println("{}", _pimpl->config.version);
}

}  // namespace dvlab::argparse
