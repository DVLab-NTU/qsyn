/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define argument parser printing functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>

#include <algorithm>
#include <cstring>
#include <fort.hpp>
#include <numeric>
#include <ranges>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>

#include "./argparse.hpp"
#include "unicode/display_width.hpp"
#include "util/dvlab_string.hpp"
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
    std::string ret   = "";
    NArgsRange nargs  = arg.get_nargs();
    auto usage_string = arg.get_usage().has_value()
                            ? arg.get_usage().value()
                            : fmt::format("{}{} {}{}", required_styled("<"), type_styled(arg.get_type_string()), metavar_styled(arg.get_metavar()), required_styled(">"));

    bool const is_required = arg.is_required();

    if (nargs.upper == SIZE_MAX) {
        if (nargs.lower == 0 || !is_required)
            ret = option_styled("[") + usage_string + option_styled("]") + "...";
        else {
            auto repeat_view = std::views::iota(0u, nargs.lower) | std::views::transform([&usage_string](size_t /*i*/) { return usage_string; });
            ret              = fmt::format("{}...", fmt::join(repeat_view, " "));
        }
    } else {
        auto repeat_view = std::views::iota(0u, nargs.upper) | std::views::transform([&usage_string, &nargs, &is_required](size_t i) { return (i < nargs.lower && is_required) ? usage_string : (option_styled("[") + usage_string + option_styled("]")); });
        ret              = fmt::format("{}", fmt::join(repeat_view, " "));
    }

    if (arg.is_option()) {
        ret = styled_arg_name(parser, arg) + (ret.size() ? (" " + ret) : "");
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
void tabulate_help_string(ArgumentParser const& parser, fort::utf8_table& table, size_t max_help_string_width, Argument const& arg) {
    auto usage_string = arg.get_usage().has_value() ? arg.get_usage().value() : metavar_styled(arg.get_metavar());

    table << type_styled(arg.get_nargs().upper > 0 ? arg.get_type_string() : "flag");
    if (arg.is_option()) {
        if (arg.get_nargs().upper > 0) {
            table << styled_option_name_and_aliases(parser, arg) << usage_string;
        } else {
            table << styled_option_name_and_aliases(parser, arg) << "";
        }
    } else {
        table << usage_string << "";
    }

    table << wrap_text(arg.get_help(), max_help_string_width) << fort::endr;
}

fort::utf8_table create_parser_help_table() {
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
    ft_set_u8strwid_func(
        [](void const* beg, void const* end, size_t* width) -> int {
            std::string const tmp_str(static_cast<const char*>(beg), static_cast<const char*>(end));

            *width = unicode::display_width(tmp_str);

            return 0;
        });

    print_usage();

    if (get_description().size()) {
        fmt::println("\n{}", section_header_styled("Description:"));
        fmt::println("  {}", get_description());
    }

    auto get_max_length = [this](std::function<size_t(Argument const&)> const fn) {
        return _pimpl->arguments.empty() ? 0 : std::ranges::max(_pimpl->arguments | std::views::values | std::views::transform(fn));
    };

    constexpr auto left_margin             = 1;
    constexpr auto left_right_cell_padding = 2;
    constexpr auto total_padding           = left_margin + 3 * left_right_cell_padding;
    auto max_help_string_width =
        dvlab::utils::get_terminal_size().width -
        get_max_length([](Argument const& arg) -> size_t { return arg.get_type_string().size(); }) -
        get_max_length([](Argument const& arg) -> size_t { return arg.get_name().size(); }) -
        get_max_length([](Argument const& arg) -> size_t { return arg.get_metavar().size(); }) -
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
            if (parser.get_description().size()) {
                table.write_ln("  " + detail::styled_parser_name(parser), detail::wrap_text(parser.get_description(), max_help_string_width));
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
