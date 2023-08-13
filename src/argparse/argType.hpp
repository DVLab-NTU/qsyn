/****************************************************************************
  FileName     [ argType.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <cassert>
#include <climits>
#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>

#include "util/util.hpp"

namespace ArgParse {

struct Token {
    Token(std::string const& tok)
        : token{tok}, parsed{false} {}
    std::string token;
    bool parsed;
};

using TokensView = std::span<Token>;

using ActionCallbackType = std::function<bool(TokensView)>;  // perform an action and return if it succeeds

struct DummyArgType {};

template <typename T>
std::string typeString(T const&);
template <typename T>
bool parseFromString(T& val, std::string const& token);

template <typename T>
concept ValidArgumentType = requires(T t) {
    { typeString(t) } -> std::same_as<std::string>;
    { parseFromString(t, std::string{}) } -> std::same_as<bool>;
};

template <typename ArithT>
requires std::is_arithmetic_v<ArithT>
bool parseFromString(ArithT& val, std::string const& token) { return myStr2Number<ArithT>(token, val); }

// NOTE - keep in the header to shadow the previous definition (bool is arithmetic type)
template <>
bool parseFromString(bool& val, std::string const& token);

namespace detail {
template <class A>
struct is_fixed_array : std::false_type {};

// only works with arrays by specialization.
template <class T, std::size_t I>
struct is_fixed_array<std::array<T, I>> : std::true_type {};
}  // namespace detail

template <typename T>
concept IsContainerType = requires(T t) {
    { t.begin() } -> std::same_as<typename T::iterator>;
    { t.end() } -> std::same_as<typename T::iterator>;
    std::constructible_from<typename T::iterator, typename T::iterator>;
    { t.size() } -> std::same_as<typename T::size_type>;
    requires !std::same_as<T, std::string>;
    requires !std::same_as<T, std::string_view>;
    requires !detail::is_fixed_array<T>::value;
};

// SECTION - On-parse actions for ArgType<T>

template <typename T>
requires ValidArgumentType<T>
class ArgType;

template <typename T>
requires ValidArgumentType<T>
ActionCallbackType store(ArgType<T>& arg);

template <typename T>
requires ValidArgumentType<T>
typename ArgType<T>::ActionType storeConst(T const& constValue);
ActionCallbackType storeTrue(ArgType<bool>& arg);
ActionCallbackType storeFalse(ArgType<bool>& arg);

struct NArgsRange {
    size_t lower;
    size_t upper;
};

enum class NArgsOption {
    OPTIONAL,
    ONE_OR_MORE,
    ZERO_OR_MORE
};

template <typename T>
requires ValidArgumentType<T>
class ArgType {
public:
    using ActionType = std::function<ActionCallbackType(ArgType<T>&)>;
    using ConditionType = std::function<bool(T const&)>;
    using ErrorType = std::function<void(T const&)>;
    using ConstraintType = std::pair<ConditionType, ErrorType>;

    ArgType(T val)
        : _values{std::move(val)}, _name{}, _help{}, _defaultValue{std::nullopt},
          _actionCallback{}, _metavar{}, _nargs{1, 1},
          _required{false}, _append{false} {}

    std::string toString() const;
    friend std::ostream& operator<<(std::ostream& os, ArgType const& arg) { return os << arg.toString(); }

    ArgType& name(std::string const& name);
    ArgType& help(std::string const& help);
    ArgType& required(bool isReq);
    ArgType& defaultValue(T const& val);
    ArgType& action(ActionType const& action);
    ArgType& metavar(std::string const& metavar);
    ArgType& constraint(ConstraintType const& constraint_error);
    ArgType& constraint(ConditionType const& constraint, ErrorType const& onerror = nullptr);
    ArgType& choices(std::vector<T> const& choices);
    ArgType& nargs(size_t n);
    ArgType& nargs(size_t l, size_t u);
    ArgType& nargs(NArgsOption opt);

    inline bool takeAction(TokensView tokens);
    inline void reset();

    // getters
    // NOTE - only giving the first argument in ArgType<T>
    //      - might need to revisit later
    template <typename Ret>
    Ret get() const {
        if constexpr (IsContainerType<Ret>) {
            return Ret{_values.begin(), _values.end()};
        } else {
            return _values.front();
        }
    }

    inline std::string const& getName() const { return _name; }
    void appendValue(T const& val) { _values.push_back(val); }

    void setValueToDefault() {
        if (_defaultValue.has_value()) {
            _values.clear();
            _values.emplace_back(_defaultValue.value());
        }
    }

    bool constraintsSatisfied() const {
        for (auto& [condition, onerror] : _constraints) {
            for (auto const& val : _values) {
                if (!condition(val)) {
                    onerror(val);
                    return false;
                }
            }
        }
        return true;
    }

private:
    friend class Argument;
    friend class ArgumentGroup;
    std::vector<T> _values;
    std::string _name;
    std::string _help;
    std::optional<T> _defaultValue;
    ActionCallbackType _actionCallback;
    std::string _metavar;
    std::vector<ConstraintType> _constraints;
    NArgsRange _nargs;

    bool _required : 1;
    bool _append : 1;
};

ArgType<std::string>::ConstraintType choices_allow_prefix(std::vector<std::string> choices);
extern ArgType<std::string>::ConstraintType const path_readable;
extern ArgType<std::string>::ConstraintType const path_writable;
ArgType<std::string>::ConstraintType starts_with(std::vector<std::string> const& prefixes);
ArgType<std::string>::ConstraintType ends_with(std::vector<std::string> const& suffixes);
ArgType<std::string>::ConstraintType allowed_extension(std::vector<std::string> const& extensions);

namespace detail {
extern std::ostream& _os;  // placeholder for the concept to work
template <typename T>
concept Printable = requires(T t) {
    { _os << t } -> std::same_as<std::ostream&>;
};

}  // namespace detail

template <typename T>
std::string ArgType<T>::toString() const {
    std::string result;
    if (_values.empty()) return "(None)";

    if constexpr (detail::Printable<T>) {
        return (_nargs.upper <= 1) ? fmt::format("{}", _values.front()) : fmt::format("[{}]", fmt::join(_values, ", "));
    } else {
        return (_nargs.upper <= 1) ? "(not representable)" : "[(not representable)]";
    }
}

/**
 * @brief set the name of the argument*
 * @tparam T
 * @param name
 * @return ArgType<T>&
 */
