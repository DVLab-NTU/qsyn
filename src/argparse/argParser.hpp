/****************************************************************************
  FileName     [ apArgParser.hpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <concepts>
#include <unordered_map>
#include <variant>

#include "./argGroup.hpp"
#include "./argument.hpp"
#include "fmt/core.h"
#include "util/ordered_hashmap.hpp"
#include "util/trie.hpp"
#include "util/util.hpp"

namespace ArgParse {

struct ArgumentParserConfig {
    bool addHelpAction = true;
    bool addVersionAction = false;
    bool exitOnFailure = true;
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
        ordered_hashmap<std::string, ArgumentParser> subparsers;
        std::string help;
        bool required;
        bool parsed;
        ArgumentParserConfig parentConfig;
    };
    std::shared_ptr<SubParsersImpl> _pimpl;

public:
    SubParsers(ArgumentParserConfig const& parentConfig) : _pimpl{std::make_shared<SubParsersImpl>()} { _pimpl->parentConfig = parentConfig; }
    void setParsed(bool isParsed) { _pimpl->parsed = isParsed; }
    SubParsers required(bool isReq) {
        _pimpl->required = isReq;
        return *this;
    }
    SubParsers help(std::string const& help) {
        _pimpl->help = help;
        return *this;
    }

    ArgumentParser addParser(std::string const& name);
    ArgumentParser addParser(std::string const& name, ArgumentParserConfig const& config);

    size_t size() const noexcept { return _pimpl->subparsers.size(); }

    auto const& getSubParsers() const { return _pimpl->subparsers; }
    auto const& getHelp() const { return _pimpl->help; }

    bool isRequired() const { return _pimpl->required; }
    bool isParsed() const { return _pimpl->parsed; }
};

namespace detail {

std::string getSyntax(ArgumentParser parser, MutuallyExclusiveGroup const& group);
std::string styledOptionNameAndAliases(ArgumentParser parser, Argument const& arg);

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
                                             .addHelpAction = true,
                                             .addVersionAction = false,
                                             .exitOnFailure = true,
                                             .version = "",
                                         });

    template <typename T>
    T get(std::string const& name) const {
        return this->getArgument(name).get<T>();
    }

    ArgumentParser& name(std::string const& name);
    ArgumentParser& description(std::string const& help);
    ArgumentParser& numRequiredChars(size_t num);

    size_t numParsedArguments() const;

    // print functions

    void printTokens() const;
    void printArguments() const;
    void printUsage() const;
    void printSummary() const;
    void printHelp() const;
    void printVersion() const;

    // setters

    void setOptionPrefix(std::string const& prefix) { _pimpl->optionPrefix = prefix; }

    // getters and attributes

    std::string const& getName() const { return _pimpl->name; }
    std::string const& getDescription() const { return _pimpl->description; }
    size_t getNumRequiredChars() const { return _pimpl->numRequiredChars; }
    size_t getArgNumRequiredChars(std::string const& name) const;
    std::optional<SubParsers> const& getSubParsers() const { return _pimpl->subparsers; }
    bool parsed(std::string const& key) const { return this->getArgument(key).isParsed(); }
    bool hasOptionPrefix(std::string const& str) const { return str.find_first_of(_pimpl->optionPrefix) == 0UL; }
    bool hasOptionPrefix(Argument const& arg) const { return hasOptionPrefix(arg.getName()); }
    bool hasSubParsers() const { return _pimpl->subparsers.has_value(); }
    bool usedSubParser(std::string const& name) const { return _pimpl->subparsers.has_value() && _pimpl->activatedSubParser == name; }

    // action
    template <typename T>
    requires ValidArgumentType<T>
    ArgType<T>& addArgument(std::string const& name, std::convertible_to<std::string> auto... alias);

    [[nodiscard]] MutuallyExclusiveGroup addMutuallyExclusiveGroup();
    [[nodiscard]] SubParsers addSubParsers();

    bool parseArgs(std::string const& line);
    bool parseArgs(std::vector<std::string> const& tokens);
    bool parseArgs(TokensView);

    std::pair<bool, std::vector<Token>> parseKnownArgs(std::string const& line);
    std::pair<bool, std::vector<Token>> parseKnownArgs(std::vector<std::string> const& tokens);
    std::pair<bool, std::vector<Token>> parseKnownArgs(TokensView);

    bool analyzeOptions() const;

private:
    friend class Argument;
    friend std::string detail::getSyntax(ArgumentParser parser, MutuallyExclusiveGroup const& group);
    friend std::string detail::styledOptionNameAndAliases(ArgumentParser parser, Argument const& arg);
    struct ArgumentParserImpl {
        ordered_hashmap<std::string, Argument> arguments;
        std::unordered_map<std::string, std::string> aliasForwardMap;
        std::unordered_multimap<std::string, std::string> aliasReverseMap;
        std::string optionPrefix = "-";
        std::vector<Token> tokens;

        std::vector<MutuallyExclusiveGroup> mutuallyExclusiveGroups;
        std::optional<SubParsers> subparsers;
        std::optional<std::string> activatedSubParser;
        std::unordered_map<std::string, MutuallyExclusiveGroup> mutable conflictGroups;  // map an argument name to a mutually-exclusive group if it belongs to one.

        std::string name;
        std::string description;
        size_t numRequiredChars = 1;

        // members for analyzing parser options
        dvlab::utils::Trie mutable trie;
        bool mutable optionsAnalyzed = false;
        ArgumentParserConfig config;
    };

    std::shared_ptr<ArgumentParserImpl> _pimpl;

    // addArgument subroutines

    template <typename T>
    requires ValidArgumentType<T>
    ArgType<T>& addPositionalArgument(std::string const& name, std::convertible_to<std::string> auto... alias);

    template <typename T>
    requires ValidArgumentType<T>
    ArgType<T>& addOption(std::string const& name, std::convertible_to<std::string> auto... alias);

    // pretty printing helpers

    std::pair<bool, std::vector<Token>> parseKnownArgsImpl(TokensView);

    void setSubParser(std::string const& name) {
        _pimpl->activatedSubParser = name;
        _pimpl->subparsers->setParsed(true);
    }
    Argument const& getArgument(std::string const& name) const;
    Argument& getArgument(std::string const& name);
    bool hasArgument(std::string const& name) const { return _pimpl->arguments.contains(name) || _pimpl->aliasForwardMap.contains(name); }

    std::optional<ArgumentParser> getActivatedSubParser() const {
        if (!_pimpl->subparsers.has_value() || !_pimpl->activatedSubParser.has_value()) return std::nullopt;
        return _pimpl->subparsers->getSubParsers().at(*(_pimpl->activatedSubParser));
    }

    // parse subroutine
    std::string getActivatedSubParserName() const { return _pimpl->activatedSubParser.value_or(""); }
    bool tokenize(std::string const& line);
    bool parseOptions(TokensView, std::vector<Token>&);
    bool parsePositionalArguments(TokensView, std::vector<Token>&);
    void fillUnparsedArgumentsWithDefaults();

    // parseOptions subroutine

    std::variant<std::string, size_t> matchOption(std::string const& token) const;
    void printAmbiguousOptionErrorMsg(std::string const& token) const;
    bool allRequiredOptionsAreParsed() const;
    bool allRequiredMutexGroupsAreParsed() const;

    bool noConflictWithParsedArguments(Argument const&) const;

    // parsePositionalArguments subroutine

    bool allRequiredArgumentsAreParsed() const;

    template <typename T>
    static auto& getArgumentImpl(T& t, std::string const& name);
};

/**
 * @brief add an argument with the name to the MutuallyExclusiveGroup
 *
 * @tparam T the type of argument
 * @param name the name of the argument
 * @return ArgType<T>& a reference to the added argument
 */
