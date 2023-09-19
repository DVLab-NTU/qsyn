/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define argparse::ArgumentParser and SubParsers ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <concepts>
#include <unordered_map>
#include <variant>

#include "./arg_group.hpp"
#include "./argument.hpp"
#include "fmt/core.h"
#include "util/ordered_hashmap.hpp"
#include "util/trie.hpp"
#include "util/util.hpp"

namespace dvlab::argparse {

struct ArgumentParserConfig {
    bool add_help_action     = true;
    bool add_version_action  = false;
    bool exitOnFailure       = true;
    std::string_view version = "";
};

/**
 * @brief A view for adding subparsers.
 *        All copies of this class represents the same underlying group of subparsers.
 *
 */
class SubParsers {
private:
    struct SubParsersImpl {
        dvlab::utils::ordered_hashmap<std::string, ArgumentParser> subparsers;
        std::string help;
        bool required = false;
        bool parsed   = false;
        ArgumentParserConfig parent_config;
    };
    std::shared_ptr<SubParsersImpl> _pimpl;

public:
    SubParsers(ArgumentParserConfig const& parent_config) : _pimpl{std::make_shared<SubParsersImpl>()} { _pimpl->parent_config = parent_config; }
    void set_parsed(bool is_parsed) { _pimpl->parsed = is_parsed; }
    SubParsers required(bool is_req) {
        _pimpl->required = is_req;
        return *this;
    }
    SubParsers help(std::string const& help) {
        _pimpl->help = help;
        return *this;
    }

    ArgumentParser add_parser(std::string const& name);
    ArgumentParser add_parser(std::string const& name, ArgumentParserConfig const& config);

    size_t size() const noexcept { return _pimpl->subparsers.size(); }

    auto const& get_subparsers() const { return _pimpl->subparsers; }
    auto const& get_help() const { return _pimpl->help; }

    bool is_required() const { return _pimpl->required; }
    bool is_parsed() const { return _pimpl->parsed; }
};

namespace detail {

std::string get_syntax(ArgumentParser parser, MutuallyExclusiveGroup const& group);
std::string styled_option_name_and_aliases(ArgumentParser parser, Argument const& arg);

}  // namespace detail

/**
 * @brief A view for argument parsers.
 *        All copies of this class represents the same underlying parsers.
 *
 */
class ArgumentParser {
    friend class Formatter;

public:
    ArgumentParser() : _pimpl{std::make_shared<ArgumentParserImpl>()} {}
    ArgumentParser(std::string const& n, ArgumentParserConfig config = {
                                             .add_help_action    = true,
                                             .add_version_action = false,
                                             .exitOnFailure      = true,
                                             .version            = "",
                                         });

    template <typename T>
    T get(std::string const& name) const {
        return this->_get_arg(name).get<T>();
    }

    ArgumentParser& name(std::string const& name);
    ArgumentParser& description(std::string const& help);
    ArgumentParser& num_required_chars(size_t num);
    ArgumentParser& option_prefix(std::string const& prefix);

    size_t num_parsed_args() const;

    // print functions

    void print_tokens() const;
    void print_arguments() const;
    void print_usage() const;
    void print_summary() const;
    void print_help() const;
    void print_version() const;

    // setters

    // getters and attributes

    std::string const& get_name() const { return _pimpl->name; }
    std::string const& get_description() const { return _pimpl->description; }
    size_t get_num_required_chars() const { return _pimpl->num_required_chars; }
    size_t get_arg_num_required_chars(std::string const& name) const;
    std::optional<SubParsers> const& get_subparsers() const { return _pimpl->subparsers; }
    bool parsed(std::string const& key) const { return this->_get_arg(key).is_parsed(); }
    bool has_option_prefix(std::string const& str) const { return str.find_first_of(_pimpl->option_prefixes) == 0UL; }
    bool has_subparsers() const { return _pimpl->subparsers.has_value(); }
    bool used_subparser(std::string const& name) const { return _pimpl->subparsers.has_value() && _pimpl->activated_subparser == name; }

    // action
    template <typename T>
    requires valid_argument_type<T>
    ArgType<T>& add_argument(std::string const& name, std::convertible_to<std::string> auto... alias);

    [[nodiscard]] MutuallyExclusiveGroup add_mutually_exclusive_group();
    [[nodiscard]] SubParsers add_subparsers();

