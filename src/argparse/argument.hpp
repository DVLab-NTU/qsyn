/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define type-erased argument class to hold ArgType<T> ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <memory>

#include "arg_type.hpp"

namespace dvlab::argparse {

class Argument {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
    friend struct fmt::formatter<Argument>;

    template <typename T>
    friend ArgType<T>& get_underlying_type(Argument& arg);

public:
    Argument()
        : _pimpl{std::make_unique<Model<ArgType<DummyArgType>>>(ArgType<DummyArgType>{"dummy", DummyArgType{}})} {}

    template <typename T>
    Argument(std::string_view name, T val)
        : _pimpl{std::make_unique<Model<ArgType<T>>>(ArgType<T>{std::string{name}, std::move(val)})} {}

    ~Argument() = default;
    Argument(Argument const& other) : _pimpl(other._pimpl->clone()), _is_option(other._is_option) {}

    Argument& operator=(Argument copy) noexcept {
        copy.swap(*this);
        return *this;
    }

    Argument(Argument&& other) noexcept = default;

    void swap(Argument& rhs) noexcept {
        std::swap(_pimpl, rhs._pimpl);
    }

    friend void swap(Argument& lhs, Argument& rhs) noexcept {
        lhs.swap(rhs);
    }

    // getters
    std::string get_type_string() const { return _pimpl->do_get_type_string(); }
    std::string const& get_name() const { return _pimpl->do_get_name(); }
    std::optional<std::string> const& get_usage() const { return _pimpl->do_get_usage(); }
    std::string const& get_help() const { return _pimpl->do_get_help(); }
    std::string const& get_metavar() const { return _pimpl->do_get_metavar(); }
    NArgsRange const& get_nargs() const { return _pimpl->do_get_nargs(); }
    std::string to_string() const { return _pimpl->do_to_string(); }

    // attributes
    bool has_default_value() const { return _pimpl->do_has_default_value(); }
    bool is_required() const { return _pimpl->do_is_required() && (_is_option || get_nargs().lower > 0); }
    bool is_option() const { return _is_option; }
    bool is_help_action() const { return _pimpl->do_is_help_action(); }
    bool is_version_action() const { return _pimpl->do_is_version_action(); }
    bool is_constraints_satisfied() const { return _pimpl->do_is_constraints_satisfied(); }
    bool is_parsed() const { return _pimpl->do_is_parsed(); }
    TokensSpan get_parse_range(TokensSpan tokens) const;
    bool tokens_enough_to_parse(TokensSpan tokens) const;

    template <typename T>
    T get() const;

    template <typename T>
    std::optional<T> get_if() const;

    // setters
    void set_value_to_default() { _pimpl->do_set_value_to_default(); }
    void set_is_option(bool is_option) { _is_option = is_option; }

    // print functions
    void print_status() const;

    // action
    void reset();
    bool take_action(TokensSpan tokens);
    void mark_as_parsed() { _pimpl->do_mark_as_parsed(); }

private:
    struct Concept {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : pure-virtual interface
        virtual ~Concept() {}

        virtual std::unique_ptr<Concept> clone() const = 0;

        virtual std::string do_get_type_string() const                 = 0;
        virtual std::string const& do_get_name() const                 = 0;
        virtual std::optional<std::string> const& do_get_usage() const = 0;
        virtual std::string const& do_get_help() const                 = 0;
        virtual std::string const& do_get_metavar() const              = 0;
        virtual NArgsRange const& do_get_nargs() const                 = 0;
        virtual bool do_is_parsed() const                              = 0;
        virtual void do_mark_as_parsed()                               = 0;

        virtual bool do_has_default_value() const        = 0;
        virtual bool do_is_required() const              = 0;
        virtual bool do_is_help_action() const           = 0;
        virtual bool do_is_version_action() const        = 0;
        virtual bool do_is_constraints_satisfied() const = 0;

        virtual std::string do_to_string() const = 0;

        virtual bool do_take_action(TokensSpan) = 0;
        virtual void do_set_value_to_default()  = 0;
        virtual void do_reset()                 = 0;
    };

    template <typename ArgT>
    struct Model final : Concept {
        ArgT inner;

        Model(ArgT val)
            : inner(std::move(val)) {}

        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }

        std::string do_get_type_string() const override {
            using V = typename std::remove_cv<typename decltype(inner._values)::value_type>::type;
            return type_string(V{});
        }
        std::string const& do_get_name() const override { return inner._name; }
        std::optional<std::string> const& do_get_usage() const override { return inner._usage; }
        std::string const& do_get_help() const override { return inner._help; }
        std::string const& do_get_metavar() const override { return inner._metavar; }
        NArgsRange const& do_get_nargs() const override { return inner._nargs; }
        bool do_is_parsed() const override { return inner._parsed; }
        void do_mark_as_parsed() override { inner._parsed = true; }

        bool do_has_default_value() const override { return inner._default_value.has_value(); }
        bool do_is_required() const override { return inner._required; }
        bool do_is_help_action() const override { return inner._is_help_action; }
        bool do_is_version_action() const override { return inner._is_version_action; }
        bool do_is_constraints_satisfied() const override { return inner.is_constraints_satisfied(); }

        std::string do_to_string() const override { return fmt::format("{}", inner); }

        bool do_take_action(TokensSpan tokens) override { return inner.take_action(tokens); }
        void do_set_value_to_default() override { return inner.set_value_to_default(); }
        void do_reset() override { inner.reset(); }
    };

    std::unique_ptr<Concept> _pimpl;
    bool _is_option = false;
};

/**
 * @brief Access the data stored in the argument.
 *        This function only works when the target type T is
 *        the same as the stored type; otherwise, this function
 *        throws an error.
 *
 * @tparam T the stored data type
 * @return T const&
 */
template <typename T>
T Argument::get() const {
    if (auto ret = this->get_if<T>()) {
        return *ret;
    }
    fmt::println(stderr, "[ArgParse] Error: cannot cast argument \"{}\" to target type!!", get_name());
    throw std::runtime_error("cannot cast argument to target type");
}

template <typename T>
std::optional<T> Argument::get_if() const {
    if constexpr (is_container_type<T>) {
        using V = typename std::decay_t<typename T::value_type>;
        if (auto ptr = dynamic_cast<Model<ArgType<V>>*>(_pimpl.get())) {
            return ptr->inner.template get<T>();
        }
    } else {
        if (auto ptr = dynamic_cast<Model<ArgType<T>>*>(_pimpl.get())) {
            return ptr->inner.template get<T>();
        }
    }
    return std::nullopt;
}

template <typename T>
ArgType<T>& get_underlying_type(Argument& arg) {
    return dynamic_cast<Argument::Model<ArgType<T>>*>(arg._pimpl.get())->inner;
}

}  // namespace dvlab::argparse

namespace fmt {

template <>
struct formatter<dvlab::argparse::Argument> {
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        return std::begin(ctx);
    }

    template <typename FormatContext>
    auto format(dvlab::argparse::Argument const& arg, FormatContext& ctx) -> format_context::iterator {
        return fmt::format_to(ctx.out(), "{}", arg.to_string());
    }
};

}  // namespace fmt
