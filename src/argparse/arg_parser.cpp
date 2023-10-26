/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define argument parser core functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./arg_parser.hpp"

#include <fmt/format.h>

#include <cassert>
#include <numeric>
#include <ranges>

#include "fmt/core.h"
#include "tl/adjacent.hpp"
#include "tl/enumerate.hpp"
#include "util/trie.hpp"
#include "util/util.hpp"

namespace dvlab::argparse {

ArgumentParser::ArgumentParser(std::string const& n, ArgumentParserConfig config) : ArgumentParser() {
    this->name(n);

    if (config.add_help_action) {
        this->add_argument<bool>("-h", "--help")
            .action(help)
            .help("show this help message and exit");
    }
    if (config.add_version_action) {
        this->add_argument<bool>("-V", "--version")
            .action(version)
            .help("show program's version number and exit");
    }

    _pimpl->config = config;
}

size_t ArgumentParser::num_parsed_args() const {
    return std::ranges::count_if(
        _pimpl->arguments | std::views::values,
        [](Argument& arg) {
            return arg.is_parsed();
        });
}

/**
 * @brief Print the tokens and their parse states
 *
 */
void ArgumentParser::print_tokens() const {
    for (auto&& [i, token] : tl::views::enumerate(_pimpl->tokens)) {
        fmt::println("Token #{:<8}:\t{} ({}) Frequency: {:>3}",
                     i, token.token, (token.parsed ? "parsed" : "unparsed"), _pimpl->identifiers.frequency(token.token));
    }
}

/**
 * @brief Print the argument and their parse states
 *
 */
void ArgumentParser::print_arguments() const {
    for (auto& [_, arg] : _pimpl->arguments) {
        arg.print_status();
    }
}

Argument& ArgumentParser::_get_arg(std::string const& name) {
    return _get_arg_impl(*this, name);
}

Argument const& ArgumentParser::_get_arg(std::string const& name) const {
    return _get_arg_impl(*this, name);
}

/**
 * @brief set the command name to the argument parser
 *
 * @param name
 * @return ArgumentParser&
 */
ArgumentParser& ArgumentParser::name(std::string const& name) {
    _pimpl->name = name;
    return *this;
}

/**
 * @brief set the help message to the argument parser
 *
 * @param name
 * @return ArgumentParser&
 */
ArgumentParser& ArgumentParser::description(std::string const& help) {
    _pimpl->description = help;
    return *this;
}

ArgumentParser& ArgumentParser::num_required_chars(size_t num) {
    _pimpl->num_required_chars = num;
    return *this;
}

ArgumentParser& ArgumentParser::option_prefix(std::string const& prefix) {
    _pimpl->option_prefixes = prefix;
    return *this;
}

size_t ArgumentParser::get_arg_num_required_chars(std::string const& name) const {
    assert(_pimpl->arguments.contains(name) || _pimpl->alias_forward_map.contains(name));
    auto n_req = _pimpl->identifiers.shortest_unique_prefix(name).size();
    while (_pimpl->option_prefixes.find_first_of(name[n_req - 1]) != std::string::npos) {
        ++n_req;
    }
    return n_req;
}

// Parser subroutine

/**
 * @brief Analyze the options for the argument parser. This function generates
 *        auxiliary parsing information for the argument parser.
 *
 * @return true
 * @return false
 */
bool ArgumentParser::analyze_options() const {
    if (_pimpl->options_analyzed) return true;

    // calculate the number of required characters to differentiate each option

    _pimpl->identifiers.clear();
    _pimpl->conflict_groups.clear();

    if (_pimpl->subparsers.has_value()) {
        for (auto const& [name, parser] : _pimpl->subparsers->get_subparsers()) {
            _pimpl->identifiers.insert(name);
        }
    }

    for (auto const& group : _pimpl->mutually_exclusive_groups) {
        for (auto const& name : group.get_arg_names()) {
            if (_pimpl->arguments.at(name).is_required()) {
                fmt::println(stderr, "[ArgParse] Error: mutually exclusive argument \"{}\" must be optional!!", name);
                exit(1);
                return false;
            }
            _pimpl->conflict_groups.emplace(name, group);
        }
    }

    for (auto& [name, arg] : _pimpl->arguments) {
        if (!has_option_prefix(name)) continue;
        _pimpl->identifiers.insert(name);
        arg.set_is_option(true);
    }

    for (auto& alias : _pimpl->alias_forward_map | std::views::keys) {
        assert(has_option_prefix(alias));
        _pimpl->identifiers.insert(alias);
    }

    if (_pimpl->subparsers.has_value()) {
        for (auto& [name, parser] : _pimpl->subparsers->get_subparsers()) {
            size_t prefix_size = _pimpl->identifiers.shortest_unique_prefix(name).size();
            while (!isalpha(name[prefix_size - 1])) ++prefix_size;
            parser.num_required_chars(std::max(prefix_size, parser.get_num_required_chars()));
        }
    }

    _pimpl->options_analyzed = true;
    return true;
}

/**
 * @brief parse the arguments from tokens
 *
 * @param tokens
 * @return true
 * @return false
 */
bool ArgumentParser::parse_args(std::vector<std::string> const& tokens) {
    auto tmp = std::vector<Token>{std::begin(tokens), std::end(tokens)};
    return parse_args(tmp);
}

/**
 * @brief parse the arguments from tokens
 *
 * @param tokens
 * @return true
 * @return false
 */
bool ArgumentParser::parse_args(TokensView tokens) {
    auto [success, unrecognized] = parse_known_args(tokens);

    if (!success) return false;

    return dvlab::utils::expect(unrecognized.empty(),
                                fmt::format("Error: unrecognized arguments: \"{}\"!!",
                                            fmt::join(unrecognized | std::views::transform([](Token const& tok) { return tok.token; }), "\" \"")));
}

/**
 * @brief  parse the arguments known by the tokens from tokens
 *
 * @return std::pair<bool, std::vector<Token>>, where
 *         the first return value specifies whether the parse has succeeded, and
 *         the second one specifies the unrecognized tokens
 */
std::pair<bool, std::vector<Token>> ArgumentParser::parse_known_args(std::vector<std::string> const& tokens) {
    auto tmp = std::vector<Token>{std::begin(tokens), std::end(tokens)};
    return parse_known_args(tmp);
}

/**
 * @brief  parse the arguments known by the tokens from tokens
 *
 * @return std::pair<bool, std::vector<Token>>, where
 *         the first return value specifies whether the parse has succeeded, and
 *         the second one specifies the unrecognized tokens
 */
std::pair<bool, std::vector<Token>> ArgumentParser::parse_known_args(TokensView tokens) {
    auto result = _parse_known_args_impl(tokens);
    if (!result.first && _pimpl->config.exitOnFailure) {
        exit(0);
    }

    return result;
}

/**
 * @brief the internal implementation of parse_known_args
 *
 * @param tokens
 * @return std::pair<bool, std::vector<Token>>
 */
std::pair<bool, std::vector<Token>> ArgumentParser::_parse_known_args_impl(TokensView tokens) {
    if (!analyze_options()) return {false, {}};
    _pimpl->activated_subparser = std::nullopt;
    for (auto& mutex : _pimpl->mutually_exclusive_groups) {
        mutex.set_parsed(false);
    }

    auto subparser_token_pos = std::invoke([this, tokens]() -> size_t {
        if (!_pimpl->subparsers.has_value())
            return tokens.size();

        for (auto const& [pos, token] : tl::views::enumerate(tokens)) {
            for (auto const& [name, subparser] : _pimpl->subparsers->get_subparsers()) {
                if (name.starts_with(token.token)) {
                    _activate_subparser(name);
                    return pos;
                }
            }
        }
        return tokens.size();
    });

    for (auto& arg : _pimpl->arguments | std::views::values) {
        arg.reset();
    }

    TokensView const main_parser_tokens = tokens.subspan(0, subparser_token_pos);

    std::vector<Token> unrecognized;
    if (!_parse_options(main_parser_tokens) ||
        !_parse_positional_arguments(main_parser_tokens, unrecognized)) {
        return {false, {}};
    }
    _fill_unparsed_args_with_defaults();
    if (has_subparsers()) {
        TokensView const subparser_tokens = tokens.subspan(subparser_token_pos + 1);
        if (_pimpl->activated_subparser) {
            auto const [success, subparser_unrecognized] = _get_activated_subparser()->parse_known_args(subparser_tokens);
            if (!success) return {false, {}};
            unrecognized.insert(std::end(unrecognized), std::begin(subparser_unrecognized), std::end(subparser_unrecognized));
        } else if (_pimpl->subparsers->is_required()) {
            fmt::println(stderr, "Error: missing mandatory subparser argument: ({})", fmt::join(_pimpl->subparsers->get_subparsers() | std::views::keys, ", "));
            return {false, {}};
        }
    }

    return {true, unrecognized};
}

/**
 * @brief Parse the optional arguments, i.e., the arguments that starts with
 *        one of the option prefix.
 *
 * @return true if succeeded
 * @return false if failed
 */
bool ArgumentParser::_parse_options(TokensView tokens) {
    for (auto const& [i, token_pair] : tl::views::enumerate(tokens)) {
        auto const& token = token_pair.token;
        auto& parsed      = token_pair.parsed;
        if (!has_option_prefix(token) || parsed) continue;
        auto match = _match_option(token);

        if (std::holds_alternative<size_t>(match)) {
            // check if the argument is a number
            if (dvlab::str::from_string<float>(token).has_value())
                continue;
            auto frequency = std::get<size_t>(match);
            assert(frequency != 1);

            // If the option is unrecognized, skip to the next arg
            // because it may be a positional argument
            if (frequency == 0) continue;

            // Else the option is ambiguous. Report the error and quit parsing
            auto const matching_option_filter = [this, &token = token](std::string const& name) {
                return has_option_prefix(name) && name.starts_with(token);
            };
            fmt::println(stderr, "Error: ambiguous option: \"{}\" could match {}",
                         token, fmt::join(_pimpl->arguments | std::views::keys | std::views::filter(matching_option_filter), ", "));
            return false;
        }
        Argument& arg = _get_arg(std::get<std::string>(match));

        if (arg.is_help_action()) {
            this->print_help();
            return false;  // break the parsing
        }

        if (arg.is_version_action()) {
            this->print_version();
            return false;  // break the parsing
        }

        parsed           = true;
        auto parse_range = arg.get_parse_range(tokens.subspan(i + 1));
        if (!arg.tokens_enough_to_parse(parse_range)) {
            fmt::println(stderr, "Error: missing argument(s) for \"{}\": expected {}{} arguments!!",
                         arg.get_name(), (arg.get_nargs().lower < arg.get_nargs().upper ? "at least " : ""), arg.get_nargs().lower);
            return false;
        }

        if (!arg.take_action(parse_range)) {
            return false;
        }

        if (!_no_conflict_with_parsed_arguments(arg)) return false;

        arg.mark_as_parsed();  // if the options is present, no matter if there's any argument the follows, mark it as parsed
    }

    return _all_required_options_are_parsed();
}

/**
 * @brief Parse positional arguments, i.e., arguments that must appear in a specific order.
 *
 * @return true if succeeded
 * @return false if failed
 */
bool ArgumentParser::_parse_positional_arguments(TokensView tokens, std::vector<Token>& unrecognized) {
    for (auto& [name, arg] : _pimpl->arguments) {
        if (arg.is_parsed() || has_option_prefix(name)) continue;

        auto parse_range    = arg.get_parse_range(tokens);
        auto [lower, upper] = arg.get_nargs();

        if (parse_range.size() < arg.get_nargs().lower) {
            if (arg.is_required()) {
                fmt::println(stderr, "Error: missing argument \"{}\": expected {}{} arguments!!",
                             arg.get_name(), (lower < upper ? "at least " : ""), lower);
                return false;
            } else
                continue;
        }

        if (!arg.take_action(parse_range)) return false;

        if (parse_range.size()) {
            if (!_no_conflict_with_parsed_arguments(arg)) return false;
            arg.mark_as_parsed();
        }
    }
    std::ranges::copy_if(tokens, back_inserter(unrecognized), [](Token const& token) { return !token.parsed; });

    return _all_required_args_are_parsed() && _all_required_mutex_groups_are_parsed();
}

void ArgumentParser::_fill_unparsed_args_with_defaults() {
    for (auto& [name, arg] : _pimpl->arguments) {
        if (!arg.is_parsed() && arg.has_default_value()) {
            arg.set_value_to_default();
        }
    }
}

/**
 * @brief Get the matching option name to a token.
 *
 * @param token
 * @return optional<string> return the option name if exactly one option matches the token. Otherwise, return std::nullopt
 */
std::variant<std::string, size_t> ArgumentParser::_match_option(std::string const& token) const {
    auto match = _pimpl->identifiers.find_with_prefix(token);
    if (match.has_value()) {
        if (token.size() < get_arg_num_required_chars(match.value())) {
            return 0u;
        }
        return match.value();
    }

    return _pimpl->identifiers.frequency(token);
}

bool ArgumentParser::_no_conflict_with_parsed_arguments(Argument const& arg) const {
    if (!_pimpl->conflict_groups.contains(arg.get_name())) return true;

    auto& mutex_group = _pimpl->conflict_groups.at(arg.get_name());
    if (!mutex_group.is_parsed()) {
        mutex_group.set_parsed(true);
        return true;
    }

    return std::ranges::all_of(mutex_group.get_arg_names(), [this, &arg](std::string const& name) {
        return dvlab::utils::expect(name == arg.get_name() || !_pimpl->arguments.at(name).is_parsed(), fmt::format("Error: argument \"{}\" cannot occur with \"{}\"!!", arg.get_name(), name));
    });
}

/**
 * @brief Check if all required options are parsed
 *
 * @return true or false
 */
bool ArgumentParser::_all_required_options_are_parsed() const {
    auto required_option_range = _pimpl->arguments | std::views::values |
                                 std::views::filter([](Argument const& arg) { return arg.is_option(); }) |
                                 std::views::filter([](Argument const& arg) { return arg.is_required(); });
    return dvlab::utils::expect(
        std::ranges::all_of(required_option_range, [](Argument const& arg) { return arg.is_parsed(); }),
        fmt::format("Error: missing option(s)!! The following options are required: {}",  // intentional linebreak
                    fmt::join(required_option_range |
                                  std::views::filter([](Argument const& arg) { return !arg.is_parsed(); }) |
                                  std::views::transform([](Argument const& arg) { return arg.get_name(); }),
                              ", ")));
}

/**
 * @brief Check if all required groups are parsed
 *
 * @return true or false
 */
bool ArgumentParser::_all_required_mutex_groups_are_parsed() const {
    return std::ranges::all_of(_pimpl->mutually_exclusive_groups, [](MutuallyExclusiveGroup const& group) {
        return dvlab::utils::expect(!group.is_required() || group.is_parsed(),
                                    fmt::format("Error: one of the options are required: {}!!", fmt::join(group.get_arg_names(), ", ")));
    });
}

/**
 * @brief Check if all required arguments are parsed
 *
 * @return true or false
 */
bool ArgumentParser::_all_required_args_are_parsed() const {
    auto required_arg_range = _pimpl->arguments | std::views::values |
                              std::views::filter([](Argument const& arg) { return arg.is_required(); });
    return dvlab::utils::expect(
        std::ranges::all_of(required_arg_range, [](Argument const& arg) { return arg.is_parsed(); }),
        fmt::format("Error: missing argument(s)!! The following arguments are required: {}",  // intentional linebreak
                    fmt::join(required_arg_range |
                                  std::views::filter([](Argument const& arg) { return !arg.is_parsed(); }) |
                                  std::views::transform([](Argument const& arg) { return arg.get_name(); }),
                              ", ")));
}

[[nodiscard]] MutuallyExclusiveGroup ArgumentParser::add_mutually_exclusive_group() {
    _pimpl->mutually_exclusive_groups.emplace_back(*this);
    return _pimpl->mutually_exclusive_groups.back();
}

/**
 * @brief add a parser to the subparser group. The parser will be initialized with the parent parser's config.
 *
 * @param name
 * @return ArgumentParser
 */
ArgumentParser SubParsers::add_parser(std::string const& name) {
    return add_parser(name, _pimpl->parent_config);
}

/**
 * @brief add a parser to the subparser group with a custom config.
 *
 * @param name
 * @param config
 * @return ArgumentParser
 */
ArgumentParser SubParsers::add_parser(std::string const& name, ArgumentParserConfig const& config) {
    _pimpl->subparsers.emplace(name, ArgumentParser{name, config});
    return _pimpl->subparsers.at(name);
}
/**
 * @brief
 *
 * @return SubParsers
 */
[[nodiscard]] SubParsers ArgumentParser::add_subparsers() {
    if (_pimpl->subparsers.has_value()) {
        fmt::println(stderr, "Error: an ArgumentParser can only have one set of subparsers!!");
        exit(-1);
    }
    _pimpl->subparsers = SubParsers{this->_pimpl->config};
    return _pimpl->subparsers.value();
}

}  // namespace dvlab::argparse
