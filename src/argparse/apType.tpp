/****************************************************************************
  FileName     [ apArgument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef AP_TYPE_H
#define AP_TYPE_H

#include "argparse.h"

namespace ArgParse {
// SECTION - ArgType<T> template functions

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
 * @brief set the const value to store when the argument is parsed. This setting is
 *        only effective when the action is set to `ArgParse::storeConst<T>`
 *
 * @tparam T
 * @param val
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::constValue(T const& val) {
    _constValue = val;
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
 * @param constraintGen takes a `ArgType<T> const&` and returns a ActionCallbackType
 * @param onerrorGen takes a `ArgType<T> const&` and returns a ErrorCallbackType
 */
template <typename T>
ArgType<T>& ArgType<T>::constraint(ArgType<T>::ActionType const& constraint, ArgType<T>::ErrorType const& onerror) {
    if (constraint == nullptr) {
        std::cerr << "[ArgParse] Failed to add constraint to argument \"" << _name
                  << "\": constraint generator cannot be nullptr!!" << std::endl;
        return *this;
    }
    ActionCallbackType constraintCallback = constraint(*this);
    if (constraintCallback == nullptr) {
        std::cerr << "[ArgParse] Failed to add constraint to argument \"" << _name
                  << "\": constraint generator does not produce valid callback!!" << std::endl;
        return *this;
    }
    ErrorCallbackType onerrorCallback;
    if (onerror == nullptr) {
        onerrorCallback = [this]() {
            std::cerr << "Error: invalid value \"" << getValue() << "\" for argument \""
                      << _name << "\": fail to satisfy constraint(s)!!" << std::endl;
        };
    } else {
        onerrorCallback = onerror(*this);
    }

    if (onerrorCallback == nullptr) {
        std::cerr << "[ArgParse] Failed to add constraint to argument \"" << _name
                  << "\": error callback generator does not produce valid callback!!" << std::endl;
        return *this;
    }

    _constraintCallbacks.emplace_back(constraintCallback, onerrorCallback);
    return *this;
}

template <typename T>
ArgType<T>& ArgType<T>::choices(std::initializer_list<T> const& choices) {
    std::vector<T> vec{choices};
    auto constraint = [&vec](ArgType<T> const& arg) -> ActionCallbackType {
        return [&arg, vec]() {
            return any_of(vec.begin(), vec.end(), [&arg](T const& choice) -> bool {
                // NOTE - only comparing the first argument in ArgType<T>
                //      - might need to revisit later
                return arg.getValue() == choice;
            });
        };
    };
    auto error = [&vec](ArgType<T> const& arg) -> ErrorCallbackType {  // REVIEW - iostream in header... is there any way to resolve this?
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

/**
 * @brief If the argument has a default value, reset to it.
 *
 */
template <typename T>
void ArgType<T>::reset() {
    _value.clear();
    if (_defaultValue.has_value()) _value.push_back(_defaultValue.value());
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
    if (_actionCallback != nullptr) return _actionCallback();
    T tmp;
    bool result = parseFromString(tmp, token);
    if (result) _value.push_back(tmp);
    return result;
}
}  // namespace ArgParse

#endif