/****************************************************************************
  FileName     [ apArgParser.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser core functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "apArgParser.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <numeric>

#include "myTrie.h"

using namespace std;

namespace ArgParse {

/**
 * @brief Access the argument with the name
 *
 * @param name
 * @return Argument&
 */
Argument& ArgumentParser::operator[](std::string const& name) {
    try {
        return _arguments.at(toLowerString(name));
    } catch (std::out_of_range& e) {
        std::cerr << "name = " << name << ", " << e.what() << std::endl;
        exit(-1);
    }
}

/**
 * @brief Access the argument with the name
 *
 * @param name
 * @return Argument const&
 */
Argument const& ArgumentParser::operator[](std::string const& name) const {
    try {
        return _arguments.at(toLowerString(name));
    } catch (std::out_of_range& e) {
        std::cerr << "name = " << name << ", " << e.what() << std::endl;
        exit(-1);
    }
}

/**
 * @brief set the command name to the argument parser
 *
 * @param name
 * @return ArgumentParser&
 */
ArgumentParser& ArgumentParser::name(std::string const& name) {
    _name = toLowerString(name);
    _numRequiredChars = countUpperChars(name);
    return *this;
}

/**
 * @brief set the help message to the argument parser
 *
 * @param name
 * @return ArgumentParser&
 */
ArgumentParser& ArgumentParser::help(std::string const& help) {
    _help = help;
    return *this;
}

/**
 * @brief get the size of parsed option
 *
 * @return size_t
 */
size_t ArgumentParser::isParsedSize() const {
    size_t parsed = 0;
    for (auto& [a, b] : _arguments) {
        parsed += b.isParsed();
    }
    return parsed;
}

/**
 * @brief parse the arguments in the line
 *
 * @param line
 * @return true
 * @return false
 */
