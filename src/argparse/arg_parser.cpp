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
#include <stdexcept>

#include "argparse/arg_def.hpp"
#include "fmt/core.h"
#include "tl/adjacent.hpp"
#include "tl/enumerate.hpp"
#include "util/trie.hpp"
#include "util/util.hpp"

namespace dvlab::argparse {

struct SubParsers::SubParsersImpl {
    SubParsers::MapType subparsers;
    std::string help;
    bool required = false;
    ArgumentParser parent_parser;
};

SubParsers::SubParsers(ArgumentParser& parent_parser) : _pimpl{std::make_shared<SubParsersImpl>()} {
    assert(parent_parser._pimpl->subparsers == std::nullopt);
    _pimpl->parent_parser = parent_parser;
}
SubParsers SubParsers::required(bool is_req) {
    _pimpl->required = is_req;
    return *this;
}
SubParsers SubParsers::help(std::string_view help) {
    _pimpl->help = help;
    return *this;
}

size_t SubParsers::size() const noexcept {
    return _pimpl->subparsers.size();
}

bool SubParsers::is_required() const noexcept {
    return _pimpl->required;
}

SubParsers::MapType const& SubParsers::get_subparsers() const { return _pimpl->subparsers; }
std::string const& SubParsers::get_help() const { return _pimpl->help; }

ArgumentParser::ArgumentParser(std::string_view n, ArgumentParserConfig config) : ArgumentParser() {
    this->name(n);

    if (config.add_help_action) {
        this->add_argument<bool>("-h", "--help")
            .action(help)
            .help(fmt::format("show this help message{}", config.exit_on_failure ? " and exit" : ""));
    }
    if (config.add_version_action) {
        this->add_argument<bool>("-V", "--version")
            .action(version)
            .help(fmt::format("show the program's version number{}", config.exit_on_failure ? " and exit" : ""));
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

bool ArgumentParser::_has_arg(std::string_view name) const {
    return _pimpl->arguments.contains(name) ||
           _pimpl->alias_forward_map.contains(name) ||
           (_pimpl->subparsers.has_value() && get_activated_subparser()->_has_arg(name));
}

std::optional<ArgumentParser> ArgumentParser::get_activated_subparser() const {
    if (!_pimpl->subparsers.has_value()) return std::nullopt;
    if (!_pimpl->activated_subparser.has_value()) return std::nullopt;
    return _pimpl->subparsers->get_subparsers().at(_pimpl->activated_subparser.value());
}

bool ArgumentParser::used_subparser(std::string_view name) const {
    auto activated_subparser = get_activated_subparser();
    return activated_subparser.has_value() && (activated_subparser->get_name() == name || activated_subparser->used_subparser(name));
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

Argument& ArgumentParser::_get_arg(std::string_view name) {
    return _get_arg_impl(*this, name);
}

Argument const& ArgumentParser::_get_arg(std::string_view name) const {
    return _get_arg_impl(*this, name);
}

/**
 * @brief set the command name to the argument parser
 *
 * @param name
 * @return ArgumentParser&
 */
ArgumentParser& ArgumentParser::name(std::string_view name) {
    _pimpl->name = name;
    return *this;
}

/**
 * @brief set the help message to the argument parser
 *
 * @param name
 * @return ArgumentParser&
 */
ArgumentParser& ArgumentParser::description(std::string_view help) {
    _pimpl->description = help;
    return *this;
}

ArgumentParser& ArgumentParser::num_required_chars(size_t num) {
    _pimpl->num_required_chars = num;
    return *this;
}

ArgumentParser& ArgumentParser::option_prefix(std::string_view prefix) {
    _pimpl->option_prefixes = prefix;
    return *this;
}

size_t ArgumentParser::get_arg_num_required_chars(std::string_view name) const {
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
                throw std::runtime_error("mutually exclusive argument must be optional");
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
bool ArgumentParser::parse_args(TokensSpan tokens) {
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
std::pair<bool, std::vector<Token>> ArgumentParser::parse_known_args(TokensSpan tokens) {
    auto result = _parse_known_args_impl(tokens);
    if (!result.first && _pimpl->config.exit_on_failure) {
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
std::pair<bool, std::vector<Token>> ArgumentParser::_parse_known_args_impl(TokensSpan tokens) {
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

    TokensSpan const main_parser_tokens = tokens.subspan(0, subparser_token_pos);

    std::vector<Token> unrecognized;
    if (!_parse_options(main_parser_tokens) ||
        !_parse_positional_arguments(main_parser_tokens, unrecognized)) {
        return {false, {}};
    }
    _fill_unparsed_args_with_defaults();
    if (has_subparsers()) {
        TokensSpan const subparser_tokens = tokens.subspan(subparser_token_pos + 1);
        if (_pimpl->activated_subparser) {
            auto const [success, subparser_unrecognized] = get_activated_subparser()->parse_known_args(subparser_tokens);
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
bool ArgumentParser::_parse_options(TokensSpan tokens) {
    for (auto const& [idx, token_pair] : tl::views::enumerate(tokens)) {
        auto const& token = token_pair.token;
        auto& parsed      = token_pair.parsed;

        if (!has_option_prefix(token) || tokens[idx].parsed) continue;

        // special token: if the token is two prefix chars, everything after it is positional
        if (token.size() == 2 && _pimpl->option_prefixes.find(token[0]) != std::string::npos && _pimpl->option_prefixes.find(token[1]) != std::string::npos) {
            parsed = true;
            return true;
        }

        // tries to match the token as a prefix to the full option name
        auto match = _match_option(token);
        if (auto p_arg_name = std::get_if<std::string>(&match)) {
            if (!_parse_one_option(_get_arg(*p_arg_name), tokens.subspan(idx + 1))) {
                return false;
            }
            parsed = true;
            continue;
        }
        // check if the option is ambiguous
        if (auto p_frequency = std::get_if<size_t>(&match)) {
            assert(p_frequency != nullptr);

            // check if the argument is a number
            if (dvlab::str::from_string<float>(token).has_value()) {
                continue;
            }

            // If the option is ambiguous, report the error and quit parsing
            if (*p_frequency > 0) {
                auto const matching_option_filter = [this, &token = token](std::string const& name) {
                    return has_option_prefix(name) && name.starts_with(token);
                };
                fmt::println(stderr, "Error: ambiguous option: \"{}\" could match {}",
                             token, fmt::join(_pimpl->arguments | std::views::keys | std::views::filter(matching_option_filter), ", "));
                return false;
            }
        }
        // else the option is unrecognized. Tries to match the token with a single-char option
        // for example, if the token is "-abc", then it will try to match "-a", "-b", and "-c",
        // among which only the last one can have an subsequent argument.

        auto const [single_char_options, remainder_token] = _explode_option(token);
        if (single_char_options.empty()) {
            return true;
        }

        for (auto const& [j, option] : tl::views::enumerate(single_char_options)) {
            auto& arg = _get_arg(option);
            if (j == single_char_options.size() - 1) {
                if (remainder_token.empty()) {
                    if (!_parse_one_option(arg, tokens.subspan(idx + 1))) {
                        return false;
                    }
                } else {
                    std::array<Token, 1> tmpToken{Token{remainder_token}};
                    if (!_parse_one_option(arg, TokensSpan{tmpToken})) {
                        return false;
                    }
                }
            } else {
                if (!_parse_one_option(arg, TokensSpan{})) {
                    return false;
                }
            }
        }

        parsed = true;
    }

    return _all_required_options_are_parsed();
}
std::pair<std::vector<std::string>, std::string> ArgumentParser::_explode_option(std::string_view token) const {
    std::vector<std::string> single_char_options;
    std::string remainder_token;
    for (auto const& [j, ch] : tl::views::enumerate(token.substr(1))) {
        std::string const single_char_option{token[0], ch};
        auto match = _match_option(single_char_option);
        if (auto p_arg_name = std::get_if<std::string>(&match)) {
            single_char_options.push_back(single_char_option);
            // if the option need at least one argument, then the remainder of the token is the argument
            if (_get_arg(*p_arg_name).get_nargs().lower > 0) {
                remainder_token = token.substr(j + 2);
                break;
            }
        }
        // if we encounter a single-char that does not match any option, then everything after it is the argument for the last option
        else if (std::get_if<size_t>(&match)) {
            remainder_token = token.substr(j + 2);
        }
    }
    return {single_char_options, remainder_token};
}

bool ArgumentParser::_parse_one_option(Argument& arg, TokensSpan tokens) {
    auto parse_range = arg.get_parse_range(tokens);
    if (arg.is_help_action()) {
        this->print_help();
        return false;  // break the parsing
    }

    if (arg.is_version_action()) {
        this->print_version();
        return false;  // break the parsing
    }

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
    return true;
}

/**
 * @brief Parse positional arguments, i.e., arguments that must appear in a specific order.
 *
 * @return true if succeeded
 * @return false if failed
 */
bool ArgumentParser::_parse_positional_arguments(TokensSpan tokens, std::vector<Token>& unrecognized) {
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
std::variant<std::string, size_t> ArgumentParser::_match_option(std::string_view token) const {
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
ArgumentParser SubParsers::add_parser(std::string_view name) {
    return add_parser(name, _pimpl->parent_parser._pimpl->config);
}

/**
 * @brief add a parser to the subparser group with a custom config.
 *
 * @param name
 * @param config
 * @return ArgumentParser
 */
ArgumentParser SubParsers::add_parser(std::string_view name, ArgumentParserConfig const& config) {
    if (_pimpl->subparsers.contains(name)) {
        fmt::println(stderr, "[ArgParse Error] Subparser \"{}\" already exists!!", name);
        throw std::runtime_error("subparser already exists");
    }
    _pimpl->subparsers.emplace(name, ArgumentParser{name, config});
    _pimpl->subparsers.at(std::string{name})._pimpl->parent_parser = &_pimpl->parent_parser;
    return _pimpl->subparsers.at(std::string{name});
}
/**
 * @brief
 *
 * @return SubParsers
 */
[[nodiscard]] SubParsers ArgumentParser::add_subparsers() {
    if (_pimpl->subparsers.has_value()) {
        fmt::println(stderr, "Error: an ArgumentParser can only have one set of subparsers!!");
        throw std::runtime_error("cannot have multiple subparsers");
    }
    _pimpl->subparsers = SubParsers{*this};
    return _pimpl->subparsers.value();
}

}  // namespace dvlab::argparse
