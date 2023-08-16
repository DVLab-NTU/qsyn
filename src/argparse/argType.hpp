/****************************************************************************
  FileName     [ argType.hpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <cassert>
#include <climits>
#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>

#include "./argDef.hpp"

namespace ArgParse {

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

struct NArgsRange {
    size_t lower;
    size_t upper;
};

enum class NArgsOption {
    OPTIONAL,
    ONE_OR_MORE,
    ZERO_OR_MORE
};

using ActionCallbackType = std::function<bool(TokensView)>;  // perform an action and return if it succeeds

template <typename T>
requires ValidArgumentType<T>
class ArgType {
public:
    using ActionType = std::function<ActionCallbackType(ArgType<T>&)>;
    using ConditionType = std::function<bool(T const&)>;
    using ErrorType = std::function<void(T const&)>;
    using ConstraintType = std::pair<ConditionType, ErrorType>;

    ArgType(std::string name, T val)
        : _values{std::move(val)}, _name{std::move(name)} {}

    friend std::ostream& operator<<(std::ostream& os, ArgType const& arg) { return os << fmt::format("{}", arg); }

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
    friend class MutuallyExclusiveGroup;
    friend struct fmt::formatter<ArgType<T>>;
    std::vector<T> _values;
    std::optional<T> _defaultValue = std::nullopt;

    std::string _name;
    std::string _help = "";
    std::string _metavar = "";
    ActionCallbackType _actionCallback = nullptr;
    std::vector<ConstraintType> _constraints = {};
    NArgsRange _nargs = {1, 1};
    size_t _numRequiredChars = 1;

    bool _required : 1 = false;
    bool _append : 1 = false;
    bool _parsed : 1 = false;
};

// SECTION - On-parse actions for ArgType<T>

template <typename T>
requires ValidArgumentType<T>
ActionCallbackType store(ArgType<T>& arg);

template <typename T>
requires ValidArgumentType<T>
typename ArgType<T>::ActionType storeConst(T const& constValue);
ActionCallbackType storeTrue(ArgType<bool>& arg);
ActionCallbackType storeFalse(ArgType<bool>& arg);

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

}  // namespace ArgParse

namespace fmt {

template <typename T>
requires ArgParse::ValidArgumentType<T>
struct formatter<ArgParse::ArgType<T>> {
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator { return ctx.begin(); }

    template <typename FormatContext>
    auto format(ArgParse::ArgType<T> const& arg, FormatContext& ctx) -> format_context::iterator {
        if (arg._values.empty()) return format_to(ctx.out(), "(None)");

        if constexpr (ArgParse::detail::Printable<T>) {
            //                                                          vvv force the typing when T = bool (std::vector<bool> is weird)
            return (arg._nargs.upper <= 1) ? format_to(ctx.out(), "{}", static_cast<T>(arg._values.front()))
                                           : format_to(ctx.out(), "[{}]", fmt::join(arg._values, ", "));
        } else {
            return (arg._nargs.upper <= 1) ? format_to(ctx.out(), "(not representable)")
                                           : format_to(ctx.out(), "[(not representable)]");
        }
    }
};

}  // namespace fmt

namespace ArgParse {

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
    _parsed = false;
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
