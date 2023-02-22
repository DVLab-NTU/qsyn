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

    ArgType& name(std::string name) {
        _traits.name = name;
        return *this;
    }

    ArgType& help(std::string help) {
        _traits.help = help;
        return *this;
    }

    ArgType& required(bool isReq) {
        _traits.required = isReq;
        return *this;
    }

    ArgType& defaultValue(T const& val) {
        _traits.defaultValue = val;
        return *this;
    }

    ArgType& action(std::function<ActionType(ArgType<T>&)> const& action) {
        _traits.actionCallback = action(*this);
        return *this;
    }

    ArgType& constValue(T const& val) {
        _traits.constValue = val;
        return *this;
    }

    ArgType& metavar(std::string metavar) {
        _traits.metavar = metavar;
        return *this;
    }

    void reset() {
        if (hasDefaultValue()) _value = _traits.defaultValue.value();
    }

    void setValueToConst() { _value = _traits.constValue; }

    bool parse(std::string const& token) {
        return (_traits.actionCallback) ? _traits.actionCallback() : detail::parseFromString(_value, token);
    }

    // getters/attributes

    std::string getTypeString() const { return detail::getTypeString(_value); }
    std::string const& getName() const { return _traits.name; }
    // std::string const& getMetaVar() const { return _traits.metavar; }
    std::string const& getHelp() const { return _traits.help; }
    std::optional<T> getDefaultValue() const { return _traits.defaultValue; }

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