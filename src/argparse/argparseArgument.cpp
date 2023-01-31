/****************************************************************************
  FileName     [ argparseArgument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define class ArgParse::Argument member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparseArgument.h"

#include <cassert>
#include <iomanip>
#include <iostream>

#include "textFormat.h"
#include "util.h"

extern size_t colorLevel;

namespace TF = TextFormat;

using namespace std;

namespace ArgParse {

ParseResult errorOption(ParseResult const& result, std::string const& token) {
    assert(result != ParseResult::success);

    switch (result) {
        case ParseResult::illegal_arg:
            cerr << "Error: illegal argument"
                 << (token.size() ? " \"" + token + "\"" : "")
                 << "!!" << endl;
            break;
        case ParseResult::extra_arg:
            cerr << "Error: extra argument"
                 << (token.size() ? " \"" + token + "\"" : "")
                 << "!!" << endl;
            break;
        case ParseResult::missing_arg:
            cerr << "Error: missing argument"
                 << (token.size() ? " after \"" + token + "\"" : "")
                 << "!!" << endl;
            break;
        default:
            break;
    }

    return result;
}

constexpr auto optionFormat = TF::YELLOW;
constexpr auto mandatoryFormat = TF::BOLD;
constexpr auto typeFormat = [](string const& str) { return TF::CYAN(TF::ITALIC(str)); };
constexpr auto accentFormat = [](string const& str) { return TF::BOLD(TF::ULINE(str)); };

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
ParseResult parse(int& arg, std::span<Token> tokens) {
    if (tokens.empty()) return ParseResult::missing_arg;
    int tmp;
    if (myStr2Int(tokens[0].first, tmp)) {
        arg = tmp;
    }
    else {
        return errorOption(ParseResult::illegal_arg, tokens[0].first);
    }
    
    tokens[0].second = true;

    return (tokens.size() == 1) ? ParseResult::success : ParseResult::extra_arg;
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
ParseResult parse(string& arg, std::span<Token> tokens) {
    if (tokens.empty()) return ParseResult::missing_arg;
    
    arg = tokens[0].first;
    tokens[0].second = true;

    return (tokens.size() == 1) ? ParseResult::success : ParseResult::extra_arg;
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
ParseResult parse(bool& arg, std::span<Token> tokens) {
    if (tokens.empty()) return ParseResult::missing_arg;
    if (myStrNCmp("true", tokens[0].first, 1) == 0) {
        arg = true;
    }
    else if (myStrNCmp("false", tokens[0].first, 1) == 0) {
        arg = false;
    }
    else {
        return errorOption(ParseResult::illegal_arg, tokens[0].first);;
    }

    tokens[0].second = true;

    return (tokens.size() == 1) ? ParseResult::success : ParseResult::extra_arg;
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
ParseResult parse(SubParsers& arg, std::span<Token> tokens) {
    //TODO - correct parsing logic for subargs!
    return ParseResult::success;
}

}  // namespace detail

//---------------------------------------
// class Argument operator
//---------------------------------------

template <>
std::ostream& Argument::ArgumentModel<bool>::doPrint(std::ostream& os) const {
    return os << std::boolalpha << _arg;
}

std::string Argument::getSyntaxString() const {
    if (isMandatory())
        return typeBracket(formattedType() + " " + formattedName());
    else
        return optionBracket(formattedName() + (isOfType<bool>() ? "" : (" " + typeBracket(formattedType()))));
}

void Argument::printInfoString() const {
    constexpr size_t typeWidth = 7;
    constexpr size_t nameWidth = 10;
    constexpr size_t nIndents = 2;
    string typeStr = getTypeString();
    string name = getName();

    size_t additionalNameWidth = isOptional()
                                     ? (TF::tokenSize(accentFormat) + (colorLevel >= 1 ? 2 : 1) * TF::tokenSize(optionFormat))
                                     : TF::tokenSize(mandatoryFormat);
    cout << string(nIndents, ' ');
    cout << setw(typeWidth + TF::tokenSize(typeFormat))
         << left << (isOfType<bool>() && isOptional() ? typeFormat("flag") : formattedType()) << " "
         << setw(nameWidth + additionalNameWidth)
         << left << formattedName() << "   ";

    size_t typeStringOccupiedSpace = max(typeWidth, typeStr.size());
    if (typeStringOccupiedSpace + name.size() >= typeWidth + nameWidth + 1) {
        cout << "\n" << string(typeWidth + nameWidth + 4 + nIndents, ' ');
    }
    cout << getHelp();
    if (hasDefaultValue()) {
        cout << " (default = " << *this << ")";
    }
    cout << endl;
}

void Argument::printStatus() const {
    cout << "  " << left << setw(8) << getName() << "  ";
    if (isParsed()) {
        cout << " = " << *this;
    } else if (hasDefaultValue()) {
        cout << " = " << *this << " (default)";
    } else {
        cout << "   (unparsed)";
    }
    cout << endl;
}

string Argument::typeBracket(string const& str) const {
    return typeFormat("<") + str + typeFormat(">");
}

string Argument::optionBracket(string const& str) const {
    return optionFormat("[") + str + optionFormat("]");
}

string Argument::formattedType() const {
    return typeFormat(getTypeString());
}

string Argument::formattedName() const {
    if (isMandatory()) return mandatoryFormat(getName());
    if (colorLevel >= 1) {
        string mand = getName().substr(0, getNumMandatoryChars());
        string rest = getName().substr(getNumMandatoryChars());
        return optionFormat(accentFormat(mand)) + optionFormat(rest);
    } else {
        string tmp = getName();
        for (size_t i = 0; i < tmp.size(); ++i) {
            tmp[i] = (i < getNumMandatoryChars()) ? ::toupper(tmp[i]) : ::tolower(tmp[i]);
        }
        return optionFormat(tmp);
    }
}

}  // namespace ArgParse
