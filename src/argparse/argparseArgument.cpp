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



constexpr auto optionFormat = TF::YELLOW;
constexpr auto mandatoryFormat = TF::BOLD;
constexpr auto typeFormat = [](string const& str) { return TF::CYAN(TF::ITALIC(str)); };
constexpr auto accentFormat = [](string const& str) { return TF::BOLD(TF::ULINE(str)); };

//---------------------------------------
// class Argument operator
//---------------------------------------

std::ostream& operator<<(std::ostream& os, Argument const& arg) {
    return arg.isParsed()
                ? arg._pimpl->doPrint(os)
                : (arg.hasDefaultValue()
                        ? arg._pimpl->doPrint(os) << " (default)"
                        : os << "(unparsed)");
}

template <>
std::ostream& Argument::ArgumentModel<bool>::doPrint(std::ostream& os) const {
    return os << std::boolalpha << _arg;
}

//---------------------------------------
// class Argument pretty-printing helpers
//---------------------------------------

std::string Argument::getSyntaxString() const {
    if (isMandatory())
        return typeBracket(formattedType() + " " + formattedName());
    else
        return optionBracket(formattedName() + (isOfType<bool>() ? "" : (" " + typeBracket(formattedType()))));
}

void Argument::printHelpString() const {
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
    if (typeStringOccupiedSpace + name.size() > typeWidth + nameWidth + 1) {
        cout << "\n"
             << string(typeWidth + nameWidth + 4 + nIndents, ' ');
    }
    cout << getHelp();
    if (hasDefaultValue() && !hasAction()) {
        cout << " (default = ";
        _pimpl->doPrint(cout) << ")";
    }
    cout << endl;
}

void Argument::printStatus() const {
    cout << "  " << left << setw(8) << getName() << "   = " << *this << endl;
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
