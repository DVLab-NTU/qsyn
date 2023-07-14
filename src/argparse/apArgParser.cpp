/****************************************************************************
  FileName     [ apArgParser.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser core functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <iomanip>
#include <iostream>
#include <numeric>

#include "argparse.h"
#include "myTrie.h"
#include "util.h"

using namespace std;

namespace ArgParse {

/**
 * @brief returns the Argument with the `name`
 *
 * @param name
 * @return Argument&
 */
Argument& ArgumentParser::operator[](std::string const& name) {
    return operatorBracketImpl(*this, name);
}

/**
 * @brief returns the Argument with the `name`
 *
 * @param name
 * @return Argument&
 */
Argument const& ArgumentParser::operator[](std::string const& name) const {
    return operatorBracketImpl(*this, name);
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

/**
 * @brief parse the arguments in the line
 *
 * @param line
 * @return true
 * @return false
 */
bool ArgumentParser::parse(std::string const& line) {
    for (auto& [_, arg] : _pimpl->arguments) {
        arg.reset();
    }

    return tokenize(line) && parseTokens(_pimpl->tokens);
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
                cerr << "[ArgParse] Error: Mutually exclusive argument \"" << name << "\" must be optional!!" << endl;
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

    // calculate tabler info

    vector<size_t> widths = {0, 0, 0, 0};

    for (auto& [name, arg] : _pimpl->arguments) {
        if (arg.getTypeString().size() >= widths[0]) {
            widths[0] = arg.getTypeString().size();
        }
        if (arg.getName().size() >= widths[1]) {
            widths[1] = arg.getName().size();
        }
        if (arg.getMetavar().size() >= widths[2]) {
            widths[2] = arg.getMetavar().size();
        }
    }

    _pimpl->tabl.presetStyle(dvlab_utils::Tabler::PresetStyle::ASCII_MINIMAL)
        .indent(1)
        .rightMargin(2)
        .widths(widths);

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
    string buffer, stripped;
    if (!stripQuotes(line, stripped)) {
        cerr << "Error: missing ending quote!!" << endl;
        return false;
    }
    size_t pos = myStrGetTok(stripped, buffer);
    while (buffer.size()) {
        _pimpl->tokens.emplace_back(buffer);
        pos = myStrGetTok(stripped, buffer, pos);
    }
    if (_pimpl->tokens.empty()) return true;
    // concat tokens with '\ ' to a single token with space in it
    for (auto itr = next(_pimpl->tokens.rbegin()); itr != _pimpl->tokens.rend(); ++itr) {
        string& currToken = itr->token;
        string& nextToken = prev(itr)->token;

        if (currToken.ends_with('\\')) {
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

bool ArgumentParser::parseTokens(TokensView tokens) {
    if (!analyzeOptions()) return false;

    size_t subparserTokenPos = std::invoke([this, tokens]() -> size_t {
        if (!_pimpl->subparsers.has_value())
            return tokens.size();

        size_t pos = 0;
        for (auto const& [token, _] : tokens) {
            for (auto const& [name, subparser] : _pimpl->subparsers->getSubParsers()) {
                if (token == name) {
                    setSubParser(name);
                    return pos;
                }
            }
            ++pos;
        }
        return pos;
    });

    if (hasSubParsers()) {
        if (subparserTokenPos >= tokens.size() && _pimpl->subparsers->isRequired()) {
            cerr << "Error: missing mandatory subparser argument: " << getSyntaxString(_pimpl->subparsers.value()) << endl;
            return false;
        }
        if (subparserTokenPos < tokens.size() && !getActivatedSubParser().parseTokens(tokens.subspan(subparserTokenPos + 1))) {
            return false;
        }
    }
    return parseOptions(tokens.subspan(0, subparserTokenPos)) && parsePositionalArguments(tokens.subspan(0, subparserTokenPos));
}

/**
 * @brief Parse the optional arguments, i.e., the arguments that starts with
 *        one of the option prefix.
 *
 * @return true if succeeded
 * @return false if failed
 */
bool ArgumentParser::parseOptions(TokensView tokens) {
    for (int i = tokens.size() - 1; i >= 0; --i) {
        if (!hasOptionPrefix(tokens[i].token)) continue;
        auto match = matchOption(tokens[i].token);
        if (std::holds_alternative<size_t>(match)) {
            auto frequency = std::get<size_t>(match);
            assert(frequency != 1);
            // if the argument is a number, skip to the next arg
            if (float tmp; myStr2Float(tokens[i].token, tmp))
                continue;
            // else this is an error
            if (frequency == 0) {
                cerr << "Error: unrecognized option \"" << tokens[i].token << "\"!!\n";
            } else {
                printAmbiguousOptionErrorMsg(tokens[i].token);
            }

            return false;
        }

        Argument& arg = _pimpl->arguments[get<string>(match)];

        if (_pimpl->conflictGroups.contains(arg.getName())) {
            auto& conflictGroup = _pimpl->conflictGroups.at(arg.getName());
            if (conflictGroup.isParsed()) {
                for (auto const& conflict : conflictGroup.getArguments()) {
                    if (_pimpl->arguments.at(conflict).isParsed()) {
                        cerr << "Error: argument \"" << arg.getName() << "\" cannot occur with \"" << conflict << "\"!!\n";
                        return false;
                    }
                }
            }
            conflictGroup.setParsed(true);
        }

        if (arg.hasAction())
            arg.parse(tokens.subspan(i, 0));
        else if (i + 1 >= (int)tokens.size() || tokens[i + 1].parsed) {  // _tokens[i] is not the last token && _tokens[i+1] is unparsed
            cerr << "Error: missing argument after \"" << tokens[i].token << "\"!!\n";
            return false;
        } else if (!arg.parse(tokens.subspan(i + 1, 1))) {
            cerr << "Error: invalid " << arg.getTypeString() << " value \""
                 << tokens[i + 1].token << "\" after \""
                 << tokens[i].token << "\"!!" << endl;
            return false;
        }

        // check if meet constraints
        for (auto& [constraint, onerror] : arg.getConstraints()) {
            if (!constraint()) {
                onerror();
                return false;
            }
        }

        tokens[i].parsed = true;

        if (!arg.hasAction()) {
            tokens[i + 1].parsed = true;
        }
    }

    return allRequiredOptionsAreParsed() && allRequiredMutexGroupsAreParsed();
}

/**
 * @brief Parse positional arguments, i.e., arguments that must appear in a specific order.
 *
 * @return true if succeeded
 * @return false if failed
 */
bool ArgumentParser::parsePositionalArguments(TokensView tokens) {
    auto currToken = tokens.begin();
    auto currArg = _pimpl->arguments.begin();

    size_t i = 0;

    auto nextToken = [&currToken, &i, tokens]() {
        while (currToken != tokens.end() && currToken->parsed == true) {
            currToken++;
            i++;
        }
    };

    auto nextArg = [&currArg, this]() {
        while (currArg != _pimpl->arguments.end() && (currArg->second.isParsed() || hasOptionPrefix(currArg->first))) {
            currArg++;
        }
    };

    nextToken();
    nextArg();

    for (; currToken != tokens.end() && currArg != _pimpl->arguments.end(); nextToken(), nextArg()) {
        auto& [token, parsed] = *currToken;
        auto& [name, arg] = *currArg;

        assert(!hasOptionPrefix(name));
        assert(!arg.hasAction());

        if (!arg.parse(tokens.subspan(i, 1))) {
            cerr << "Error: invalid " << arg.getTypeString() << " value \""
                 << token << "\" for argument \"" << name << "\"!!" << endl;
            return false;
        }
        // check if meet constraints
        for (auto& [constraint, onerror] : arg.getConstraints()) {
            if (!constraint()) {
                onerror();
                return false;
            }
        }

        parsed = true;
    }

    if (!allTokensAreParsed(tokens)) {
        cerr << "Error: unrecognized argument \"" << currToken->token << "\"!!" << endl;
        return false;
    }
    if (!allRequiredArgumentsAreParsed()) {
        printRequiredArgumentsMissingErrorMsg();
        return false;
    }

    return true;
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

}  // namespace ArgParse