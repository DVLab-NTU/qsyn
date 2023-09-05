/****************************************************************************
  FileName     [ apArgParser.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser core functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./argParser.hpp"

#include <fmt/format.h>

#include <cassert>
#include <cstdlib>
#include <numeric>
#include <ranges>

#include "fmt/core.h"
#include "util/trie.hpp"
#include "util/util.hpp"

using namespace std;

namespace ArgParse {

ArgumentParser::ArgumentParser(std::string const& n, ParserConfig config) : ArgumentParser() {
    this->name(n);

    if (config.addHelpAction) {
        this->addArgument<bool>("-h", "--help")
            .action(help)
            .help("show this help message and exit");
    }
    if (config.addVersionAction) {
        this->addArgument<bool>("-V", "--version")
            .action(version)
            .help("show program's version number and exit");
    }

    _pimpl->exitOnFailure = config.exitOnFailure;
    _pimpl->version = config.version;
}

size_t ArgumentParser::numParsedArguments() const {
    return ranges::count_if(
        _pimpl->arguments | views::values,
        [](Argument& arg) {
            return arg.isParsed();
        });
}

/**
 * @brief Print the tokens and their parse states
 *
 */
void ArgumentParser::printTokens() const {
    size_t i = 0;
    for (auto& [token, parsed] : _pimpl->tokens) {
        fmt::println("Token #{:<8}:\t{} ({}) Frequency: {:>3}",
                     ++i, token, (parsed ? "parsed" : "unparsed"), _pimpl->trie.frequency(token));
    }
}

/**
 * @brief Print the argument and their parse states
 *
 */
void ArgumentParser::printArguments() const {
    for (auto& [_, arg] : _pimpl->arguments) {
        arg.printStatus();
    }
}

Argument& ArgumentParser::getArgument(std::string const& name) {
    return getArgumentImpl(*this, name);
}

