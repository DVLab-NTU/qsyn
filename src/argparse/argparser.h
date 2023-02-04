/****************************************************************************
  FileName     [ argparse.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define class ArgumentParser member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_ARG_PARSER_H
#define QSYN_ARG_PARSER_H

#include <map>
#include <string>
#include <vector>

#include "argparseArgument.h"
#include "argparseErrorMsg.h"
#include "ordered_hashmap.h"
#include "util.h"

namespace ArgParse {

class ArgumentParser {
public:
    ArgumentParser() {}

    template <typename ArgType>
    Argument& addArgument(std::string const& argName);

    SubParsers& addSubParsers(std::string const& name, std::string const& help) {
        return addArgument<SubParsers>(name).help(help);
    }

    void printUsage() const;
    void printSummary() const;
    void printHelp() const;
    void printTokens() const;
    void printArguments() const;

    // add attribute
    void cmdInfo(std::string const& cmdName, std::string const& cmdDescription);

    // setters and getters
    std::string getCmdName() const { return _cmdName; }

    Argument& operator[](std::string const& key);
    Argument const& operator[](std::string const& key) const;

    ParseResult parse(std::string const& line);

protected:
    ordered_hashmap<std::string, Argument> _arguments;
    std::string _cmdName;
    std::string _cmdDescription;
    size_t _cmdNumMandatoryChars;

    std::vector<TokenPair> _tokenPairs;

    bool mutable _optionsAnalyzed;

    bool analyzeOptions() const;
    bool tokenize(std::string const& line);

    ParseResult parseOptionalArguments();
    ParseResult parseMandatoryArguments();

    std::string formattedCmdName() const;
};

class SubParsers {
public:
    ArgumentParser& addParser(std::string const& name, std::string const& help);

    friend std::ostream& operator<<(std::ostream& os, SubParsers const& sap) {
        return os << "(subparsers)";
    }

    bool operator== (SubParsers const& other) const {
        // REVIEW - how to check equivalency?
        return false;
    }

    bool operator!= (SubParsers const& other) const {
        return !(*this == other);
    }

    ArgumentParser& operator[](std::string const& name) { return _subparsers.at(toLowerString(name)); }
    ArgumentParser const& operator[](std::string const& name) const { return _subparsers.at(toLowerString(name)); }

private:
    ordered_hashmap<std::string, ArgumentParser> _subparsers;
};

/**
 * @brief              add an argument to the `ArgumentParser`.
 *
 * @tparam ArgType     argument type
 * @param argName      the name to the argument. This name can be used to
 *                     access the parsed argument by calling
 *                     `ArgParse::ArgumentParser::operator[](std::string const&)`
 *
 * @return Argument&   return the added argument to ease streaming argument decorators
 */
template <typename ArgType>
Argument& ArgumentParser::addArgument(std::string const& argName) {
    if (argName.empty()) {
        detail::printArgNameEmptyErrorMsg();
        throw illegal_parser_arg{};
    }

    std::string realName = toLowerString(argName);

    if (_arguments.contains(realName)) {
        detail::printArgNameDuplicateErrorMsg(realName);
        throw illegal_parser_arg{};
    }

    _arguments.emplace(realName, ArgType());
    _arguments.at(realName).name(realName);
    _arguments.at(realName).setNumMandatoryChars(countUpperChars(argName));

    _optionsAnalyzed = false;
    return _arguments.at(realName);
}

}  // namespace ArgParse

#endif  // QSYN_ARG_PARSER_H