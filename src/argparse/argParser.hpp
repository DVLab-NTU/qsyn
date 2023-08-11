/****************************************************************************
  FileName     [ apArgParser.tpp ]
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
        std::string activatedSubparser;
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

    Argument& operator[](std::string const& name);
    Argument const& operator[](std::string const& name) const;

    template <typename T>
    T get(std::string const& name) const {
        auto lower = toLowerString(name);
        if (_pimpl->activatedSubParser.size()) {
            auto& subparser = _pimpl->subparsers->getSubParsers().at(_pimpl->activatedSubParser);
            if (subparser._pimpl->arguments.contains(lower)) {
                return subparser._pimpl->arguments.at(lower).get<T>();
            }
        }
        if (_pimpl->arguments.contains(lower)) {
            return _pimpl->arguments.at(lower).get<T>();
        }
        std::cerr << "[ArgParse error] Argument name \"" << name
                  << "\" does not exist for command \""
                  << formatter.styledCmdName(getName(), getNumRequiredChars()) << "\"\n";
        throw std::out_of_range{"Trying to access non-existent arguments"};
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
    bool hasOptionPrefix(std::string const& str) const { return str.find_first_of(_pimpl->optionPrefix) == 0UL; }
    bool hasOptionPrefix(Argument const& arg) const { return hasOptionPrefix(arg.getName()); }
    bool hasSubParsers() const { return _pimpl->subparsers.has_value(); }
    bool usedSubParser(std::string const& name) const { return _pimpl->subparsers.has_value() && _pimpl->activatedSubParser == name; }
    std::string const& getActivatedSubParserName() const { return _pimpl->activatedSubParser; }

    // action

    template <typename T>
    requires ValidArgumentType<T>
    ArgType<T>& addArgument(std::string const& name);

    ArgumentGroup addMutuallyExclusiveGroup();
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
    struct ArgumentParserImpl {
        ArgumentParserImpl() : optionPrefix("-"), optionsAnalyzed(false) {}
        ordered_hashmap<std::string, Argument> arguments;
        std::string optionPrefix;
        std::vector<Token> tokens;

        std::vector<ArgumentGroup> mutuallyExclusiveGroups;
        std::optional<SubParsers> subparsers;
        std::string activatedSubParser;
        std::unordered_map<std::string, ArgumentGroup> mutable conflictGroups;  // map an argument name to a mutually-exclusive group if it belongs to one.

        std::string name;
        std::string help;
        size_t numRequiredChars;

        // members for analyzing parser options
        dvlab_utils::Trie mutable trie;
        bool mutable optionsAnalyzed;
    };

    static Formatter formatter;

    std::shared_ptr<ArgumentParserImpl> _pimpl;

    template <typename ArgT>
    static decltype(auto)
    operator_bracket_impl(ArgT&& t, std::string const& name);

    // addArgument error printing

    void printDuplicateArgNameErrorMsg(std::string const& name) const;

    // pretty printing helpers

    void setNumRequiredChars(size_t num) { _pimpl->numRequiredChars = num; }
    void setSubParser(std::string const& name) {
        _pimpl->activatedSubParser = name;
        _pimpl->subparsers->setParsed(true);
    }
    ArgumentParser getActivatedSubParser() const { return _pimpl->subparsers->getSubParsers().at(_pimpl->activatedSubParser); }

    // parse subroutine
    bool tokenize(std::string const& line);
    bool parseOptions(TokensView, std::vector<Token>&);
    bool parsePositionalArguments(TokensView, std::vector<Token>&);
    void fillUnparsedArgumentsWithDefaults();

    // parseOptions subroutine

    std::variant<std::string, size_t> matchOption(std::string const& token) const;
    void printAmbiguousOptionErrorMsg(std::string const& token) const;
    bool allRequiredOptionsAreParsed() const;
    bool allRequiredMutexGroupsAreParsed() const;

    bool conflictWithParsedArguments(Argument const&) const;

    // parsePositionalArguments subroutine

    bool allTokensAreParsed(TokensView) const;
    bool allRequiredArgumentsAreParsed() const;
    void printRequiredArgumentsMissingErrorMsg() const;
};

/**
 * @brief add an argument with the name to the ArgumentGroup
 *
 * @tparam T the type of argument
 * @param name the name of the argument
 * @return ArgType<T>& a reference to the added argument
 */
template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgumentGroup::addArgument(std::string const& name) {
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
    auto realname = toLowerString(name);
    if (_pimpl->arguments.contains(realname)) {
        printDuplicateArgNameErrorMsg(name);
    } else {
        _pimpl->arguments.emplace(realname, Argument(T{}));
    }

    ArgType<T>& returnRef = dynamic_cast<Argument::Model<ArgType<T>>*>(_pimpl->arguments.at(realname)._pimpl.get())->inner;

    if (!hasOptionPrefix(realname)) {
        returnRef.required(true).metavar(realname);
    } else {
        returnRef.metavar(toUpperString(realname.substr(realname.find_first_not_of(_pimpl->optionPrefix))));
    }

    _pimpl->optionsAnalyzed = false;

    return returnRef.name(realname);
}

/**
 * @brief implements the details of ArgumentParser::operator[]
 *
 * @tparam ArgT ArgType<T>
 * @param arg the ArgType<T> object
 * @param name name of the argument to look up
 * @return decltype(auto)
 */
template <typename ArgT>
decltype(auto)
ArgumentParser::operator_bracket_impl(ArgT&& arg, std::string const& name) {
    if (std::forward<ArgT>(arg)._pimpl->subparsers.has_value() && std::forward<ArgT>(arg)._pimpl->subparsers->isParsed()) {
        if (std::forward<ArgT>(arg).getActivatedSubParser()._pimpl->arguments.contains(toLowerString(name))) {
            return std::forward<ArgT>(arg).getActivatedSubParser()._pimpl->arguments.at(toLowerString(name));
        }
    }
    try {
        return std::forward<ArgT>(arg)._pimpl->arguments.at(toLowerString(name));
    } catch (std::out_of_range& e) {
        std::cerr << "Argument name \"" << name
                  << "\" does not exist for command \""
                  << formatter.styledCmdName(std::forward<ArgT>(arg).getName(), std::forward<ArgT>(arg).getNumRequiredChars()) << "\"\n";
        throw e;
    }
}

}  // namespace ArgParse