bool ArgumentParser::parse(std::string const& line) {
    for (auto& [_, arg] : _arguments) {
        arg.reset();
    }

    return tokenize(line) && analyzeOptions() && parseOptions() && parsePositionalArguments();
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
    if (_optionsAnalyzed) return true;

    // calculate the number of required characters to differentiate each option

    _trie.clear();
    _conflictGroups.clear();

    for (auto const& group : _mutuallyExclusiveGroups) {
        for (auto const& name : group.getArguments()) {
            if (_arguments.at(name).isRequired()) {
                cerr << "[ArgParse] Error: Mutually exclusive argument \"" << name << "\" must be optional!!" << endl;
                return false;
            }
            _conflictGroups.emplace(name, group);
        }
    }

    for (auto const& [name, arg] : _arguments) {
        if (!hasOptionPrefix(name)) continue;
        _trie.insert(name);
    }

    for (auto& [name, arg] : _arguments) {
        if (!hasOptionPrefix(name)) continue;
        size_t prefixSize = _trie.shortestUniquePrefix(name).value().size();
        while (!isalpha(name[prefixSize - 1])) ++prefixSize;
        arg.setNumRequiredChars(max(prefixSize, arg.getNumRequiredChars()));
    }

    // calculate tabler info

    vector<size_t> widths = {0, 0, 0, 0};

    for (auto& [name, arg] : _arguments) {
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

    _tabl.presetStyle(dvlab_utils::Tabler::PresetStyle::ASCII_MINIMAL)
        .indent(1)
        .rightMargin(2)
        .widths(widths);

    _optionsAnalyzed = true;
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
    _tokens.clear();
    string buffer, stripped;
    if (!stripQuotes(line, stripped)) {
        cerr << "Error: missing ending quote!!" << endl;
        return false;
    }
    size_t pos = myStrGetTok(stripped, buffer);
    while (buffer.size()) {
        _tokens.emplace_back(buffer);
        pos = myStrGetTok(stripped, buffer, pos);
    }
    if (_tokens.empty()) return true;
    // concat tokens with '\ ' to a single token with space in it
    for (auto itr = next(_tokens.rbegin()); itr != _tokens.rend(); ++itr) {
        string& currToken = itr->token;
        string& nextToken = prev(itr)->token;

        if (currToken.ends_with('\\')) {
            currToken.back() = ' ';
            currToken += nextToken;
            nextToken = "";
        }
    }
    erase_if(_tokens, [](Token const& token) { return token.token == ""; });

    // convert "abc=def", "abc:def" to "abc def"

    size_t nTokens = _tokens.size();
    for (size_t i = 0; i < nTokens; ++i) {
        string& currToken = _tokens[i].token;
        size_t pos = currToken.find_first_of("=:");

        if (pos != string::npos && pos != 0) {
            _tokens.emplace(_tokens.begin() + i + 1, currToken.substr(pos + 1));
            nTokens++;
            currToken = currToken.substr(0, pos);
        }
    }

    return true;
}

/**
 * @brief Parse the optional arguments, i.e., the arguments that starts with
 *        one of the option prefix.
 *
 * @return true if succeeded
 * @return false if failed
 */
bool ArgumentParser::parseOptions() {
    for (int i = _tokens.size() - 1; i >= 0; --i) {
        if (!hasOptionPrefix(_tokens[i].token)) continue;
        auto match = matchOption(_tokens[i].token);
        if (std::holds_alternative<size_t>(match)) {
            auto frequency = std::get<size_t>(match);
            assert(frequency != 1);
            // if the argument is a number, skip to the next arg
            float tmp;
            if (myStr2Float(_tokens[i].token, tmp)) {
                continue;
            }
            // else this is an error
            if (frequency == 0) {
                cerr << "Error: unrecognized option \"" << _tokens[i].token << "\"!!\n";
            } else {
                printAmbiguousOptionErrorMsg(_tokens[i].token);
            }

            return false;
        }

        Argument& arg = _arguments[get<string>(match)];

        if (_conflictGroups.contains(arg.getName())) {
            auto& conflictGroup = _conflictGroups.at(arg.getName());
            if (conflictGroup.isParsed()) {
                for (auto const& conflict : conflictGroup.getArguments()) {
                    if (_arguments.at(conflict).isParsed()) {
                        cerr << "Error: argument \"" << arg.getName() << "\" cannot occur with \"" << conflict << "\"!!\n";
                        return false;
                    }
                }
            }
            conflictGroup.setParsed(true);
        }

        if (arg.hasAction())
            arg.parse("");
        else if (i + 1 >= (int)_tokens.size() || _tokens[i + 1].parsed == true) {  // _tokens[i] is not the last token && _tokens[i+1] is unparsed
            cerr << "Error: missing argument after \"" << _tokens[i].token << "\"!!\n";
            return false;
        } else if (!arg.parse(_tokens[i + 1].token)) {
            cerr << "Error: invalid " << arg.getTypeString() << " value \""
                 << _tokens[i + 1].token << "\" after \""
                 << _tokens[i].token << "\"!!" << endl;
            return false;
        }

        // check if meet constraints
        for (auto& [constraint, onerror] : arg.getConstraints()) {
            if (!constraint()) {
                onerror();
                return false;
            }
        }

        _tokens[i].parsed = true;

        if (!arg.hasAction()) {
            _tokens[i + 1].parsed = true;
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
bool ArgumentParser::parsePositionalArguments() {
    auto currToken = _tokens.begin();
    auto currArg = _arguments.begin();

    auto nextToken = [&currToken, this]() {
        while (currToken != _tokens.end() && currToken->parsed == true) {
            currToken++;
        }
    };

    auto nextArg = [&currArg, this]() {
        while (currArg != _arguments.end() && (currArg->second.isParsed() || hasOptionPrefix(currArg->first))) {
            currArg++;
        }
    };

    nextToken();
    nextArg();

    for (; currToken != _tokens.end() && currArg != _arguments.end(); nextToken(), nextArg()) {
        auto& [token, parsed] = *currToken;
        auto& [name, arg] = *currArg;

        assert(!hasOptionPrefix(name));
        assert(!arg.hasAction());

        if (!arg.parse(token)) {
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

    if (!allTokensAreParsed()) {
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
    auto match = _trie.findWithPrefix(key);
    if (match.has_value()) {
        if (key.size() < _arguments.at(match.value()).getNumRequiredChars()) {
            return 0u;
        }
        return match.value();
    }

    return _trie.frequency(key);
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
    for (auto& [name, _] : _arguments) {
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
    for (auto& [name, arg] : _arguments) {
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
    for (auto& group : _mutuallyExclusiveGroups) {
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
bool ArgumentParser::allTokensAreParsed() const {
    return all_of(_tokens.begin(), _tokens.end(), [](Token const& tok) {
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
    return all_of(_arguments.begin(), _arguments.end(), [](pair<string, Argument> const& argPair) {
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
    for (auto& [name, arg] : _arguments) {
        if (!arg.isParsed() && arg.isRequired()) {
            if (ctr > 0) cerr << ", ";
            cerr << name;
            ctr++;
        }
    }
    cerr << endl;
}

}  // namespace ArgParse