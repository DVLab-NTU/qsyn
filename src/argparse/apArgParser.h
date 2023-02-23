/****************************************************************************
  FileName     [ apArgParser.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef QSYN_ARGPARSE_ARGPARSER_H
#define QSYN_ARGPARSE_ARGPARSER_H

#include <array>
#include <cassert>
#include <variant>

#include "apArgument.h"
#include "myTrie.h"
#include "ordered_hashmap.h"

namespace ArgParse {

class ArgumentParser {
public:
    ArgumentParser() : _optionPrefix("-"), _optionsAnalyzed(false) {}

    Argument& operator[](std::string const& name);
    Argument const& operator[](std::string const& name) const;

    ArgumentParser& name(std::string const& name);
    ArgumentParser& help(std::string const& help);

    // print functions

    void printTokens() const;
    void printArguments() const;
    void printUsage() const;
    void printSummary() const;
    void printHelp() const;

    std::string getSyntaxString(Argument const& arg) const;

    // setters

    void setOptionPrefix(std::string const& prefix) { _optionPrefix = prefix; }

    // getters and attributes

    std::string const& getName() const { return _name; }
    std::string const& getHelp() const { return _help; }

    bool hasOptionPrefix(std::string const& str) const {
        return str.find_first_of(_optionPrefix) == 0UL;
    }

    bool hasOptionPrefix(Argument const& arg) const {
        return arg.getName().find_first_of(_optionPrefix) == 0UL;
    }

    // action

    template <typename T>
    ArgType<T>& addArgument(std::string const& name);

    bool parse(std::string const& line);

private:
    ordered_hashmap<std::string, Argument> _arguments;
    std::string _optionPrefix;
    std::vector<Token> _tokens;

    std::string _name;
    std::string _help;
    size_t _numRequiredChars;

    // members for analyzing parser options
    MyTrie mutable _trie;
    bool mutable _optionsAnalyzed;
    std::array<size_t, 3> mutable _printTableWidths;

    // addArgument error printing

    void printDuplicateArgNameErrorMsg(std::string const& name) const;

    // pretty printing helpers

    std::string requiredArgBracket(std::string const& str) const;
    std::string optionalArgBracket(std::string const& str) const;
    void printHelpString(Argument const& arg) const;
    std::string styledArgName(Argument const& arg) const;
    std::string styledCmdName() const;

    // parse subroutine
    bool analyzeOptions() const;
    bool tokenize(std::string const& line);
    bool parseOptions();
    bool parsePositionalArguments();

    // parseOptions subroutine

    std::variant<std::string, size_t> matchOption(std::string const& token) const;
    void printAmbiguousOptionErrorMsg(std::string const& token) const;
    bool allRequiredOptionsAreParsed() const;

    // parsePositionalArguments subroutine

    bool allTokensAreParsed() const;
    bool allRequiredArgumentsAreParsed() const;
    void printRequiredArgumentsMissingErrorMsg() const;
};

/**
 * @brief add an argument with the name.
 * 
 * @tparam T 
 * @param name 
 * @return ArgType<T>& 
 */
template <typename T>
ArgType<T>& ArgumentParser::addArgument(std::string const& name) {
    auto realname = toLowerString(name);
    if (_arguments.contains(realname)) {
        printDuplicateArgNameErrorMsg(name);
    } else {
        _arguments.emplace(realname, Argument(T{}));
    }

    ArgType<T>& returnRef = dynamic_cast<Argument::Model<ArgType<T>>*>(_arguments.at(realname)._pimpl.get())->inner;

    if (!hasOptionPrefix(realname)) {
        returnRef.required(true).metavar(realname);
    } else {
        returnRef.metavar(toUpperString(realname.substr(realname.find_first_not_of(_optionPrefix))));
    }

    _optionsAnalyzed = false;

    return returnRef.name(name);
}

}  // namespace ArgParse

#endif  // QSYN_ARGPARSE_ARGPARSER_H