/****************************************************************************
  FileName     [ apArgType.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define parser argument types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_ARGPARSE_ARGTYPE_H
#define QSYN_ARGPARSE_ARGTYPE_H

#include <functional>
#include <optional>
#include <span>
#include <string>

#include "util.h"

namespace ArgParse {

struct Token {
    Token(std::string const& tok) : token{tok}, parsed{false} {}
    Token(char* const tok) : token{tok}, parsed{false} {}
    template <size_t N>
    Token(char const (&tok)[N]) : token{tok}, parsed{false} {}
    std::string token;
    bool parsed;
};

namespace detail {

std::string getTypeString(bool);
std::string getTypeString(int);
std::string getTypeString(unsigned);
std::string getTypeString(std::string const& val);

bool parseFromString(bool& val, std::string const& token);
bool parseFromString(int& val, std::string const& token);
bool parseFromString(unsigned& val, std::string const& token);
bool parseFromString(std::string& val, std::string const& token);

}  // namespace detail

template <typename T>
class ArgType {
public:
    using ActionType = std::function<bool()>;

    ArgType(T const& val) : _value{val}, _traits{} {}

    friend std::ostream& operator<<(std::ostream& os, ArgType<T> const& arg) {
        return os << arg._value;
    }

    operator T&() { return _value; }
    operator T const&() const { return _value; }

    // argument decorators

    ArgType& name(std::string const& name);
    ArgType& help(std::string const& help);
    ArgType& required(bool isReq);
    ArgType& defaultValue(T const& val);
    ArgType& action(std::function<ActionType(ArgType<T>&)> const& action);
    ArgType& constValue(T const& val);
    ArgType& metavar(std::string const& metavar);

    /**
     * @brief If the argument has a default value, reset to it.
     * 
     */
    void reset();

    /**
     * @brief parse the argument. If the argument has an action, perform it; otherwise,
     *        try to parse the value from token.
     * 
     * @param token 
     * @return true if succeeded
     * @return false if failed
     */
    bool parse(std::string const& token);

    // getters

    std::string getTypeString() const { return detail::getTypeString(_value); }
    std::string const& getName() const { return _traits.name; }
    std::string const& getHelp() const { return _traits.help; }
    std::optional<T> getDefaultValue() const { return _traits.defaultValue; }
    std::string const& getMetaVar() const { return _traits.metavar; }

    // setters

    void setValueToConst() { _value = _traits.constValue; }

    // attributes
    bool hasDefaultValue() const { return _traits.defaultValue.has_value(); }
    bool hasAction() const { return _traits.actionCallback != nullptr; }
    bool isRequired() const { return _traits.required; }

private:
    struct Traits {
        Traits()
            : name{}, help{}, required{false}, defaultValue{std::nullopt}, constValue{}, actionCallback{}, metavar{} {}
        std::string name;
        std::string help;
        bool required;
        std::optional<T> defaultValue;
        T constValue;
        ActionType actionCallback;
        std::string metavar;
    };

    T _value;
    Traits _traits;
};

// --------------------------------------
//   argument decorators
// --------------------------------------

/**
 * @brief set the name of the argument
 * 
 * @tparam T 
 * @param name 
 * @return ArgType<T>& 
 */
template <typename T>
ArgType<T>& ArgType<T>::name(std::string const& name) {
    _traits.name = name;
    return *this;
}

/**
 * @brief set the help message of the argument
 * 
 * @tparam T 
 * @param help 
 * @return ArgType<T>& 
 */
template <typename T>
ArgType<T>& ArgType<T>::help(std::string const& help) {
    _traits.help = help;
    return *this;
}

/**
 * @brief set if the argument is required
 * 
 * @tparam T 
 * @param isReq 
 * @return ArgType<T>& 
 */
template <typename T>
ArgType<T>& ArgType<T>::required(bool isReq) {
    _traits.required = isReq;
    return *this;
}

/**
 * @brief set the default value of the argument
 * 
 * @tparam T 
 * @param val 
 * @return ArgType<T>& 
 */
template <typename T>
ArgType<T>& ArgType<T>::defaultValue(T const& val) {
    _traits.defaultValue = val;
    return *this;
}

/**
 * @brief set the action of the argument when parsed. An action can be 
 *        any callable type that takes an `ArgType<T>&` and returns a
 *        `ArgType<T>::ActionType` (aka `std::function<bool()>`).
 * 
 * @tparam T 
 * @param action 
 * @return ArgType<T>& 
 */
template <typename T>
ArgType<T>& ArgType<T>::action(std::function<ActionType(ArgType<T>&)> const& action) {
    _traits.actionCallback = action(*this);
    return *this;
}

/**
 * @brief set the const value to store when the argument is parsed. This setting is 
 *        only effective when the action is set to `ArgParse::storeConst<T>`
 * 
 * @tparam T 
 * @param val 
 * @return ArgType<T>& 
 */
template <typename T>
ArgType<T>& ArgType<T>::constValue(T const& val) {
    _traits.constValue = val;
    return *this;
}

/**
 * @brief set the meta-variable, i.e., the displayed name of the argument as
 *        seen in the help message.
 * 
 * @tparam T 
 * @param metavar 
 * @return ArgType<T>& 
 */
template <typename T>
ArgType<T>& ArgType<T>::metavar(std::string const& metavar) {
    _traits.metavar = metavar;
    return *this;
}

// --------------------------------------
//   action
// --------------------------------------

/**
 * @brief If the argument has a default value, reset to it.
 * 
 */
template <typename T>
void ArgType<T>::reset() {
    if (hasDefaultValue()) _value = _traits.defaultValue.value();
}

/**
 * @brief parse the argument. If the argument has an action, perform it; otherwise,
 *        try to parse the value from token.
 * 
 * @param token 
 * @return true if succeeded
 * @return false if failed
 */
template <typename T>
bool ArgType<T>::parse(std::string const& token) {
    return (_traits.actionCallback) ? _traits.actionCallback() : detail::parseFromString(_value, token);
}

// --------------------------------------
//   actions
// --------------------------------------

/**
 * @brief generate a callback that sets the argument to const value. This function
 *        should be used along with the const decorator of an argument.
 * 
 * @tparam T 
 * @param arg 
 * @return ArgType<T>::ActionType (aka `std::function<bool()>`)
 */
template <typename T>
ArgType<T>::ActionType storeConst(ArgType<T>& arg) {
    return [&arg]() -> bool {
        arg.setValueToConst();
        return true;
    };
}

ArgType<bool>::ActionType storeTrue(ArgType<bool>& arg);
ArgType<bool>::ActionType storeFalse(ArgType<bool>& arg);

}  // namespace ArgParse

#endif  // QSYN_ARGPARSE_ARGTYPE_H