template <typename T>
requires ValidArgumentType<T>
ArgType<T>& MutuallyExclusiveGroup::addArgument(std::string const& name, std::convertible_to<std::string> auto... alias) {
    ArgType<T>& returnRef = _pimpl->_parser->addArgument<T>(name, alias...);
    _pimpl->_arguments.insert(returnRef._name);
    return returnRef;
}

/**
 * @brief add an argument with the name. This function may exit if the there are duplicate names/aliases or the name is invalid
 *
 * @tparam T the type of argument
 * @param name the name of the argument
 * @return ArgType<T>&
 */
template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgumentParser::addArgument(std::string const& name, std::convertible_to<std::string> auto... alias) {
    if (name.empty()) {
        fmt::println(stderr, "[ArgParse] Error: argument name cannot be an empty string!!");
        exit(1);
    }

    if (_pimpl->arguments.contains(name) || _pimpl->aliasForwardMap.contains(name)) {
        fmt::println(stderr, "[ArgParse] Error: duplicate argument name \"{}\"!!", name);
        exit(1);
    }

    _pimpl->optionsAnalyzed = false;

    return hasOptionPrefix(name) ? addOption<T>(name, alias...) : addPositionalArgument<T>(name, alias...);
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
requires ValidArgumentType<T>
ArgType<T>& ArgumentParser::addPositionalArgument(std::string const& name, std::convertible_to<std::string> auto... alias) {
    assert(!hasOptionPrefix(name));

    if ((0 + ... + sizeof(alias)) > 0 /* has aliases */) {
        fmt::println(stderr, "[ArgParse] Error: positional argument \"{}\" cannot have alias!!", name);
        exit(1);
    }

    _pimpl->arguments.emplace(name, Argument(name, T{}));

    return _pimpl->arguments.at(name).toUnderlyingType<T>()  //
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
requires ValidArgumentType<T>
ArgType<T>& ArgumentParser::addOption(std::string const& name, std::convertible_to<std::string> auto... alias) {
    assert(hasOptionPrefix(name));

    // checking if every alias is valid
    if (!(std::invoke([&]() {  // NOTE : don't extract this lambda out. It will fail for some reason.
              if (std::string_view{alias}.empty()) {
                  fmt::println(stderr, "[ArgParse] Error: argument alias cannot be an empty string!!");
                  return false;
              }
              // alias should start with option prefix
              if (!hasOptionPrefix(alias)) {
                  fmt::println(stderr, "[ArgParse] Error: alias \"{}\" of argument \"{}\" must start with \"{}\"!!", alias, name, _pimpl->optionPrefix);
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
              auto [_, inserted] = _pimpl->aliasForwardMap.emplace(alias, name);
              _pimpl->aliasReverseMap.emplace(name, alias);
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

    return _pimpl->arguments.at(name).toUnderlyingType<T>()  //
        .metavar(toUpperString(name.substr(name.find_first_not_of(_pimpl->optionPrefix))));
}

template <typename T>
auto& ArgumentParser::getArgumentImpl(T& t, std::string const& name) {
    if (t._pimpl->subparsers.has_value() && t._pimpl->subparsers->isParsed()) {
        if (t.getActivatedSubParser()->hasArgument(name)) {
            return t.getActivatedSubParser()->getArgument(name);
        }
    }
    if (t._pimpl->aliasForwardMap.contains(name)) {
        return t._pimpl->arguments.at(t._pimpl->aliasForwardMap.at(name));
    }
    if (t._pimpl->arguments.contains(name)) {
        return t._pimpl->arguments.at(name);
    }

    fmt::println(stderr, "[ArgParse] Error: argument name \"{}\" does not exist for command \"{}\"",
                 name,
                 t.getName());
    exit(1);
}

}  // namespace ArgParse