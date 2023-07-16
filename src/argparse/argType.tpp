/****************************************************************************
  FileName     [ argType.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef ARGPARSE_ARGTYPE_TPP
#define ARGPARSE_ARGTYPE_TPP

#include <climits>

#include "argparse.h"

namespace ArgParse {

/**
 * @brief print the value of the argument if it is printable; otherwise, shows "(not representable)"
 *
 * @tparam T
 * @param os
 * @param arg
 * @return std::ostream&
 */
template <typename U>
std::ostream& operator<<(std::ostream& os, ArgType<U> const& arg) {
    if (arg._value.empty()) return os << "(None)";

    if constexpr (Printable<U>) {
        if (arg._nargs.upper <= 1)
            os << arg._value.front();
        else {
            size_t i = 0;
            os << '[';
            for (auto&& val : arg._value) {
                if (i > 0) os << ", ";
                os << val;
                ++i;
            }
            os << ']';
        }
    } else {
        if (arg._nargs.upper <= 1)
            return os << "(not representable)";
        else
            os << "[(not representable)]";
    }
    return os;
}

/**
 * @brief set the name of the argument
 *
 * @tparam T
 * @param name
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::name(std::string const& name) {
    _name = name;
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
    _help = help;
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
    _required = isReq;
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
    _defaultValue = val;
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
ArgType<T>& ArgType<T>::action(ArgType<T>::ActionType const& action) {
    _actionCallback = action(*this);
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
    _metavar = metavar;
    return *this;
}

/**
 * @brief Add constraint to the argument.
 *
 * @tparam T
 * @param constraint_error a pair of constraint generator and on-error callback generator.
 *        The constraint generator takes a `ArgType<T> const&` and returns a ActionType,
 *        while takes a `ArgType<T> const&` and returns a ErrorCallbackType.
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::constraint(ArgType<T>::ConstraintType const& constraint_error) {
    return this->constraint(constraint_error.first, constraint_error.second);
}

/**
 * @brief Add constraint to the argument.
 *
 * @tparam T
 * @param constraint takes a `ArgType<T> const&` and returns a ActionCallbackType
 * @param onerror takes a `ArgType<T> const&` and returns a ErrorCallbackType
 */
template <typename T>
ArgType<T>& ArgType<T>::constraint(ArgType<T>::ConditionType const& constraint, ArgType<T>::ErrorType const& onerror) {
    if (constraint == nullptr) {
        std::cerr << "[ArgParse] Failed to add constraint to argument \"" << _name
                  << "\": constraint generator cannot be nullptr!!" << std::endl;
        return *this;
    }
    auto const conditionCallback = constraint(*this);
    auto const onerrorCallback = std::invoke([&onerror, this]() -> ErrorCallbackType {
        if (onerror == nullptr) {
            return [this]() -> void {
                std::cerr << "Error: invalid value \"" << getValue() << "\" for argument \""
                          << _name << "\": fail to satisfy constraint(s)!!" << std::endl;
            };
        } else {
            return onerror(*this);
        }
    });

    if (conditionCallback == nullptr) {
        std::cerr << "[ArgParse] Failed to add constraint to argument \"" << _name
                  << "\": constraint generator does not produce valid callback!!" << std::endl;
        return *this;
    }
    if (onerrorCallback == nullptr) {
        std::cerr << "[ArgParse] Failed to add constraint to argument \"" << _name
                  << "\": error callback generator does not produce valid callback!!" << std::endl;
        return *this;
    }

    _constraintCallbacks.emplace_back(conditionCallback, onerrorCallback);
    return *this;
}

template <typename T>
ArgType<T>& ArgType<T>::choices(std::initializer_list<T> const& choices) {
    std::vector<T> vec{choices};
    auto constraint = [&vec](ArgType<T> const& arg) -> ConditionCallbackType {
        return [&arg, vec]() {
            return any_of(vec.begin(), vec.end(), [&arg](T const& choice) -> bool {
                // NOTE - only comparing the first argument in ArgType<T>
                //      - might need to revisit later
                return arg.getValue() == choice;
            });
        };
    };
    auto error = [&vec](ArgType<T> const& arg) -> ErrorCallbackType {
        return [&arg, vec]() {
            std::cerr << "Error: invalid choice for argument \"" << arg._name << "\": "
                      << "please choose from {";
            size_t ctr = 0;
            for (auto& choice : vec) {
                if (ctr > 0) std::cerr << ", ";
                std::cerr << choice;
                ctr++;
            }
            std::cerr << "}!!\n";
        };
    };

    return this->constraint(constraint, error);
}

template <typename T>
ArgType<T>& ArgType<T>::nargs(size_t n) {
    return nargs(n, n);
}

template <typename T>
ArgType<T>& ArgType<T>::nargs(size_t l, size_t u) {
    _nargs = {l, u};
    return *this;
}

template <typename T>
ArgType<T>& ArgType<T>::nargs(NArgsOption opt) {
    using enum NArgsOption;
    switch (opt) {
        case OPTIONAL:
            return nargs(0, 1);
        case ONE_OR_MORE:
            return nargs(1, SIZE_MAX);
        case ZERO_OR_MORE:
            return nargs(0, SIZE_MAX);
        default:
            return *this;
    }
    return *this;
}

/**
 * @brief If the argument has a default value, reset to it.
 *
 */
template <typename T>
void ArgType<T>::reset() {
    _value.clear();
    if (_actionCallback == nullptr) {
        this->action(store<T>);
    }
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
bool ArgType<T>::takeAction(std::string const& token) {
    assert(_actionCallback != nullptr);
    return _actionCallback(token);
}

/**
 * @brief try to parse the token.
 *        On success, store the parsed result and return true;
 *        otherwise do nothing and return false.
 *
 * @tparam T
 * @param arg
 * @return ArgType<T>::ActionType that store `constValue` upon parsed
 */
template <typename T>
requires ValidArgumentType<T>
ActionCallbackType store(ArgType<T>& arg) {
    return [&arg](std::string const& token) -> bool {
        T tmp;
        bool result = parseFromString(tmp, token);
        if (result) {
            arg.appendValue(tmp);
        }
        return result;
    };
}

/**
 * @brief generate a callback that sets the argument to `constValue`.
 *
 * @tparam T argument type
 * @param constValue
 * @return ArgType<T>::ActionType that store `constValue` upon parsed
 */
template <typename T>
requires ValidArgumentType<T>
ArgType<T>::ActionType storeConst(T const& constValue) {
    return [constValue](ArgType<T>& arg) -> ActionCallbackType {
        arg.nargs(0ul);
        return [&arg, constValue](std::string const&) { arg.appendValue(constValue); return true; };
    };
}

}  // namespace ArgParse

#endif