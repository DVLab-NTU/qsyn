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
constexpr auto positionalFormat = TF::BOLD;
constexpr auto typeFormat = [](string const& str) { return TF::CYAN(TF::ITALIC(str)); };
constexpr auto accentFormat = [](string const& str) { return TF::BOLD(TF::ULINE(str)); };

//---------------------------------------
// class Argument public functions
//---------------------------------------

std::ostream& operator<<(std::ostream& os, Argument const& arg) {
    return arg._pimpl->doPrint(os);
}

/**
 * @brief set name to an argument. 
 *        This function returns the reference to the argument to ease decorator chaining
 * 
 * @tparam T 
 * @param val 
 * @return Argument& 
 */
Argument& Argument::name(std::string const& name) {
    setName(name);
    return *this;
}

/**
 * @brief set meta-variable, i.e., displayed name, to an argument. 
 *        This function returns the reference to the argument to ease decorator chaining
 * 
 * @tparam T 
 * @param val 
 * @return Argument& 
 */
Argument& Argument::metavar(std::string const& mvar) {
    setMetavar(mvar);
    return *this;
}

/**
 * @brief set an argument as required. 
 *        This function returns the reference to the argument to ease decorator chaining
 * 
 * @tparam T 
 * @param val 
 * @return Argument& 
 */
Argument& Argument::required() {
    setRequired(true);
    return *this;
}

/**
 * @brief set an argument as optional. 
 *        This function returns the reference to the argument to ease decorator chaining
 * 
 * @tparam T 
 * @param val 
 * @return Argument& 
 */
Argument& Argument::optional() {
    setRequired(false);
    return *this;
}

/**
 * @brief set help message to an argument. 
 *        This function returns the reference to the argument to ease decorator chaining
 * 
 * @tparam T 
 * @param val 
 * @return Argument& 
 */
Argument& Argument::help(std::string const& help) {
    setHelp(help);
    return *this;
}

/**
 * @brief set the action on parsing for an argument. 
 *        If not set, the default behavior is to parse the argument from string.
 *        This function returns the reference to the argument to ease decorator chaining
 * 
 * @tparam T 
 * @param val 
 * @return Argument& 
 */
Argument& Argument::action(ActionType const& action) {
    setAction(action);
    return *this;
}

/**
 * @brief set constraint to an argument. 
 *        This function returns the reference to the argument to ease decorator chaining
 * 
 * @tparam T 
 * @param val 
 * @return Argument& 
 */
Argument& Argument::constraint(ActionType const& constraint, OnErrorCallbackType const& onerror) {
    addConstraint(constraint, onerror);
    return *this;
}

bool Argument::parse(std::span<TokenPair> tokens) {
        bool result = hasAction() ? getAction()(*this) : _pimpl->doParse(tokens);

        setParsed(true);
        
        if (!hasAction()) {
            for (auto const& [callback, onerror] : getConstraintCallbacks()) {
                if (!callback) continue;

                if (!callback(*this)) {
                    if (onerror) onerror(*this);
                    return false;
                }
            }
        }

        return result;
    }

//---------------------------------------
// class Argument pretty-printing helpers
//---------------------------------------

std::string Argument::getSyntaxString() const {
    string ret; 

    if (!hasAction()) {
        ret += formattedType() + " " + formattedMetaVar();
    }
    
    if (isNonPositional()) {
        ret = formattedName() + " " + typeBracket(ret);
    }

    auto bracketFormat = isPositional() ? TF::CYAN : optionFormat;

    if (isRequired())
        return bracketFormat("<") + ret + bracketFormat(">");
    else
        return bracketFormat("[") + ret + bracketFormat("]");
}

void Argument::printHelpString() const {
    constexpr size_t typeWidth = 7;
    constexpr size_t nameWidth = 10;
    constexpr size_t nIndents = 2;
    string typeStr = getTypeString();
    string name = getName();

    size_t additionalNameWidth = isPositional()
                                     ? TF::tokenSize(positionalFormat)
                                     : (TF::tokenSize(accentFormat) + (colorLevel >= 1 ? 2 : 1) * TF::tokenSize(optionFormat));
    cout << string(nIndents, ' ');
    cout << setw(typeWidth + TF::tokenSize(typeFormat))
         << left << formattedType() << " "
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
    cout << "  " << left << setw(8) << getName() << "   = ";
    if (isParsed()) {
        cout << *this;
    } else if (hasDefaultValue()) {
        cout << *this << " (default)";
    } else {
        cout << "(unparsed)";
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
    if (isPositional()) return positionalFormat(getName());
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

string Argument::formattedMetaVar() const {
    return positionalFormat(getMetaVar());
}

}  // namespace ArgParse