    bool parse_args(std::string const& line);
    bool parse_args(std::vector<std::string> const& tokens);
    bool parse_args(TokensView);

    std::pair<bool, std::vector<Token>> parse_known_args(std::string const& line);
    std::pair<bool, std::vector<Token>> parse_known_args(std::vector<std::string> const& tokens);
    std::pair<bool, std::vector<Token>> parse_known_args(TokensView);

    bool analyze_options() const;

private:
    friend class Argument;
    friend std::string detail::get_syntax(ArgumentParser parser, MutuallyExclusiveGroup const& group);
    friend std::string detail::styled_option_name_and_aliases(ArgumentParser parser, Argument const& arg);
    struct ArgumentParserImpl {
        dvlab::utils::ordered_hashmap<std::string, Argument> arguments;
        std::unordered_map<std::string, std::string> alias_forward_map;
        std::unordered_multimap<std::string, std::string> alias_reverse_map;
        std::string option_prefixes = "-";
        std::vector<Token> tokens;

        std::vector<MutuallyExclusiveGroup> mutually_exclusive_groups;
        std::optional<SubParsers> subparsers;
        std::optional<std::string> activated_subparser;
        std::unordered_map<std::string, MutuallyExclusiveGroup> mutable conflict_groups;  // map an argument name to a mutually-exclusive group if it belongs to one.

        std::string name;
        std::string description;
        size_t num_required_chars = 1;

        // members for analyzing parser options
        dvlab::utils::Trie mutable identifiers;
        bool mutable options_analyzed = false;
        ArgumentParserConfig config;
    };

    std::shared_ptr<ArgumentParserImpl> _pimpl;

    // addArgument subroutines

    template <typename T>
    requires valid_argument_type<T>
    ArgType<T>& _add_positional_argument(std::string const& name, std::convertible_to<std::string> auto... alias);

    template <typename T>
    requires valid_argument_type<T>
    ArgType<T>& _add_option(std::string const& name, std::convertible_to<std::string> auto... alias);

    // pretty printing helpers

    std::pair<bool, std::vector<Token>> _parse_known_args_impl(TokensView);

    void _activate_subparser(std::string const& name) {
        _pimpl->activated_subparser = name;
        _pimpl->subparsers->set_parsed(true);
    }
    Argument const& _get_arg(std::string const& name) const;
    Argument& _get_arg(std::string const& name);
    bool _has_arg(std::string const& name) const { return _pimpl->arguments.contains(name) || _pimpl->alias_forward_map.contains(name); }

    std::optional<ArgumentParser> _get_activated_subparser() const {
        if (!_pimpl->subparsers.has_value() || !_pimpl->activated_subparser.has_value()) return std::nullopt;
        return _pimpl->subparsers->get_subparsers().at(*(_pimpl->activated_subparser));
    }

    // parse subroutine
    std::string _get_activated_subparser_name() const { return _pimpl->activated_subparser.value_or(""); }
    bool _tokenize(std::string const& line);
    bool _parse_options(TokensView tokens);
    bool _parse_positional_arguments(TokensView tokens, std::vector<Token>& unrecognized);
    void _fill_unparsed_args_with_defaults();

    // parseOptions subroutine

    std::variant<std::string, size_t> _match_option(std::string const& token) const;
    bool _all_required_options_are_parsed() const;
    bool _all_required_mutex_groups_are_parsed() const;

    bool _no_conflict_with_parsed_arguments(Argument const&) const;

    // parsePositionalArguments subroutine

    bool _all_required_args_are_parsed() const;

