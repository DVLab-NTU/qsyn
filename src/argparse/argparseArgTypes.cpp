/****************************************************************************
  FileName     [ argparseArgument.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define per-type details for types that represents an ArgParse::Argument ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparseArgTypes.h"

#include <cassert>

#include "util.h"

using namespace std;

namespace ArgParse {

namespace detail {

/**
 * @brief Get the type string of the `int` argument.
 *        This function is a implementation to the type-erased
 *        interface `std::string getTypeString(ArgParse::Argument const& arg)`
 *
 * @param arg Argument
 * @return std::string the type string
 */
string getTypeString(int const& arg) {
    return "int";
}

/**
 * @brief Parse the tokens and to a `int` argument.
 *
 * @param arg Argument
 * @param tokens the tokens
 * @return ParseResult
 */
ParseResult parse(int& arg, std::span<TokenPair> tokens) {
    assert(!tokens.empty());
    int tmp;
    if (myStr2Int(tokens[0].first, tmp)) {
        arg = tmp;
    } else {
        return errorOption(ParseErrorType::illegal_arg, tokens[0].first);
    }

    tokens[0].second = true;

    return ParseResult::success;
}

/**
 * @brief Get the type string of the `unsigned` argument.
 *        This function is a implementation to the type-erased
 *        interface `std::string getTypeString(ArgParse::Argument const& arg)`
 *
 * @param arg Argument
 * @return std::string the type string
 */
string getTypeString(unsigned const& arg) {
    return "unsigned";
}

/**
 * @brief Parse the tokens and to a `unsigned` argument.
 *
 * @param arg Argument
 * @param tokens the tokens
 * @return ParseResult
 */
ParseResult parse(unsigned& arg, std::span<TokenPair> tokens) {
    assert(!tokens.empty());
    unsigned tmp;
    if (myStr2Uns(tokens[0].first, tmp)) {
        arg = tmp;
    } else {
        return errorOption(ParseErrorType::illegal_arg, tokens[0].first);
    }

    tokens[0].second = true;

    return ParseResult::success;
}

/**
 * @brief Get the type string of the `size_t` argument.
 *        This function is a implementation to the type-erased
 *        interface `std::string getTypeString(ArgParse::Argument const& arg)`
 *
 * @param arg Argument
 * @return std::string the type string
 */
string getTypeString(size_t const& arg) {
    return "size_t";
}

/**
 * @brief Parse the tokens and to a `size_t` argument.
 *
 * @param arg Argument
 * @param tokens the tokens
 * @return ParseResult
 */
ParseResult parse(size_t& arg, std::span<TokenPair> tokens) {
    assert(!tokens.empty());
    unsigned tmp;
    if (myStr2Uns(tokens[0].first, tmp)) {
        arg = (size_t) tmp;
    } else {
        return errorOption(ParseErrorType::illegal_arg, tokens[0].first);
    }

    tokens[0].second = true;

    return ParseResult::success;
}

/**
 * @brief Get the type string of the `std::string` argument.
 *        This function is a implementation to the type-erased
 *        interface `std::string getTypeString(ArgParse::Argument const& arg)`
 *
 * @param arg Argument
 * @return std::string the type string
 */
string getTypeString(string const& arg) {
    return "string";
}

/**
 * @brief Parse the tokens and to a `string` argument.
 *
 * @param arg Argument
 * @param tokens the tokens
 * @return ParseResult
 */
ParseResult parse(string& arg, std::span<TokenPair> tokens) {
    assert(!tokens.empty());

    arg = tokens[0].first;
    tokens[0].second = true;

    return ParseResult::success;
}

/**
 * @brief Get the type string of the `bool` argument.
 *        This function is a implementation to the type-erased
 *        interface `std::string getTypeString(ArgParse::Argument const& arg)`
 *
 * @param arg Argument
 * @return std::string the type string
 */
string getTypeString(bool const& arg) {
    return "bool";
}

/**
 * @brief Parse the tokens and to a `bool` argument.
 *
 * @param arg Argument
 * @param tokens the tokens
 * @return ParseResult
 */
ParseResult parse(bool& arg, std::span<TokenPair> tokens) {
    assert(!tokens.empty());

    if (myStrNCmp("true", tokens[0].first, 1) == 0) {
        arg = true;
    } else if (myStrNCmp("false", tokens[0].first, 1) == 0) {
        arg = false;
    } else {
        return errorOption(ParseErrorType::illegal_arg, tokens[0].first);
    }

    tokens[0].second = true;

    return ParseResult::success;
}

/**
 * @brief Get the type string of the `SubParsers` argument.
 *        This function is a implementation to the type-erased
 *        interface `std::string getTypeString(ArgParse::Argument const& arg)`
 *
 * @param arg Argument
 * @return std::string the type string
 */
string getTypeString(SubParsers const& arg) {
    return "subparser";
}

/**
 * @brief Parse the tokens and to a `bool` argument.
 *
 * @param arg Argument
 * @param tokens the tokens
 * @return ParseResult
 */
ParseResult parse(SubParsers& arg, std::span<TokenPair> tokens) {
    assert(!tokens.empty());
    // TODO - correct parsing logic for subargs!
    return ParseResult::success;
}

}  // namespace detail

} // namespace argparse