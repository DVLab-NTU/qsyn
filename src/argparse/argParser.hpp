/****************************************************************************
  FileName     [ apArgParser.hpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <variant>

#include "./argGroup.hpp"
#include "./argument.hpp"
#include "./formatter.hpp"
#include "util/ordered_hashmap.hpp"
#include "util/trie.hpp"
#include "util/util.hpp"

namespace ArgParse {

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
    };
    std::shared_ptr<SubParsersImpl> _pimpl;

public:
    SubParsers() : _pimpl{std::make_shared<SubParsersImpl>()} {}
    void setParsed(bool isParsed) { _pimpl->parsed = isParsed; }
    SubParsers required(bool isReq) {
        _pimpl->required = isReq;
        return *this;
    }
    SubParsers help(std::string const& help) {
        _pimpl->help = help;
        return *this;
    }

    ArgumentParser addParser(std::string const& n);

    size_t size() const noexcept { return _pimpl->subparsers.size(); }

    auto const& getSubParsers() const { return _pimpl->subparsers; }
    auto const& getHelp() const { return _pimpl->help; }

    bool isRequired() const { return _pimpl->required; }
    bool isParsed() const { return _pimpl->parsed; }
};

/**
 * @brief A view for argument parsers.
 *        All copies of this class represents the same underlying parsers.
 *
 */
class ArgumentParser {
    friend class Formatter;

public:
    ArgumentParser() : _pimpl{std::make_shared<ArgumentParserImpl>()} {}
    ArgumentParser(std::string const& n) : ArgumentParser() {
        this->name(n);
    }

    Argument const& operator[](std::string const& name) const;

    template <typename T>
    T get(std::string const& name) const {
        return (*this)[name].get<T>();
    }

    ArgumentParser& name(std::string const& name);
    ArgumentParser& help(std::string const& help);

    size_t numParsedArguments() const {
        return std::count_if(
            _pimpl->arguments.begin(), _pimpl->arguments.end(),
            [](auto& pr) {
                return pr.second.isParsed();
            });
    }

    // print functions

    void printTokens() const;
    void printArguments() const;
    void printUsage() const { formatter.printUsage(*this); }
    void printSummary() const { formatter.printSummary(*this); }
    void printHelp() const { formatter.printHelp(*this); }

    // setters

    void setOptionPrefix(std::string const& prefix) { _pimpl->optionPrefix = prefix; }

    // getters and attributes

    std::string const& getName() const { return _pimpl->name; }
    std::string const& getHelp() const { return _pimpl->help; }
    size_t getNumRequiredChars() const { return _pimpl->numRequiredChars; }
    bool parsed(std::string const& key) const { return (*this)[toLowerString(key)].isParsed(); }
    bool hasOptionPrefix(std::string const& str) const { return str.find_first_of(_pimpl->optionPrefix) == 0UL; }
    bool hasOptionPrefix(Argument const& arg) const { return hasOptionPrefix(arg.getName()); }
    bool hasSubParsers() const { return _pimpl->subparsers.has_value(); }
    bool usedSubParser(std::string const& name) const { return _pimpl->subparsers.has_value() && _pimpl->activatedSubParser == name; }

    // action

    template <typename T>
    requires ValidArgumentType<T>
    ArgType<T>& addArgument(std::string const& name);

    MutuallyExclusiveGroup addMutuallyExclusiveGroup();
    SubParsers addSubParsers();

    /**
     * @brief tokenize the line and parse the arguments
     *
     * @param line
     * @return true
     * @return false
     */
    bool parseArgs(std::string const& line) { return tokenize(line) && parseArgs(_pimpl->tokens); }
    bool parseArgs(std::vector<std::string> const& tokens) {
        auto tmp = std::vector<Token>{tokens.begin(), tokens.end()};
        return parseArgs(tmp);
    }
    bool parseArgs(TokensView);

    /**
     * @brief tokenize the line and parse the arguments known by the parser
     *
     * @param line
     * @return std::pair<bool, std::vector<Token>>, where
     *         the first return value specifies whether the parse has succeeded, and
     *         the second one specifies the unrecognized tokens
     */
    std::pair<bool, std::vector<Token>> parseKnownArgs(std::string const& line) {
        if (!tokenize(line)) return {false, {}};
        return parseKnownArgs(_pimpl->tokens);
    }
    std::pair<bool, std::vector<Token>> parseKnownArgs(std::vector<std::string> const& tokens) {
        auto tmp = std::vector<Token>{tokens.begin(), tokens.end()};
        return parseKnownArgs(tmp);
    }
    std::pair<bool, std::vector<Token>> parseKnownArgs(TokensView);
    bool analyzeOptions() const;

private:
    friend class Argument;
    struct ArgumentParserImpl {
        ArgumentParserImpl() {}
        ordered_hashmap<std::string, Argument> arguments;
        std::string optionPrefix = "-";
        std::vector<Token> tokens;

        std::vector<MutuallyExclusiveGroup> mutuallyExclusiveGroups;
        std::optional<SubParsers> subparsers;
        std::optional<std::string> activatedSubParser;
        std::unordered_map<std::string, MutuallyExclusiveGroup> mutable conflictGroups;  // map an argument name to a mutually-exclusive group if it belongs to one.

        std::string name;
        std::string help;
        size_t numRequiredChars;

        // members for analyzing parser options
        dvlab::utils::Trie mutable trie;
        bool mutable optionsAnalyzed = false;
    };

    static Formatter formatter;

    std::shared_ptr<ArgumentParserImpl> _pimpl;

    // pretty printing helpers

    void setNumRequiredChars(size_t num) { _pimpl->numRequiredChars = num; }
    void setSubParser(std::string const& name) {
        _pimpl->activatedSubParser = name;
        _pimpl->subparsers->setParsed(true);
    }
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
ArgType<T>& MutuallyExclusiveGroup::addArgument(std::string const& name) {
    ArgType<T>& returnRef = _pimpl->_parser.addArgument<T>(name);
    _pimpl->_arguments.insert(returnRef._name);
    return returnRef;
}

/**
 * @brief add an argument with the name.
 *
 * @tparam T the type of argument
 * @param name the name of the argument
 * @return ArgType<T>&
 */
template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgumentParser::addArgument(std::string const& name) {
    auto key = toLowerString(name);
    if (_pimpl->arguments.contains(key)) {
        fmt::println(stderr, "[ArgParse] Error: Duplicate argument name \"{}\"!!", name);
    } else {
        _pimpl->arguments.emplace(key, Argument(key, T{}));
    }

    auto& returnRef = _pimpl->arguments.at(key).toUnderlyingType<T>();

    if (!hasOptionPrefix(key)) {
        returnRef.required(true).metavar(key);
    } else {
        returnRef.metavar(toUpperString(key.substr(key.find_first_not_of(_pimpl->optionPrefix))));
    }

    _pimpl->optionsAnalyzed = false;

    return returnRef;
}

}  // namespace ArgParse