Argument const& ArgumentParser::getArgument(std::string const& name) const {
    return getArgumentImpl(*this, name);
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

ArgumentParser& ArgumentParser::numRequiredChars(size_t num) {
    _pimpl->numRequiredChars = num;
    return *this;
}

size_t ArgumentParser::getArgNumRequiredChars(std::string const& name) const {
    assert(_pimpl->arguments.contains(name) || _pimpl->aliasForwardMap.contains(name));
    auto nReq = _pimpl->trie.shortestUniquePrefix(name).value().size();
    while (_pimpl->optionPrefix.find_first_of(name[nReq - 1]) != std::string::npos) {
        ++nReq;
    }
    return nReq;
}

// Parser subroutine

/**
 * @brief Analyze the options for the argument parser. This function generates
 *        auxiliary parsing information for the argument parser.
 *
 * @return true
 * @return false
 */
bool ArgumentParser::analyzeOptions() const {
    if (_pimpl->optionsAnalyzed) return true;

    // calculate the number of required characters to differentiate each option

    _pimpl->trie.clear();
    _pimpl->conflictGroups.clear();

    if (_pimpl->subparsers.has_value()) {
        for (auto const& [name, parser] : _pimpl->subparsers->getSubParsers()) {
            _pimpl->trie.insert(name);
        }
    }

    for (auto const& group : _pimpl->mutuallyExclusiveGroups) {
        for (auto const& name : group.getArguments()) {
            if (_pimpl->arguments.at(name).isRequired()) {
                fmt::println(stderr, "[ArgParse] Error: mutually exclusive argument \"{}\" must be optional!!", name);
                exit(1);
                return false;
            }
            _pimpl->conflictGroups.emplace(name, group);
        }
    }

    for (auto& [name, arg] : _pimpl->arguments) {
        if (!hasOptionPrefix(name)) continue;
        _pimpl->trie.insert(name);
        arg._isOption = true;
    }

    for (auto& alias : _pimpl->aliasForwardMap | views::keys) {
        assert(hasOptionPrefix(alias));
        _pimpl->trie.insert(alias);
    }

    if (_pimpl->subparsers.has_value()) {
        for (auto& [name, parser] : _pimpl->subparsers->getSubParsers()) {
            size_t prefixSize = _pimpl->trie.shortestUniquePrefix(name).value().size();
            while (!isalpha(name[prefixSize - 1])) ++prefixSize;
            parser.numRequiredChars(max(prefixSize, parser.getNumRequiredChars()));
        }
    }

    _pimpl->optionsAnalyzed = true;
    return true;
}

/**
 * @brief tokenize the string for the argument parsing
 *
 * @param line
 * @return true if succeeded
 * @return false if failed
 */
bool ArgumentParser::tokenize(string const& line) {
    _pimpl->tokens.clear();
    auto stripped = stripQuotes(line);
    if (!stripped.has_value()) {
        fmt::println(stderr, "Error: missing ending quote!!");
        return false;
    }

    for (auto&& tmp : split(line, " ")) {
        _pimpl->tokens.emplace_back(tmp);
    }

    if (_pimpl->tokens.empty()) return true;
    // concat tokens with '\ ' to a single token with space in it
    for (auto itr = next(_pimpl->tokens.rbegin()); itr != _pimpl->tokens.rend(); ++itr) {
        string& currToken = itr->token;
        string& nextToken = prev(itr)->token;

        if (currToken.ends_with('\\') && !currToken.ends_with("\\\\")) {
            currToken.back() = ' ';
            currToken += nextToken;
            nextToken = "";
        }
    }
    erase_if(_pimpl->tokens, [](Token const& token) { return token.token == ""; });

    // convert "abc=def", "abc:def" to "abc def"

    size_t nTokens = _pimpl->tokens.size();
    for (auto tokenItr = _pimpl->tokens.begin(); tokenItr != _pimpl->tokens.end(); ++tokenItr) {
        string& currToken = tokenItr->token;
        size_t pos = currToken.find_first_of("=:");

        if (pos != string::npos && pos != 0) {
            _pimpl->tokens.emplace(std::next(tokenItr), currToken.substr(pos + 1));
            nTokens++;
            currToken = currToken.substr(0, pos);
        }
    }

    return true;
}

/**
 * @brief tokenize the line and parse the arguments
 *
 * @param line
 * @return true
 * @return false
 */
bool ArgumentParser::parseArgs(std::string const& line) { return tokenize(line) && parseArgs(_pimpl->tokens); }

/**
 * @brief parse the arguments from tokens
 *
 * @param tokens
 * @return true
 * @return false
 */
bool ArgumentParser::parseArgs(std::vector<std::string> const& tokens) {
    auto tmp = std::vector<Token>{tokens.begin(), tokens.end()};
    return parseArgs(tmp);
}

/**
 * @brief parse the arguments from tokens
 *
 * @param tokens
 * @return true
 * @return false
 */
bool ArgumentParser::parseArgs(TokensView tokens) {
    auto [success, unrecognized] = parseKnownArgs(tokens);

    if (!success) return false;

    return dvlab::utils::expect(unrecognized.empty(),
                                fmt::format("Error: unrecognized arguments: \"{}\"!!",
                                            fmt::join(unrecognized | std::views::transform([](Token const& tok) { return tok.token; }), "\" \"")));
}

/**
 * @brief tokenize the line and parse the arguments known by the parser
 *
 * @param line
 * @return std::pair<bool, std::vector<Token>>, where
 *         the first return value specifies whether the parse has succeeded, and
 *         the second one specifies the unrecognized tokens
 */
std::pair<bool, std::vector<Token>> ArgumentParser::parseKnownArgs(std::string const& line) {
    if (!tokenize(line)) return {false, {}};
    return parseKnownArgs(_pimpl->tokens);
}

/**
 * @brief  parse the arguments known by the tokens from tokens
 *
 * @return std::pair<bool, std::vector<Token>>, where
 *         the first return value specifies whether the parse has succeeded, and
 *         the second one specifies the unrecognized tokens
 */
std::pair<bool, std::vector<Token>> ArgumentParser::parseKnownArgs(std::vector<std::string> const& tokens) {
    auto tmp = std::vector<Token>{tokens.begin(), tokens.end()};
    return parseKnownArgs(tmp);
}

/**
 * @brief  parse the arguments known by the tokens from tokens
 *
 * @return std::pair<bool, std::vector<Token>>, where
 *         the first return value specifies whether the parse has succeeded, and
 *         the second one specifies the unrecognized tokens
 */
std::pair<bool, std::vector<Token>> ArgumentParser::parseKnownArgs(TokensView tokens) {
    auto result = parseKnownArgsImpl(tokens);
    if (!result.first && _pimpl->exitOnFailure) {
        exit(0);
    }

    return result;
}

/**
 * @brief the internal implementation of parseKnownArgs
 *
 * @param tokens
 * @return std::pair<bool, std::vector<Token>>
 */
std::pair<bool, std::vector<Token>> ArgumentParser::parseKnownArgsImpl(TokensView tokens) {
    if (!analyzeOptions()) return {false, {}};

    _pimpl->activatedSubParser = std::nullopt;
    for (auto& mutex : _pimpl->mutuallyExclusiveGroups) {
        mutex.setParsed(false);
    }

    auto subparserTokenPos = std::invoke([this, tokens]() -> size_t {
        if (!_pimpl->subparsers.has_value())
            return tokens.size();

        size_t pos = 0;
        for (auto const& [token, _] : tokens) {
            for (auto const& [name, subparser] : _pimpl->subparsers->getSubParsers()) {
                if (name.starts_with(token)) {
                    setSubParser(name);
                    return pos;
                }
            }
            ++pos;
        }
        return pos;
    });

    for (auto& arg : _pimpl->arguments | views::values) {
        arg.reset();
    }

    TokensView main_parser_tokens = tokens.subspan(0, subparserTokenPos);

    std::vector<Token> unrecognized;
    if (!parseOptions(main_parser_tokens, unrecognized) ||
        !parsePositionalArguments(main_parser_tokens, unrecognized)) {
        return {false, {}};
    }

    fillUnparsedArgumentsWithDefaults();
    if (hasSubParsers()) {
        TokensView subparser_tokens = tokens.subspan(subparserTokenPos + 1);
        if (_pimpl->activatedSubParser) {
            auto [success, subparser_unrecognized] = getActivatedSubParser()->parseKnownArgs(subparser_tokens);
            if (!success) return {false, {}};
            unrecognized.insert(unrecognized.end(), subparser_unrecognized.begin(), subparser_unrecognized.end());
        } else if (_pimpl->subparsers->isRequired()) {
            fmt::println(stderr, "Error: missing mandatory subparser argument: ({})", fmt::join(_pimpl->subparsers->getSubParsers() | views::keys, ", "));
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
bool ArgumentParser::parseOptions(TokensView tokens, std::vector<Token>& unrecognized) {
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (!hasOptionPrefix(tokens[i].token) || tokens[i].parsed) continue;
        auto match = matchOption(tokens[i].token);
        if (std::holds_alternative<size_t>(match)) {
            if (float tmp = 0; myStr2Float(tokens[i].token, tmp))  // if the argument is a number, skip to the next arg
                continue;
            auto frequency = std::get<size_t>(match);
            assert(frequency != 1);

            if (frequency == 0) continue;  // unrecognized; may be positional arguments or errors
            // else this is an error
            printAmbiguousOptionErrorMsg(tokens[i].token);
            return false;
        }
        Argument& arg = getArgument(std::get<std::string>(match));

        if (arg.isHelpAction()) {
            this->printHelp();
            return false;  // break the parsing
        }

        if (arg.isVersionAction()) {
            this->printVersion();
            return false;  // break the parsing
        }

        auto parse_range = arg.getParseRange(tokens);
        if (!arg.tokensEnoughToParse(parse_range)) return false;

        if (!arg.takeAction(tokens.subspan(i + 1, std::min(arg.getNArgs().upper, tokens.size() - (i + 1))))) {
            return false;
        }

        if (!noConflictWithParsedArguments(arg)) return false;

        tokens[i].parsed = true;
        arg.markAsParsed();  // if the options is present, no matter if there's any argument the follows, mark it as parsed
    }

    return allRequiredOptionsAreParsed();
}

/**
 * @brief Parse positional arguments, i.e., arguments that must appear in a specific order.
 *
 * @return true if succeeded
 * @return false if failed
 */
bool ArgumentParser::parsePositionalArguments(TokensView tokens, std::vector<Token>& unrecognized) {
    for (auto& [name, arg] : _pimpl->arguments) {
        if (arg.isParsed() || hasOptionPrefix(name)) continue;

        auto parse_range = arg.getParseRange(tokens);
        auto [lower, upper] = arg.getNArgs();

        if (parse_range.size() < arg.getNArgs().lower) {
            if (arg.isRequired()) {
                fmt::println(stderr, "Error: missing argument \"{}\": expected {}{} arguments!!",
                             getName(), (lower < upper ? "at least " : ""), lower);
                return false;
            } else
                continue;
        }

        if (!arg.takeAction(parse_range)) return false;

        if (parse_range.size()) {
            if (!noConflictWithParsedArguments(arg)) return false;
            arg.markAsParsed();
        }

        // only mark as parsed if at least some tokens is associated with this argument
    }
    ranges::copy_if(tokens, back_inserter(unrecognized), [](Token const& token) { return !token.parsed; });

    return allRequiredArgumentsAreParsed() && allRequiredMutexGroupsAreParsed();
}

void ArgumentParser::fillUnparsedArgumentsWithDefaults() {
    for (auto& [name, arg] : _pimpl->arguments) {
        if (!arg.isParsed() && arg.hasDefaultValue()) {
            arg.setValueToDefault();
        }
    }
}

/**
 * @brief Get the matching option name to a token.
 *
 * @param token
 * @return optional<string> return the option name if exactly one option matches the token. Otherwise, return std::nullopt
 */
variant<string, size_t> ArgumentParser::matchOption(std::string const& token) const {
    auto match = _pimpl->trie.findWithPrefix(token);
    if (match.has_value()) {
        if (token.size() < getArgNumRequiredChars(match.value())) {
            return 0u;
        }
        return match.value();
    }

    return _pimpl->trie.frequency(token);
}

bool ArgumentParser::noConflictWithParsedArguments(Argument const& arg) const {
    if (!_pimpl->conflictGroups.contains(arg.getName())) return true;

    auto& mutexGroup = _pimpl->conflictGroups.at(arg.getName());
    if (!mutexGroup.isParsed()) {
        mutexGroup.setParsed(true);
        return true;
    }

    return std::ranges::all_of(mutexGroup.getArguments(), [this, &arg](string const& name) {
        return dvlab::utils::expect(name == arg.getName() || !_pimpl->arguments.at(name).isParsed(), fmt::format("Error: argument \"{}\" cannot occur with \"{}\"!!", arg.getName(), name));
    });
}

/**
 * @brief print all potential option name for a token.
 *        This function is meant to be used when there are
 *        multiple such instances
 *
 */
void ArgumentParser::printAmbiguousOptionErrorMsg(std::string const& token) const {
    fmt::println(stderr, "Error: ambiguous option: \"{}\" could match {}",
                 token,
                 fmt::join(_pimpl->arguments | views::keys | views::filter([this, token](string const& name) {
                               return hasOptionPrefix(name) && name.starts_with(token);
                           }),
                           ", "));
}

/**
 * @brief Check if all required options are parsed
 *
 * @return true or false
 */
bool ArgumentParser::allRequiredOptionsAreParsed() const {
    auto requiredOptionRange = _pimpl->arguments | views::values |
                               views::filter([](Argument const& arg) { return arg.isOption(); }) |
                               views::filter([](Argument const& arg) { return arg.isRequired(); });
    return dvlab::utils::expect(
        ranges::all_of(requiredOptionRange, [this](Argument const& arg) { return arg.isParsed(); }),
        fmt::format("Error: missing option(s)!! The following options are required: {}",  // intentional linebreak
                    fmt::join(requiredOptionRange |
                                  views::filter([](Argument const& arg) { return !arg.isParsed(); }) |
                                  views::transform([](Argument const& arg) { return arg.getName(); }),
                              ", ")));
}

/**
 * @brief Check if all required groups are parsed
 *
 * @return true or false
 */
bool ArgumentParser::allRequiredMutexGroupsAreParsed() const {
    return ranges::all_of(_pimpl->mutuallyExclusiveGroups, [](MutuallyExclusiveGroup const& group) {
        return dvlab::utils::expect(!group.isRequired() || group.isParsed(),
                                    fmt::format("Error: one of the options are required: {}!!", fmt::join(group.getArguments(), ", ")));
    });
}

/**
 * @brief Check if all required arguments are parsed
 *
 * @return true or false
 */
bool ArgumentParser::allRequiredArgumentsAreParsed() const {
    auto requiredArgRange = _pimpl->arguments | views::values |
                            views::filter([](Argument const& arg) { return arg.isRequired(); });
    return dvlab::utils::expect(
        ranges::all_of(requiredArgRange, [this](Argument const& arg) { return arg.isParsed(); }),
        fmt::format("Error: missing argument(s)!! The following arguments are required: {}",  // intentional linebreak
                    fmt::join(requiredArgRange |
                                  views::filter([](Argument const& arg) { return !arg.isParsed(); }) |
                                  views::transform([](Argument const& arg) { return arg.getName(); }),
                              ", ")));
}

MutuallyExclusiveGroup ArgumentParser::addMutuallyExclusiveGroup() {
    _pimpl->mutuallyExclusiveGroups.emplace_back(*this);
    return _pimpl->mutuallyExclusiveGroups.back();
}

ArgumentParser SubParsers::addParser(std::string const& name) {
    _pimpl->subparsers.emplace(name, ArgumentParser{name});
    return _pimpl->subparsers.at(name);
}

SubParsers ArgumentParser::addSubParsers() {
    if (_pimpl->subparsers.has_value()) {
        fmt::println(stderr, "Error: an ArgumentParser can only have one set of subparsers!!");
        exit(-1);
    }
    _pimpl->subparsers = SubParsers{};
    return _pimpl->subparsers.value();
}

}  // namespace ArgParse