/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define argparse::ArgumentParser and SubParsers ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <concepts>
#include <functional>
#include <unordered_map>
#include <variant>

#include "./arg_group.hpp"
#include "./argument.hpp"
#include "fmt/core.h"
#include "util/dvlab_string.hpp"
#include "util/ordered_hashmap.hpp"
#include "util/trie.hpp"

namespace dvlab::argparse {

class ArgumentParser;

struct ArgumentParserConfig {
    bool add_help_action     = true;
    bool add_version_action  = false;
    bool exit_on_failure     = true;
    std::string_view version = "";
};

/**
 * @brief A view for adding subparsers.
 *        All copies of this class represents the same underlying group of subparsers.
 *
 */
class SubParsers {
private:
    struct SubParsersImpl;
    using MapType = dvlab::utils::ordered_hashmap<std::string, ArgumentParser, detail::heterogeneous_string_hash, std::equal_to<>>;
    std::shared_ptr<SubParsersImpl> _pimpl;

public:
    SubParsers(ArgumentParser& parent_parser, std::string_view dest);
    SubParsers required(bool is_req);
    SubParsers help(std::string_view help);

    ArgumentParser add_parser(std::string_view name);
    ArgumentParser add_parser(std::string_view name, ArgumentParserConfig const& config);

    size_t size() const noexcept;

    SubParsers::MapType const& get_subparsers() const;
    std::string const& get_help() const;
    std::string const& get_dest() const;

    bool is_required() const noexcept;
};

namespace detail {

std::string get_syntax(ArgumentParser parser, MutuallyExclusiveGroup const& group);
std::string styled_option_name_and_aliases(ArgumentParser parser, Argument const& arg);
std::string styled_parser_name(ArgumentParser const& parser);
std::string styled_parser_name_trace(ArgumentParser const& parser);

}  // namespace detail

/**
 * @brief A view for argument parsers.
 *        All copies of this class represents the same underlying parsers.
 *
 */
class ArgumentParser {
public:
    ArgumentParser() : _pimpl{std::make_shared<ArgumentParserImpl>()} {}
    ArgumentParser(std::string_view n, ArgumentParserConfig config = {
                                           .add_help_action    = true,
                                           .add_version_action = false,
                                           .exit_on_failure    = true,
                                           .version            = "",
                                       });

    template <typename T>
    T get(std::string_view name) const {
        if constexpr (std::same_as<T, std::string>) {
            if (auto ret = get_dest(name)) {
                return ret.value();
            }
        }
        auto* arg = this->_get_arg(name);
        if (arg == nullptr) {
            fmt::println(stderr,
                         "[ArgParse] Error: argument name \"{}\" does not exist for command \"{}\"",
                         name,
                         get_name());
            throw std::runtime_error("argument name does not exist");
        }
        return this->_get_arg(name)->get<T>();
    }

    template <typename T>
    std::optional<T> get_if(std::string_view name) const {
        if constexpr (std::same_as<T, std::string>) {
            if (auto ret = get_dest(name)) {
                return ret;
            }
        }
        auto* arg = this->_get_arg(name);
        if (arg == nullptr) {
            return std::nullopt;
        }

        return this->_get_arg(name)->get_if<T>();
    }

    std::optional<std::string> get_dest(std::string_view name) const {
        if (get_activated_subparser().has_value()) {
            if (auto ret = get_activated_subparser()->get_dest(name)) {
                return ret;
            }
        }
        if (_pimpl->dests.contains(std::string{name})) {
            return _pimpl->dests.at(std::string{name});
        }
        return std::nullopt;
    }

    ArgumentParser& name(std::string_view name);
    ArgumentParser& description(std::string_view help);
    ArgumentParser& num_required_chars(size_t num);
    ArgumentParser& option_prefix(std::string_view prefix);

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
    size_t get_arg_num_required_chars(std::string_view name) const;
    std::optional<SubParsers> const& get_subparsers() const { return _pimpl->subparsers; }
    std::optional<ArgumentParser> get_activated_subparser() const;
    bool parsed(std::string_view key) const { return this->_get_arg(key)->is_parsed(); }
    bool has_option_prefix(std::string_view str) const { return str.find_first_of(_pimpl->option_prefixes) == 0UL; }
    bool has_subparsers() const { return _pimpl->subparsers.has_value(); }

    // action
    template <typename T>
    requires valid_argument_type<T>
    ArgType<T>& add_argument(std::string_view name, std::convertible_to<std::string> auto... alias);

    [[nodiscard]] MutuallyExclusiveGroup add_mutually_exclusive_group();
    [[nodiscard]] SubParsers add_subparsers(std::string_view dest);

    bool parse_args(std::vector<std::string> const& tokens);
    bool parse_args(TokensSpan);

    std::pair<bool, std::vector<Token>> parse_known_args(std::vector<std::string> const& tokens);
    std::pair<bool, std::vector<Token>> parse_known_args(TokensSpan);