template <typename T>
requires ValidArgumentType<T>
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
requires ValidArgumentType<T>
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
requires ValidArgumentType<T>
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
requires ValidArgumentType<T>
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
requires ValidArgumentType<T>
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
requires ValidArgumentType<T>
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
requires ValidArgumentType<T>
ArgType<T>& ArgType<T>::constraint(ArgType<T>::ConstraintType const& constraint_error) {
    return this->constraint(constraint_error.first, constraint_error.second);
}

/**
 * @brief Add constraint to the argument.
 *
 * @tparam T
 * @param condition takes a `ArgType<T> const&` and returns a ActionCallbackType
 * @param onerror takes a `ArgType<T> const&` and returns a ErrorCallbackType
 */
template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgType<T>::constraint(ArgType<T>::ConditionType const& condition, ArgType<T>::ErrorType const& onerror) {
    if (condition == nullptr) {
        fmt::println(stderr, "[ArgParse] Failed to add constraint to argument \"{}\": condition cannot be `nullptr`!!", _name);
        return *this;
    }

    _constraints.emplace_back(
        condition,
        (onerror != nullptr) ? onerror : [this](T const& val) -> void {
            fmt::println(stderr, "Error: invalid value \"{}\" for argument \"{}\": failed to satisfy constraint!!", val, _name);
        });
    return *this;
}

template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgType<T>::choices(std::vector<T> const& choices) {
    auto constraint = [choices](T const& val) -> bool {
        return any_of(choices.begin(), choices.end(), [&val](T const& choice) -> bool {
            return val == choice;
        });
    };
    auto error = [choices, this](T const& arg) -> void {
        fmt::println(stderr, "Error: invalid choice for argument \"{}\": please choose from {{{}}}!!",
                     this->_name, fmt::join(choices, ", "));
    };

    return this->constraint(constraint, error);
}

template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgType<T>::nargs(size_t n) {
    return nargs(n, n);
}

template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgType<T>::nargs(size_t l, size_t u) {
    _nargs = {l, u};
    return (l > 0) ? *this : this->required(false);
}

template <typename T>
requires ValidArgumentType<T>
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
requires ValidArgumentType<T>
void ArgType<T>::reset() {
    _values.clear();
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
requires ValidArgumentType<T>
bool ArgType<T>::takeAction(TokensView tokens) {
    assert(_actionCallback != nullptr);

    return _actionCallback(tokens);
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
    return [&arg](TokensView tokens) -> bool {
        T tmp;
        for (auto& [token, parsed] : tokens) {
            if (!parseFromString(tmp, token)) {
                fmt::println(stderr, "Error: invalid {} value \"{}\" for argument \"{}\"!!",
                             typeString(tmp), token, arg.getName());
                return false;
            }
            arg.appendValue(tmp);
            parsed = true;
        }
        return true;
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
typename ArgType<T>::ActionType storeConst(T const& constValue) {
    return [constValue](ArgType<T>& arg) -> ActionCallbackType {
        arg.nargs(0ul);
        return [&arg, constValue](TokensView) { arg.appendValue(constValue); return true; };
    };
}

}  // namespace ArgParse
