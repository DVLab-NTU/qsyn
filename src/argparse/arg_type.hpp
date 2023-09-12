/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define argparse arg types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <cassert>
#include <climits>
#include <functional>
#include <optional>
#include <span>
#include <string>

#include "./arg_def.hpp"

namespace argparse {

namespace detail {
template <class A>
struct is_fixed_array : std::false_type {};  // NOLINT(readability-identifier-naming) : type trait naming convention

// only works with arrays by specialization.
template <class T, std::size_t I>
struct is_fixed_array<std::array<T, I>> : std::true_type {};
}  // namespace detail

template <typename T>
concept is_container_type = requires(T t) {
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
    optional,
    one_or_more,
    zero_or_more
};

using ActionCallbackType = std::function<bool(TokensView)>;  // perform an action and return if it succeeds

template <typename T>
requires valid_argument_type<T>
class ArgType {
public:
    using ActionType = std::function<ActionCallbackType(ArgType<T>&)>;
    using ConstraintType = std::function<bool(T const&)>;

    ArgType(std::string name, T val)
        : _values{std::move(val)}, _name{std::move(name)} {}

    ArgType& usage(std::string const& usage);
    ArgType& help(std::string const& help);
    ArgType& required(bool is_required);
    ArgType& default_value(T const& val);
    ArgType& action(ActionType const& action);
    ArgType& metavar(std::string const& metavar);
    ArgType& constraint(ConstraintType const& constraint);
    ArgType& choices(std::vector<T> const& choices);
    ArgType& nargs(size_t n);
    ArgType& nargs(size_t l, size_t u);
    ArgType& nargs(NArgsOption opt);

    inline bool take_action(TokensView tokens);
    inline void reset();

    template <typename Ret>
    Ret get() const {
        if constexpr (is_container_type<Ret>) {
            return Ret{_values.begin(), _values.end()};
        } else {
            return _values.front();
        }
    }

    inline std::string const& get_name() const { return _name; }
    void append_value(T const& val) { _values.push_back(val); }

    void set_value_to_default() {
        if (_default_value.has_value()) {
            _values.clear();
            _values.emplace_back(_default_value.value());
        }
    }

    bool is_constraints_satisfied() const {
        return std::ranges::all_of(_constraints, [this](ConstraintType const& condition) -> bool {
            return std::ranges::all_of(_values, [&condition](T const& val) -> bool {
                return condition(val);
            });
        });
    }

    void mark_as_help_action() { _is_help_action = true; }
    void mark_as_version_action() { _is_version_action = true; }

private:
    friend class Argument;
    friend class MutuallyExclusiveGroup;
    friend struct fmt::formatter<ArgType<T>>;
    std::vector<T> _values;
    std::optional<T> _default_value = std::nullopt;

    std::string _name;
    std::string _help = "";
    std::string _metavar = "";
    std::optional<std::string> _usage = std::nullopt;
    ActionCallbackType _action_callback = nullptr;
    std::vector<ConstraintType> _constraints = {};
    NArgsRange _nargs = {1, 1};

    bool _required : 1 = false;
    bool _append : 1 = false;
    bool _parsed : 1 = false;
    bool _is_help_action : 1 = false;
    bool _is_version_action : 1 = false;
};

// SECTION - On-parse actions for ArgType<T>

template <typename T>
requires valid_argument_type<T>
ActionCallbackType store(ArgType<T>& arg);

template <typename T>
requires valid_argument_type<T>
typename ArgType<T>::ActionType store_const(T const& const_value);
ActionCallbackType store_true(ArgType<bool>& arg);
ActionCallbackType store_false(ArgType<bool>& arg);
ActionCallbackType help(ArgType<bool>& arg);
ActionCallbackType version(ArgType<bool>& arg);

ArgType<std::string>::ConstraintType choices_allow_prefix(std::vector<std::string> choices);
bool path_readable(std::string const& filepath);
bool path_writable(std::string const& filepath);
ArgType<std::string>::ConstraintType starts_with(std::vector<std::string> const& prefixes);
ArgType<std::string>::ConstraintType ends_with(std::vector<std::string> const& suffixes);
ArgType<std::string>::ConstraintType allowed_extension(std::vector<std::string> const& extensions);

}  // namespace argparse