    template <typename T>
    static auto& _get_arg_impl(T& t, std::string const& name);
};

/**
 * @brief add an argument with the name to the MutuallyExclusiveGroup
 *
 * @tparam T the type of argument
 * @param name the name of the argument
 * @return ArgType<T>& a reference to the added argument
 */
template <typename T>
requires valid_argument_type<T>
ArgType<T>& MutuallyExclusiveGroup::add_argument(std::string const& name, std::convertible_to<std::string> auto... alias) {
    ArgType<T>& return_ref = _pimpl->_parser->add_argument<T>(name, alias...);
    _pimpl->_arguments.insert(return_ref._name);
    return return_ref;
}

/**
 * @brief add an argument with the name. This function may exit if the there are duplicate names/aliases or the name is invalid
 *
 * @tparam T the type of argument
 * @param name the name of the argument
 * @return ArgType<T>&
 */
template <typename T>
requires valid_argument_type<T>
ArgType<T>& ArgumentParser::add_argument(std::string const& name, std::convertible_to<std::string> auto... alias) {
    if (name.empty()) {
        fmt::println(stderr, "[ArgParse] Error: argument name cannot be an empty string!!");
        exit(1);
    }

    if (_pimpl->arguments.contains(name) || _pimpl->alias_forward_map.contains(name)) {
        fmt::println(stderr, "[ArgParse] Error: duplicate argument name \"{}\"!!", name);
        exit(1);
    }

    _pimpl->options_analyzed = false;

    return has_option_prefix(name) ? _add_option<T>(name, alias...) : _add_positional_argument<T>(name, alias...);
}

/**
 * @brief add a positional argument with the name. This function should only be called by addArgument
 *
 * @tparam T
 * @param name
 * @param alias
 * @return requires&
 */
template <typename T>
requires valid_argument_type<T>
ArgType<T>& ArgumentParser::_add_positional_argument(std::string const& name, std::convertible_to<std::string> auto... alias) {
    assert(!has_option_prefix(name));

    if ((0 + ... + sizeof(alias)) > 0 /* has aliases */) {
        fmt::println(stderr, "[ArgParse] Error: positional argument \"{}\" cannot have alias!!", name);
        exit(1);
    }

    _pimpl->arguments.emplace(name, Argument(name, T{}));

    return get_underlying_type<T>(_pimpl->arguments.at(name))  //
        .required(true)
        .metavar(name);
}

/**
 * @brief add an option with the name. This function should only be called by addArgument
 *
 * @tparam T
 * @param name
 * @param alias
 * @return requires&
 */
template <typename T>
requires valid_argument_type<T>
ArgType<T>& ArgumentParser::_add_option(std::string const& name, std::convertible_to<std::string> auto... alias) {
    assert(has_option_prefix(name));

    // checking if every alias is valid
    if (!(std::invoke([&]() {  // NOTE : don't extract this lambda out. It will fail for some reason.
              if (std::string_view{alias}.empty()) {
                  fmt::println(stderr, "[ArgParse] Error: argument alias cannot be an empty string!!");
                  return false;
              }
              // alias should start with option prefix
              if (!has_option_prefix(alias)) {
                  fmt::println(stderr, "[ArgParse] Error: alias \"{}\" of argument \"{}\" must start with \"{}\"!!", alias, name, _pimpl->option_prefixes);
                  return false;
              }
              // alias should not be the same as the name
              if (name == alias) {
                  fmt::println(stderr, "[ArgParse] Error: alias \"{}\" of argument \"{}\" cannot be the same as the name!!", alias, name);
                  return false;
              }
              // alias should not clash with other arguments
              if (_pimpl->arguments.contains(alias)) {
                  fmt::println(stderr, "[ArgParse] Error: argument alias \"{}\" conflicts with other argument name \"{}\"!!", alias, name);
                  return false;
              }
              auto [_, inserted] = _pimpl->alias_forward_map.emplace(alias, name);
              _pimpl->alias_reverse_map.emplace(name, alias);
              // alias should not clash with other aliases
              if (!inserted) {
                  fmt::println(stderr, "[ArgParse] Error: duplicate argument alias \"{}\"!!", alias);
                  return false;
              }
              return true;
          }) &&
          ...)) {
        exit(1);
    }

    _pimpl->arguments.emplace(name, Argument(name, T{}));

    return get_underlying_type<T>(_pimpl->arguments.at(name))  //
        .metavar(dvlab::str::toupper_string(name.substr(name.find_first_not_of(_pimpl->option_prefixes))));
}

template <typename T>
auto& ArgumentParser::_get_arg_impl(T& t, std::string const& name) {
    if (t._pimpl->subparsers.has_value() && t._pimpl->subparsers->is_parsed()) {
        if (t._get_activated_subparser()->_has_arg(name)) {
            return t._get_activated_subparser()->_get_arg(name);
        }
    }
    if (t._pimpl->alias_forward_map.contains(name)) {
        return t._pimpl->arguments.at(t._pimpl->alias_forward_map.at(name));
    }
    if (t._pimpl->arguments.contains(name)) {
        return t._pimpl->arguments.at(name);
    }

    fmt::println(stderr, "[ArgParse] Error: argument name \"{}\" does not exist for command \"{}\"",
                 name,
                 t.get_name());
    exit(1);
}

}  // namespace dvlab::argparse