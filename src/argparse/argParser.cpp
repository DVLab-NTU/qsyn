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
#include <numeric>

#include "util/trie.hpp"
#include "util/util.hpp"

using namespace std;

namespace ArgParse {

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
        if (arg.isRequired()) arg.printStatus();
    }
    for (auto& [_, arg] : _pimpl->arguments) {
        if (!arg.isRequired()) arg.printStatus();
    }
}

/**
 * @brief returns the Argument with the `name`
 *
 * @param name
 * @return Argument&
 */
Argument& ArgumentParser::operator[](std::string const& name) {
    return operator_bracket_impl(*this, name);
}

/**
 * @brief returns the Argument with the `name`
 *
 * @param name
 * @return Argument&
 */
Argument const& ArgumentParser::operator[](std::string const& name) const {
    return operator_bracket_impl(*this, name);
}

/**
 * @brief set the command name to the argument parser
 *
 * @param name
 * @return ArgumentParser&
 */
ArgumentParser& ArgumentParser::name(std::string const& name) {
    _pimpl->name = toLowerString(name);
    _pimpl->numRequiredChars = countUpperChars(name);
    return *this;
}

/**
 * @brief set the help message to the argument parser
 *
 * @param name
 * @return ArgumentParser&
 */
ArgumentParser& ArgumentParser::help(std::string const& help) {
    _pimpl->help = help;
    return *this;
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
                fmt::println(stderr, "[ArgParse] Error: Mutually exclusive argument \"{}\" must be optional!!", name);
                return false;
            }
            _pimpl->conflictGroups.emplace(name, group);
        }
    }

    for (auto const& [name, arg] : _pimpl->arguments) {
        if (!hasOptionPrefix(name)) continue;
        _pimpl->trie.insert(name);
    }

    for (auto& [name, arg] : _pimpl->arguments) {
        if (!hasOptionPrefix(name)) continue;
        size_t prefixSize = _pimpl->trie.shortestUniquePrefix(name).value().size();
        while (!isalpha(name[prefixSize - 1])) ++prefixSize;
        arg.setNumRequiredChars(max(prefixSize, arg.getNumRequiredChars()));
    }

    if (_pimpl->subparsers.has_value()) {
        for (auto& [name, parser] : _pimpl->subparsers->getSubParsers()) {
            size_t prefixSize = _pimpl->trie.shortestUniquePrefix(name).value().size();
            while (!isalpha(name[prefixSize - 1])) ++prefixSize;
            parser.setNumRequiredChars(max(prefixSize, parser.getNumRequiredChars()));
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
    for (size_t i = 0; i < nTokens; ++i) {
        string& currToken = _pimpl->tokens[i].token;
        size_t pos = currToken.find_first_of("=:");

        if (pos != string::npos && pos != 0) {
            _pimpl->tokens.emplace(_pimpl->tokens.begin() + i + 1, currToken.substr(pos + 1));
            nTokens++;
            currToken = currToken.substr(0, pos);
        }
    }

    return true;
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

    if (unrecognized.size()) {
        auto quotedView = unrecognized | std::views::transform([](Token const& tok) { return '"' + tok.token + '"'; });
        fmt::println(stderr, "Error: unrecognized arguments: {:}!!", fmt::join(quotedView, " "));
        return false;
    }

    return true;
}

/**
 * @brief  parse the arguments known by the tokens from tokens
 *
 * @return std::pair<bool, std::vector<Token>>, where
 *         the first return value specifies whether the parse has succeeded, and
 *         the second one specifies the unrecognized tokens
 */
std::pair<bool, std::vector<Token>> ArgumentParser::parseKnownArgs(TokensView tokens) {
    if (!analyzeOptions()) return {false, {}};

    _pimpl->activatedSubParser = std::nullopt;

    auto subparserTokenPos = std::invoke([this, tokens]() -> size_t {
        if (!_pimpl->subparsers.has_value())
            return tokens.size();

        size_t pos = 0;
        for (auto const& [token, _] : tokens) {
            for (auto const& [name, subparser] : _pimpl->subparsers->getSubParsers()) {
                if (name.starts_with(toLowerString(token))) {
                    setSubParser(name);
                    return pos;
                }
            }
            ++pos;
        }
        return pos;
    });

    for (auto& [_, arg] : _pimpl->arguments) {
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
            cerr << "Error: missing mandatory subparser argument: "
                 << formatter.getSyntaxString(_pimpl->subparsers.value())
                 << endl;
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
            if (float tmp; myStr2Float(tokens[i].token, tmp))  // if the argument is a number, skip to the next arg
                continue;
            auto frequency = std::get<size_t>(match);
            assert(frequency != 1);

            if (frequency == 0) continue;  // unrecognized; may be positional arguments or errors
            // else this is an error
            printAmbiguousOptionErrorMsg(tokens[i].token);
            return false;
        }

        Argument& arg = _pimpl->arguments[std::get<string>(match)];

        if (conflictWithParsedArguments(arg)) return false;

        auto parse_range = arg.getParseRange(tokens);
        if (!arg.tokensEnoughToParse(parse_range)) return false;

        if (!arg.takeAction(tokens.subspan(i + 1, std::min(arg.getNArgs().upper, tokens.size() - (i + 1))))) return false;
        tokens[i].parsed = true;
        arg.markAsParsed();
    }

    return allRequiredOptionsAreParsed() && allRequiredMutexGroupsAreParsed();
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
        if (!arg.tokensEnoughToParse(parse_range)) return false;
        if (!arg.takeAction(parse_range)) return false;

        // only mark as parsed if at least some tokens is associated with this argument
        if (parse_range.size()) arg.markAsParsed();
    }

    if (!allRequiredArgumentsAreParsed()) {
        printRequiredArgumentsMissingErrorMsg();
        return false;
    }

    for (auto& token : tokens) {
        if (!token.parsed) {
            unrecognized.emplace_back(token);
        }
    }

    return true;
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
    auto key = toLowerString(token);
    auto match = _pimpl->trie.findWithPrefix(key);
    if (match.has_value()) {
        if (key.size() < _pimpl->arguments.at(match.value()).getNumRequiredChars()) {
            return 0u;
        }
        return match.value();
    }

    return _pimpl->trie.frequency(key);
}