namespace fmt {

template <typename T>
requires argparse::valid_argument_type<T>
struct formatter<argparse::ArgType<T>> {
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator { return ctx.begin(); }

    template <typename FormatContext>
    auto format(argparse::ArgType<T> const& arg, FormatContext& ctx) -> format_context::iterator {
        if (arg._values.empty()) return fmt::format_to(ctx.out(), "(None)");

        if constexpr (fmt::is_formattable<T, char>::value) {
            //                                                          vvv force the typing when T = bool (std::vector<bool> is weird)
            return (arg._nargs.upper <= 1) ? fmt::format_to(ctx.out(), "{}", static_cast<T>(arg._values.front()))
                                           : fmt::format_to(ctx.out(), "[{}]", fmt::join(arg._values, ", "));
        } else {
            return (arg._nargs.upper <= 1) ? fmt::format_to(ctx.out(), "(not representable)")
                                           : fmt::format_to(ctx.out(), "[(not representable)]");
        }
    }
};

}  // namespace fmt

namespace argparse {

/**
 * @brief set the usage message of the argument when printing usage of command
 *
 * @tparam T
 * @param help
 * @return ArgType<T>&
 */
template <typename T>
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::usage(std::string const& usage) {
    _usage = usage;
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
requires valid_argument_type<T>
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
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::required(bool is_required) {
    _required = is_required;
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
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::default_value(T const& val) {
    _default_value = val;
    _required = false;
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
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::action(ArgType<T>::ActionType const& action) {
    _action_callback = action(*this);
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
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::metavar(std::string const& metavar) {
    _metavar = metavar;
    return *this;
}

/**
 * @brief Add constraint to the argument.
 *
 * @tparam T
 * @param condition takes a `ArgType<T> const&` and returns a ActionCallbackType
 * @param onerror takes a `ArgType<T> const&` and returns a ErrorCallbackType
 */
template <typename T>
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::constraint(ArgType<T>::ConstraintType const& constraint) {
    if (constraint == nullptr) {
        fmt::println(stderr, "[ArgParse] Error: failed to add constraint to argument \"{}\": condition cannot be `nullptr`!!", _name);
        exit(1);
    }

    _constraints.emplace_back(constraint);
    return *this;
}

template <typename T>
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::choices(std::vector<T> const& choices) {
    auto constraint = [choices, this](T const& val) -> bool {
        if (any_of(choices.begin(), choices.end(), [&val](T const& choice) -> bool {
                return val == choice;
            })) return true;
        fmt::println(stderr, "Error: invalid choice for argument \"{}\": please choose from {{{}}}!!",
                     this->_name, fmt::join(choices, ", "));
        return false;
    };

    return this->constraint(constraint);
}

template <typename T>
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::nargs(size_t n) {
    return nargs(n, n);
}

template <typename T>
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::nargs(size_t l, size_t u) {
    _nargs = {l, u};
    return *this;
}

template <typename T>
requires valid_argument_type<T>
ArgType<T>& ArgType<T>::nargs(NArgsOption opt) {
    using enum NArgsOption;
    switch (opt) {
        case optional:
            return nargs(0, 1);
        case one_or_more:
            return nargs(1, SIZE_MAX);
        case zero_or_more:
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
requires valid_argument_type<T>
void ArgType<T>::reset() {
    _parsed = false;
    _values.clear();
    if (_action_callback == nullptr) {
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
requires valid_argument_type<T>
bool ArgType<T>::take_action(TokensView tokens) {
    assert(_action_callback != nullptr);

    return _action_callback(tokens);
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
requires valid_argument_type<T>
ActionCallbackType store(ArgType<T>& arg) {
    return [&arg](TokensView tokens) -> bool {
        T tmp;
        for (auto& [token, parsed] : tokens) {
            if (!parse_from_string(tmp, token)) {
                fmt::println(stderr, "Error: invalid {} value \"{}\" for argument \"{}\"!!",
                             type_string(tmp), token, arg.get_name());
                return false;
            }
            arg.append_value(tmp);
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
requires valid_argument_type<T>
typename ArgType<T>::ActionType store_const(T const& const_value) {
    return [const_value](ArgType<T>& arg) -> ActionCallbackType {
        arg.nargs(0ul);
        return [&arg, const_value](TokensView) { arg.append_value(const_value); return true; };
    };
}

}  // namespace argparse