    bool analyze_options() const;

private:
    friend class Argument;
    friend class SubParsers;
    friend std::string detail::get_syntax(ArgumentParser parser, MutuallyExclusiveGroup const& group);
    friend std::string detail::styled_option_name_and_aliases(ArgumentParser parser, Argument const& arg);
    friend std::string detail::styled_parser_name(ArgumentParser const& parser);
    friend std::string detail::styled_parser_name_trace(ArgumentParser const& parser);
    struct ArgumentParserImpl {
        dvlab::utils::ordered_hashmap<std::string, Argument, detail::heterogeneous_string_hash, std::equal_to<>> arguments;
        std::unordered_map<std::string, std::string, detail::heterogeneous_string_hash, std::equal_to<>> alias_forward_map;
        std::unordered_multimap<std::string, std::string, detail::heterogeneous_string_hash, std::equal_to<>> alias_reverse_map;
        std::string option_prefixes = "-";
        std::vector<Token> tokens;
        std::unordered_map<std::string, std::string> dests;

        ArgumentParser* parent_parser = nullptr;

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
    ArgType<T>& _add_positional_argument(std::string_view name, std::convertible_to<std::string> auto... alias);

    template <typename T>
    requires valid_argument_type<T>
    ArgType<T>& _add_option(std::string_view name, std::convertible_to<std::string> auto... alias);

    // pretty printing helpers

    std::pair<bool, std::vector<Token>> _parse_known_args_impl(TokensSpan);

    void _activate_subparser(std::string_view name) {
        _pimpl->activated_subparser = name;
    }
    Argument* _get_arg(std::string_view name) const;
    Argument* _get_arg(std::string_view name);
    bool _has_arg(std::string_view name) const;

    // parse subroutine
    std::string _get_activated_subparser_name() const { return _pimpl->activated_subparser.value_or(""); }
    bool _parse_options(TokensSpan tokens);
    bool _parse_one_option(Argument& arg, TokensSpan tokens);
    std::pair<std::vector<std::string>, std::string> _explode_option(std::string_view token) const;
    bool _parse_positional_arguments(TokensSpan tokens, std::vector<Token>& unrecognized);
    void _fill_unparsed_args_with_defaults();

    // parseOptions subroutine

    std::variant<std::string, size_t> _match_option(std::string_view token) const;
    bool _all_required_options_are_parsed() const;
    bool _all_required_mutex_groups_are_parsed() const;

    bool _no_conflict_with_parsed_arguments(Argument const&) const;

    // parsePositionalArguments subroutine

    bool _all_required_args_are_parsed() const;

    template <typename T>
    static auto* _get_arg_impl(T&& t, std::string_view name);
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
ArgType<T>& MutuallyExclusiveGroup::add_argument(std::string_view name, std::convertible_to<std::string> auto... alias) {
    ArgType<T>& return_ref = _pimpl->_parser->add_argument<T>(name, alias...);
    _pimpl->_arg_names.emplace_back(name);
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
ArgType<T>& ArgumentParser::add_argument(std::string_view name, std::convertible_to<std::string> auto... alias) {
    if (name.empty()) {
        fmt::println(stderr, "[ArgParse] Error: argument name cannot be an empty string!!");
        throw std::runtime_error("argument name cannot be an empty string");
    }

    if (_pimpl->arguments.contains(name) || _pimpl->alias_forward_map.contains(name)) {
        fmt::println(stderr, "[ArgParse] Error: duplicate argument name \"{}\"!!", name);
        throw std::runtime_error("cannot have duplicate argument name");
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
ArgType<T>& ArgumentParser::_add_positional_argument(std::string_view name, std::convertible_to<std::string> auto... alias) {
    assert(!has_option_prefix(name));

    if ((0 + ... + sizeof(alias)) > 0 /* has aliases */) {
        fmt::println(stderr, "[ArgParse] Error: positional argument \"{}\" cannot have alias!!", name);
        throw std::runtime_error("positional argument cannot have alias");
    }

    _pimpl->arguments.emplace(std::string{name}, Argument(name, T{}));

    return get_underlying_type<T>(_pimpl->arguments.at(std::string{name}))  //
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
ArgType<T>& ArgumentParser::_add_option(std::string_view name, std::convertible_to<std::string> auto... alias) {
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
        throw std::runtime_error("invalid argument alias");
    }

    _pimpl->arguments.emplace(std::string{name}, Argument(name, T{}));

    return get_underlying_type<T>(_pimpl->arguments.at(std::string{name}))
        .metavar(dvlab::str::toupper_string(name.substr(name.find_first_not_of(_pimpl->option_prefixes))));
}
template <typename T>
auto* ArgumentParser::_get_arg_impl(T&& t, std::string_view name) {
    if (t._pimpl->subparsers.has_value() && t._pimpl->activated_subparser.has_value()) {
        if (t.get_activated_subparser()->_has_arg(name)) {
            return t.get_activated_subparser()->_get_arg(name);
        }
    }
    if (t._pimpl->alias_forward_map.contains(name)) {
        return &t._pimpl->arguments.at(t._pimpl->alias_forward_map.at(std::string{name}));
    }
    if (t._pimpl->arguments.contains(name)) {
        return &t._pimpl->arguments.at(std::string{name});
    }

    return (Argument*){nullptr};
}

}  // namespace dvlab::argparse