bool ArgumentParser::conflictWithParsedArguments(Argument const& arg) const {
    if (!_pimpl->conflictGroups.contains(arg.getName())) return false;

    auto& conflictGroup = _pimpl->conflictGroups.at(arg.getName());
    if (!conflictGroup.isParsed()) {
        conflictGroup.setParsed(true);
        return false;
    }

    for (auto const& conflict : conflictGroup.getArguments()) {
        if (_pimpl->arguments.at(conflict).isParsed()) {
            cerr << "Error: argument \"" << arg.getName() << "\" cannot occur with \"" << conflict << "\"!!\n";
            return true;
        }
    }

    return false;
}

/**
 * @brief print all potential option name for a token.
 *        This function is meant to be used when there are
 *        multiple such instances
 *
 */
void ArgumentParser::printAmbiguousOptionErrorMsg(std::string const& token) const {
    auto key = toLowerString(token);
    cerr << "Error: ambiguous option: \"" << token << "\" could match ";
    size_t ctr = 0;
    for (auto& [name, _] : _pimpl->arguments) {
        if (!hasOptionPrefix(name)) continue;
        if (name.starts_with(key)) {
            if (ctr > 0) cerr << ", ";
            cerr << name;
            ctr++;
        }
    }
    cerr << endl;
}

/**
 * @brief Check if all required options are parsed
 *
 * @return true or false
 */
bool ArgumentParser::allRequiredOptionsAreParsed() const {
    // Want: ∀ arg ∈ _arguments. (option(arg) ∧ required(arg)) → parsed(arg)
    // Thus: ∀ arg ∈ _arguments. ¬option(arg) ∨ ¬required(arg) ∨ parsed(arg)
    for (auto& [name, arg] : _pimpl->arguments) {
        if (hasOptionPrefix(name) && arg.isRequired() && !arg.isParsed()) {
            cerr << "Error: The option \"" << name << "\" is required!!" << endl;
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if all required groups are parsed
 *
 * @return true or false
 */
bool ArgumentParser::allRequiredMutexGroupsAreParsed() const {
    for (auto& group : _pimpl->mutuallyExclusiveGroups) {
        if (group.isRequired() && !group.isParsed()) {
            cerr << "Error: One of the options are required: ";
            size_t ctr = 0;
            for (auto& name : group.getArguments()) {
                cerr << name;
                if (++ctr < group.getArguments().size()) cerr << ", ";
            }
            cerr << "!!\n";
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if all tokens are parsed
 *
 * @return true or false
 */
bool ArgumentParser::allTokensAreParsed(TokensView tokens) const {
    return ranges::all_of(tokens, [](Token const& tok) {
        return tok.parsed;
    });
}

/**
 * @brief Check if all required arguments are parsed
 *
 * @return true or false
 */
bool ArgumentParser::allRequiredArgumentsAreParsed() const {
    // Want: ∀ arg ∈ _arguments. required(arg) → parsed(arg)
    // Thus: ∀ arg ∈ _arguments. ¬required(arg) ∨ parsed(arg)
    return all_of(_pimpl->arguments.begin(), _pimpl->arguments.end(), [](pair<string, Argument> const& argPair) {
        return !argPair.second.isRequired() || argPair.second.isParsed();
    });
}

/**
 * @brief print all missing required arguments in a parsing.
 *
 */
void ArgumentParser::printRequiredArgumentsMissingErrorMsg() const {
    cerr << "Error: Missing argument(s)!! The following arguments are required: ";
    size_t ctr = 0;
    for (auto& [name, arg] : _pimpl->arguments) {
        if (!arg.isParsed() && arg.isRequired()) {
            if (ctr > 0) cerr << ", ";
            cerr << name;
            ctr++;
        }
    }
    cerr << endl;
}

/**
 * @brief print the error message when duplicated argument name is detected
 *
 * @param name
 */
void ArgumentParser::printDuplicateArgNameErrorMsg(std::string const& name) {
    fmt::println(stderr, "[ArgParse] Error: Duplicate argument name \"{}\"!!", name);
}

ArgumentGroup ArgumentParser::addMutuallyExclusiveGroup() {
    _pimpl->mutuallyExclusiveGroups.emplace_back(*this);
    return _pimpl->mutuallyExclusiveGroups.back();
}

ArgumentParser SubParsers::addParser(std::string const& n) {
    _pimpl->subparsers.emplace(toLowerString(n), ArgumentParser{n});
    return _pimpl->subparsers.at(toLowerString(n));
}

SubParsers ArgumentParser::addSubParsers() {
    if (_pimpl->subparsers.has_value()) {
        fmt::println(stderr, "Error: An ArgumentParser can only have one set of subparsers!!");
        exit(-1);
    }
    _pimpl->subparsers = SubParsers{};
    return _pimpl->subparsers.value();
}

}  // namespace ArgParse