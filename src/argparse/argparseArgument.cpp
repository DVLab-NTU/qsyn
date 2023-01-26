/****************************************************************************
  FileName     [ argparseArgument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define class ArgParse::Argument member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparseArgument.h"

#include <iomanip>
#include <iostream>

#include "textFormat.h"

extern size_t colorLevel;

namespace TF = TextFormat;

using namespace std;

namespace ArgParse {

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

}  // namespace detail

//---------------------------------------
// class Argument operator
//---------------------------------------

Argument& Argument::operator=(Argument const& other) {
    other._pimpl->clone().swap(_pimpl);
    _name = other._name;
    return *this;
}

ostream& operator<<(ostream& os, Argument const& arg) {
    return arg._pimpl->doPrint(os);
}

std::string Argument::getSyntaxString() const {
    if (isMandatory())
        return typeBracket(formattedType() + " " + formattedName());
    else
        return optionBracket(formattedName() + (isFlag()
                                                    ? ""
                                                    : (" " + typeBracket(formattedType()))));
    // return optionBracket(formattedName() + (isFlag() ? (" " + typeBracket(formattedType())) : ""));
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
         << left << (isFlag() ? typeFormat("flag") : formattedType()) << " "
         << setw(nameWidth + additionalNameWidth)
         << left << formattedName() << "   ";

    size_t typeStringOccupiedSpace = max(typeWidth, typeStr.size());
    if (typeStringOccupiedSpace + name.size() >= typeWidth + nameWidth + 1) {
        cout << "\n" << string(typeWidth + nameWidth + 4 + nIndents, ' ');
    }
    cout << _helpMessage;
    if (isOptional()) {
        cout << " (default = " << getDefaultValue().value() << ")";
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
    if (isMandatory()) return mandatoryFormat(_name);
    if (colorLevel >= 1) {
        string mand = _name.substr(0, _numMandatoryChars);
        string rest = _name.substr(_numMandatoryChars);
        return optionFormat(accentFormat(mand)) + optionFormat(rest);
    } else {
        string tmp = _name;
        for (size_t i = 0; i < tmp.size(); ++i) {
            tmp[i] = (i < _numMandatoryChars) ? ::toupper(tmp[i]) : ::tolower(tmp[i]);
        }
        return optionFormat(tmp);
    }
}

template <>
std::ostream& Argument::ArgumentModel<bool>::doPrint(std::ostream& os) const {
    return os << std::boolalpha << _arg;
}

}  // namespace ArgParse
